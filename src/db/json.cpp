#include "util/db/json.h"

#include "util/format.h"
#include "util/utf.h"

namespace lex_detail {
#include "lex_defs.h"
}
namespace lex_detail {
#include "lex_analyzer.inl"
}

using namespace util;
using namespace util::db;

// --------------------------

value json::reader::read() {
    auto error = [&ln = n_ln_]() { throw exception(format("parsing error on line {}", ln)); };

    n_ln_ = 1;
    if (input_.peek() == iobuf::traits_type::eof()) { error(); }

    auto token_to_value = [&error](int tk, std::string_view lval) -> value {
        switch (tk) {
            case '[': return value::make_empty_array();
            case '{': return value::make_empty_record();
            case kNull: return {};
            case kTrue: return true;
            case kFalse: return false;
            case kInteger: {
                uint64_t u64 = 0;
                if (stoval(lval, u64) != 0) {
                    if (lval[0] == '-') {
                        int64_t i64 = static_cast<int64_t>(u64);
                        if (i64 >= std::numeric_limits<int32_t>::min() && i64 <= std::numeric_limits<int32_t>::max()) {
                            return static_cast<int32_t>(i64);
                        }
                        return i64;
                    } else if (u64 <= static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
                        return static_cast<int32_t>(u64);
                    } else if (u64 <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                        return static_cast<int64_t>(u64);
                    }
                    return u64;
                }
                // too big integer - treat as double
                return from_string<double>(lval);
            } break;
            case kDouble: return from_string<double>(lval);
            case kString: return lval;
        }
        error();
        return {};
    };

    std::string_view lval;
    basic_dynbuffer<value*, 32> stack;

    state_stack_.clear();
    state_stack_.push_back(lex_detail::sc_initial);

    int tk = parse_token(lval);
    value result = token_to_value(tk, lval);
    if (!result.is_array() && !result.is_record()) { return result; }

    value* top = &result;
    bool comma = false;
    tk = parse_token(lval);

loop:
    if (top->is_array()) {
        if (comma || tk != ']') {
            while (true) {
                value& el = top->emplace_back(token_to_value(tk, lval));
                tk = parse_token(lval);
                if (el.is_array() || el.is_record()) {
                    stack.push_back(top);
                    top = &el, comma = false;
                    goto loop;
                }
                if (tk == ']') { break; }
                if (tk != ',') { error(); }
                tk = parse_token(lval);
            }
        }
    } else if (comma || tk != '}') {
        while (true) {
            if (tk != kString) { error(); }
            std::string name(lval);
            tk = parse_token(lval);
            if (tk != ':') { error(); }
            tk = parse_token(lval);
            value& el = top->emplace(std::move(name), token_to_value(tk, lval));
            tk = parse_token(lval);
            if (el.is_array() || el.is_record()) {
                stack.push_back(top);
                top = &el, comma = false;
                goto loop;
            }
            if (tk == '}') { break; }
            if (tk != ',') { error(); }
            tk = parse_token(lval);
        }
    }

    while (!stack.empty()) {
        top = stack.back();
        stack.pop_back();
        tk = parse_token(lval);
        if (tk != (top->is_array() ? ']' : '}')) {
            if (tk != ',') { error(); }
            tk = parse_token(lval), comma = true;
            goto loop;
        }
    }
    return result;
}

int json::reader::parse_token(std::string_view& lval) {
    unsigned surrogate = 0;
    while (true) {
        int pat = 0;
        unsigned llen = 0;
        const char* first = input_.in_avail_first();
        while (true) {
            bool stack_limitation = false;
            const char* last = input_.in_avail_last();
            if (state_stack_.avail() < static_cast<size_t>(last - first)) {
                last = first + state_stack_.avail();
                stack_limitation = true;
            }
            pat = lex_detail::lex(first, last, state_stack_.p_curr(), &llen, stack_limitation || input_);
            if (pat >= lex_detail::predef_pat_default) {
                break;
            } else if (stack_limitation) {  // enlarge state stack and continue analysis
                state_stack_.reserve_at_curr(llen);
                first = last;
                continue;
            } else if (!input_) {
                return kEof;  // end of sequence, first_ == last_
            }
            if (!input_.in_avail_empty()) {  // append read buffer to stash
                stash_.append(input_.in_avail_first(), input_.in_avail_last());
                input_.bump(input_.in_avail());
            }
            // read more characters from input
            input_.peek();
            first = input_.in_avail_first();
        }
        const char* lexeme = input_.in_avail_first();
        if (stash_.empty()) {  // the stash is empty
            input_.bump(llen);
        } else if (llen >= stash_.size()) {  // all characters in stash buffer are used
            // concatenate full lexeme in stash
            size_t len_rest = llen - stash_.size();
            stash_.append(input_.in_avail_first(), input_.in_avail_first() + len_rest);
            input_.bump(len_rest);
            stash_.clear();  // it resets end pointer, but retains the contents
            lexeme = stash_.data();
        } else {  // at least one character in stash is yet unused
                  // put unused chars back to `iobuf`
            for (const char* p = stash_.last(); p != stash_.curr(); --p) { input_.unget(*(p - 1)); }
            lexeme = stash_.data();
        }
        switch (pat) {
            // ------ escape sequences
            case lex_detail::pat_escape_quot: str_.push_back('\"'); break;
            case lex_detail::pat_escape_rev_sol: str_.push_back('\\'); break;
            case lex_detail::pat_escape_sol: str_.push_back('/'); break;
            case lex_detail::pat_escape_b: str_.push_back('\b'); break;
            case lex_detail::pat_escape_f: str_.push_back('\f'); break;
            case lex_detail::pat_escape_n: str_.push_back('\n'); break;
            case lex_detail::pat_escape_r: str_.push_back('\r'); break;
            case lex_detail::pat_escape_t: str_.push_back('\t'); break;
            case lex_detail::pat_escape_other: return kEof;
            case lex_detail::pat_escape_unicode: {
                unsigned unicode = (util::dig_v<16>(lexeme[2]) << 12) | (util::dig_v<16>(lexeme[3]) << 8) |
                                   (util::dig_v<16>(lexeme[4]) << 4) | util::dig_v<16>(lexeme[5]);
                if (surrogate != 0) {
                    unicode = 0x10000 + (((surrogate & 0x3ff) << 10) | (unicode & 0x3ff));
                    surrogate = 0;
                } else if ((unicode & 0xdc00) == 0xd800) {
                    surrogate = unicode;
                    break;
                }
                to_utf8(unicode, std::back_inserter(str_));
            } break;

            // ------ strings
            case lex_detail::pat_string_seq: {
                str_.append(lexeme, lexeme + llen);
            } break;
            case lex_detail::pat_string_close: {
                if (str_.empty()) {
                    lval = std::string_view(lexeme, llen - 1);
                } else {
                    str_.append(lexeme, lexeme + llen - 1);
                    lval = std::string_view(str_.data(), str_.size());
                }
                state_stack_.pop_back();
                return kString;
            } break;

            case lex_detail::pat_null: return kNull;
            case lex_detail::pat_true: return kTrue;
            case lex_detail::pat_false: return kFalse;
            case lex_detail::pat_decimal: lval = std::string_view(lexeme, llen); return kInteger;
            case lex_detail::pat_real: lval = std::string_view(lexeme, llen); return kDouble;

            case lex_detail::pat_whitespace: break;
            case lex_detail::pat_comment: {  // skip till end of line or stream
                int ch = input_.get();
                while (input_ && ch != '\n') {
                    if (ch == '\0') { return kEof; }
                    ch = input_.get();
                }
                ++n_ln_;
            } break;
            case lex_detail::pat_c_comment: {  // skip till `*/`
                int ch = input_.get();
                do {
                    while (input_ && ch != '*') {
                        if (ch == '\n') { ++n_ln_; }
                        if (ch == '\0') { return kEof; }
                        ch = input_.get();
                    }
                    ch = input_.get();
                } while (input_ && ch != '/');
            } break;

            default: {
                if (lexeme[0] != '\n' && lexeme[0] != '\"') {
                    return lexeme[0];
                } else if (lexeme[0] == '\"') {
                    str_.clear();
                    state_stack_.push_back(lex_detail::sc_string);
                } else {
                    if (state_stack_.back() == lex_detail::sc_string) { return kEof; }
                    ++n_ln_;
                }
            } break;
        }
    }

    return kEof;
}

// --------------------------

struct writer_stack_item_t {
    writer_stack_item_t(const value* p, const value* el) : v(p), array_element(el) {}
    writer_stack_item_t(const value* p, value::const_record_iterator el) : v(p), record_element(el) {}
    const value* v;
#if !defined(_MSC_VER) || _MSC_VER >= 1920
    union {
        const value* array_element;
        value::const_record_iterator record_element;
    };
#else   // !defined(_MSC_VER) || _MSC_VER >= 1920
    // Old versions of VS compiler don't support such unions
    const value* array_element = nullptr;
    value::const_record_iterator record_element;
#endif  // !defined(_MSC_VER) || _MSC_VER >= 1920
};

void json::writer::write(const value& v) {
    unsigned indent = 0;
    basic_dynbuffer<writer_stack_item_t, 32> stack;

    auto write_value = [this, &stack, &indent](const value& v) {
        switch (v.type()) {
            case value::dtype::kArray: {
                output_.put('[');
                stack.emplace_back(&v, v.as_array().data());
                return true;
            } break;
            case value::dtype::kRecord: {
                output_.put('{');
                indent += indent_size_;
                stack.emplace_back(&v, v.as_map().begin());
                return true;
            } break;
            default: v.print_scalar(output_); break;
        }
        return false;
    };

    if (!write_value(v)) { return; }

loop:
    writer_stack_item_t* top = stack.curr() - 1;
    if (top->v->is_array()) {
        auto range = top->v->as_array();
        const value *el = top->array_element, *el_end = range.data() + range.size();
        while (el != el_end) {
            if (el != range.data()) { output_.put(',').put(' '); }
            if (write_value(*el++)) {
                (stack.curr() - 2)->array_element = el;
                goto loop;
            }
        }
        output_.put(']');
    } else {
        auto range = top->v->as_map();
        auto el = top->record_element, el_end = range.end();
        while (el != el_end) {
            if (el != range.begin()) { output_.put(','); }
            output_.put('\n');
            output_.fill_n(indent, indent_char_);
            print_quoted_text<char>(output_, el->first);
            output_.put(':').put(' ');
            if (write_value((el++)->second)) {
                (stack.curr() - 2)->record_element = el;
                goto loop;
            }
        }
        indent -= indent_size_;
        if (!top->v->empty()) {
            output_.put('\n');
            output_.fill_n(indent, indent_char_);
        }
        output_.put('}');
    }

    stack.pop_back();
    if (!stack.empty()) { goto loop; }
}
