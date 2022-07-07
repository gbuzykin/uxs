#include "uxs/db/json_impl.h"
#include "uxs/utf.h"

namespace lex_detail {
#include "lex_defs.h"
}
namespace lex_detail {
#include "lex_analyzer.inl"
}

namespace uxs {
namespace db {

void json::reader::init_parser() {
    n_ln_ = 1;
    str_.clear();
    stash_.clear();
    state_stack_.clear();
    state_stack_.push_back(lex_detail::sc_initial);
}

int json::reader::parse_token(std::string_view& lval) {
    unsigned surrogate = 0;
    while (true) {
        int pat = 0;
        unsigned llen = 0;
        const char* first = input_.first_avail();
        while (true) {
            bool stack_limitation = false;
            const char* last = input_.last_avail();
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
            if (input_.avail()) {  // append read buffer to stash
                stash_.append(input_.first_avail(), input_.last_avail());
                input_.bump(input_.avail());
            }
            // read more characters from input
            input_.peek();
            first = input_.first_avail();
        }
        const char* lexeme = input_.first_avail();
        if (stash_.empty()) {  // the stash is empty
            input_.bump(llen);
        } else {
            if (llen >= stash_.size()) {  // all characters in stash buffer are used
                // concatenate full lexeme in stash
                size_t len_rest = llen - stash_.size();
                stash_.append(input_.first_avail(), input_.first_avail() + len_rest);
                input_.bump(len_rest);
            } else {  // at least one character in stash is yet unused
                      // put unused chars back to `iobuf`
                for (const char* p = stash_.last(); p != stash_.curr(); --p) { input_.unget(*(p - 1)); }
            }
            lexeme = stash_.data();
            stash_.clear();  // it resets end pointer, but retains the contents
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
                unsigned unicode = (uxs::dig_v<16>(lexeme[2]) << 12) | (uxs::dig_v<16>(lexeme[3]) << 8) |
                                   (uxs::dig_v<16>(lexeme[4]) << 4) | uxs::dig_v<16>(lexeme[5]);
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
            case lex_detail::pat_string_nl: return kEof;
            case lex_detail::pat_string_seq: {
                str_.append(lexeme, lexeme + llen);
            } break;
            case lex_detail::pat_string_close: {
                if (str_.empty()) {
                    lval = std::string_view(lexeme, llen - 1);
                } else {
                    str_.append(lexeme, lexeme + llen - 1);
                    lval = std::string_view(str_.data(), str_.size());
                    str_.clear();  // it resets end pointer, but retains the contents
                }
                state_stack_.pop_back();
                return kString;
            } break;

            case lex_detail::pat_null: return kNull;
            case lex_detail::pat_true: return kTrue;
            case lex_detail::pat_false: return kFalse;
            case lex_detail::pat_decimal: lval = std::string_view(lexeme, llen); return kInteger;
            case lex_detail::pat_neg_decimal: lval = std::string_view(lexeme, llen); return kNegInteger;
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
                        if (ch == '\0') { return kEof; }
                        if (ch == '\n') { ++n_ln_; }
                        ch = input_.get();
                    }
                    ch = input_.get();
                } while (input_ && ch != '/');
            } break;

            case lex_detail::predef_pat_default: {
                switch (lexeme[0]) {
                    case '\n': ++n_ln_; break;
                    case '\"': state_stack_.push_back(lex_detail::sc_string); break;
                    default: return lexeme[0];
                }
            } break;

            default: UNREACHABLE_CODE;
        }
    }
}

namespace json {
template basic_value<char> reader::read(const std::allocator<char>&);
template basic_value<wchar_t> reader::read(const std::allocator<wchar_t>&);
template void writer::write(const basic_value<char>&);
template void writer::write(const basic_value<wchar_t>&);
}  // namespace json
}  // namespace db
}  // namespace uxs