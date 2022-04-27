#include "util/db/json.h"

#include "util/format.h"
#include "util/stringalg.h"

namespace lex_detail {
#include "lex_defs.h"
}

namespace lex_detail {
#include "lex_analyzer.inl"
}

using namespace util;
using namespace util::db;

// --------------------------

json::reader::reader(iobuf& input) : input_(input) {
    state_stack_.reserve(256);
    state_stack_.push_back(lex_detail::sc_initial);
}

int json::reader::read(value& v) {
    if (!input_) { return -1; }
    token_val_t tk_val;
    int tk = parse_token(tk_val);
    return parse_value(v, tk, tk_val);
}

int json::reader::parse_value(value& v, int tk, token_val_t& tk_val) {
    if (tk == '{') {
        if ((tk = parse_object(v, tk_val)) == 0) { return 0; }
    } else if (tk == '[') {
        if ((tk = parse_array(v, tk_val)) == 0) { return 0; }
    } else if (tk >= kNull) {
        v = token_to_value(tk, tk_val);
        return 0;
    }
    return -1;
}

int json::reader::parse_array(value& v, token_val_t& tk_val) {
    int tk = parse_token(tk_val);
    if (tk == ']') {  // create an empty array
        v.convert(value::dtype::kArray);
        return 0;
    }
    while (parse_value(v.emplace_back(), tk, tk_val) == 0) {
        if ((tk = parse_token(tk_val)) == ']') { return 0; }
        if (tk != ',') { break; }
        tk = parse_token(tk_val);
    }
    return -1;
}

int json::reader::parse_object(value& v, token_val_t& tk_val) {
    int tk = parse_token(tk_val);
    if (tk == '}') {  // create an empty object
        v.convert(value::dtype::kRecord);
        return 0;
    }
    while (tk == kString) {
        if ((tk = parse_token(tk_val)) != ':') { return -1; }
        value& field = v[tk_val.str];
        tk = parse_token(tk_val);
        if (parse_value(field, tk, tk_val) != 0) { return -1; }
        if ((tk = parse_token(tk_val)) == '}') { return 0; }
        if (tk != ',') { return -1; }
        tk = parse_token(tk_val);
    }
    return -1;
}

int json::reader::parse_token(token_val_t& tk_val) {
    std::string s;
    while (true) {
        int pat = 0;
        unsigned llen = 0;
        const char* lexeme = first_;
        while (true) {
            pat = lex_detail::lex(first_, buf_.data() + buf_.size(), state_stack_, llen, input_);
            if (pat != lex_detail::err_end_of_input) {
                break;
            } else if (!input_) {
                return kEof;  // end of sequence
            }
            // add more characters
            if (input_.in_avail() == 0) { input_.peek(); }
            auto avail = input_.in_avail_view();
            if (!avail.empty()) {
                buf_.erase(buf_.begin(), buf_.end() - llen);
                buf_.insert(buf_.end(), avail.begin(), avail.end());
                lexeme = first_ = buf_.data();
                input_.skip(avail.size());
            }
        }
        first_ += llen;
        switch (pat) {
            // ------ escape sequences
            case lex_detail::pat_escape_a: s.push_back('\a'); break;
            case lex_detail::pat_escape_b: s.push_back('\b'); break;
            case lex_detail::pat_escape_f: s.push_back('\f'); break;
            case lex_detail::pat_escape_n: s.push_back('\n'); break;
            case lex_detail::pat_escape_r: s.push_back('\r'); break;
            case lex_detail::pat_escape_t: s.push_back('\t'); break;
            case lex_detail::pat_escape_v: s.push_back('\v'); break;
            case lex_detail::pat_escape_other: s.push_back(lexeme[1]); break;
            case lex_detail::pat_escape_unicode: {
                unsigned unicode = (util::dig_v<16>(lexeme[2]) << 12) | (util::dig_v<16>(lexeme[3]) << 8) |
                                   (util::dig_v<16>(lexeme[4]) << 4) | util::dig_v<16>(lexeme[5]);
                to_utf8(unicode, std::back_inserter(s));
            } break;

            // ------ strings
            case lex_detail::pat_string_nl: s.push_back('\n'); break;
            case lex_detail::pat_string: {
                state_stack_.push_back(lex_detail::sc_string);
            } break;
            case lex_detail::pat_string_seq: {
                s.append(lexeme, llen);
            } break;
            case lex_detail::pat_string_close: {
                tk_val.str = std::move(s);
                state_stack_.pop_back();
                return kString;
            } break;

            case lex_detail::pat_null_literal: return kNull;
            case lex_detail::pat_true_literal: return kTrue;
            case lex_detail::pat_false_literal: return kFalse;

            case lex_detail::pat_dec_literal: {
                tk_val.u64 = from_string<uint64_t>(std::string_view(lexeme, llen));
                return lexeme[0] == '-' ? kNegInt : kPosInt;
            } break;

            case lex_detail::pat_real_literal: {
                tk_val.dbl = from_string<double>(std::string_view(lexeme, llen));
                return kDouble;
            } break;

            case lex_detail::pat_whitespace: break;
            case lex_detail::pat_nl: break;
            case lex_detail::pat_comment: {  // skip till end of line or stream
                first_ = std::find(first_, buf_.data() + buf_.size(), '\n');
                if (first_ != buf_.data() + buf_.size()) {
                    ++first_;
                } else {
                    while (input_ && input_.get() != '\n') {}
                }
            } break;

            case lex_detail::pat_other_char:
            default: return lexeme[0];
        }
    }

    return kEof;
}

/*static*/ value json::reader::token_to_value(int tk, const token_val_t& tk_val) {
    switch (tk) {
        case kTrue: return value(true);
        case kFalse: return value(false);
        case kPosInt: {
            if (tk_val.u64 <= static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
                return value(static_cast<int32_t>(tk_val.u64));
            } else if (tk_val.u64 <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                return value(static_cast<int64_t>(tk_val.u64));
            }
            return value(tk_val.u64);
        } break;
        case kNegInt: {
            int64_t i64 = static_cast<int64_t>(tk_val.u64);
            if (i64 >= std::numeric_limits<int32_t>::min() && i64 <= std::numeric_limits<int32_t>::max()) {
                return value(static_cast<int32_t>(i64));
            }
            return value(i64);
        } break;
        case kDouble: return value(tk_val.dbl);
        case kString: return value(tk_val.str);
        default: break;
    }
    return value();
}

// --------------------------

void json::writer::write(const value& v, unsigned indent) {
    switch (v.type()) {
        case value::dtype::kNull: output_.write("null"); break;
        case value::dtype::kBoolean: output_.write(v.as_bool() ? "true" : "false"); break;
        case value::dtype::kInteger: fprint(output_, "{}", v.as_int()); break;
        case value::dtype::kUInteger: fprint(output_, "{}", v.as_uint()); break;
        case value::dtype::kInteger64: fprint(output_, "{}", v.as_int64()); break;
        case value::dtype::kUInteger64: fprint(output_, "{}", v.as_uint64()); break;
        case value::dtype::kDouble: fprint(output_, "{:#}", v.as_double()); break;
        case value::dtype::kString: output_.write(make_quoted_text(v.as_string())); break;
        case value::dtype::kArray: fmt_array(v, indent); break;
        case value::dtype::kRecord: fmt_object(v, indent); break;
    }
}

void json::writer::fmt_array(const value& v, unsigned indent) {
    output_.put('[');
    if (!v.empty()) {
        bool first = true;
        for (const value& sub_v : v.view()) {
            if (!first) { output_.write(", "); }
            write(sub_v, indent);
            first = false;
        }
    }
    output_.put(']');
}

void json::writer::fmt_object(const value& v, unsigned indent) {
    output_.put('{');
    if (!v.empty()) {
        bool first = true;
        for (const auto& item : v.map()) {
            if (!first) { output_.put(','); }
            output_.endl();
            output_.fill_n(indent + 4, ' ');
            output_.write(make_quoted_text(item.first));
            output_.write(": ");
            write(item.second, indent + 4);
            first = false;
        }
        output_.endl();
        output_.fill_n(indent, ' ');
    }
    output_.put('}');
}
