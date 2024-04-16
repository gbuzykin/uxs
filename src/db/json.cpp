#include "uxs/db/json_impl.h"
#include "uxs/utf.h"

namespace lex_detail {
#include "json_lex_defs.h"
}
namespace lex_detail {
#include "json_lex_analyzer.inl"
}

static std::uint8_t g_whitespaces[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

namespace uxs {
namespace db {

json::reader::reader(ibuf& input) : input_(input) { state_stack_.push_back(lex_detail::sc_initial); }

int json::reader::parse_token(std::string_view& lval) {
    unsigned surrogate = 0;
    while (true) {
        int pat = 0;
        unsigned llen = 0;
        const char* first = input_.first_avail();
        if (state_stack_.back() == lex_detail::sc_initial) {
            while (true) {  // skip whitespaces
                first = std::find_if<const char*>(first, input_.last_avail(), [this](char ch) {
                    if (ch != '\n') { return !g_whitespaces[static_cast<unsigned char>(ch)]; }
                    ++n_ln_;
                    return false;
                });
                input_.advance(first - input_.first_avail());
                if (first != input_.last_avail()) { break; }
                if (input_.peek() == ibuf::traits_type::eof()) { return static_cast<int>(token_t::eof); }
                first = input_.first_avail();
            }
            // just process a single character if it can't be recognized with analyzer
            if (lex_detail::Dtran[lex_detail::symb2meta[static_cast<unsigned char>(*first)]] < 0) {
                input_.advance(1);
                if (*first != '\"') { return static_cast<unsigned char>(*first); }
                state_stack_.push_back(lex_detail::sc_string);
                continue;
            }
        }
        while (true) {
            bool stack_limitation = false;
            const char* last = input_.last_avail();
            if (state_stack_.avail() < static_cast<std::size_t>(last - first)) {
                last = first + state_stack_.avail();
                stack_limitation = true;
            }
            pat = lex_detail::lex(first, last, state_stack_.p_curr(), &llen, stack_limitation || input_);
            if (pat >= lex_detail::predef_pat_default) {
                break;
            } else if (stack_limitation) {  // enlarge state stack and continue analysis
                state_stack_.reserve(llen);
                first = last;
                continue;
            } else if (!input_) {
                return static_cast<int>(token_t::eof);  // end of sequence, first_ == last_
            }
            if (input_.avail()) {  // append read buffer to stash
                stash_.append(input_.first_avail(), input_.last_avail());
                input_.advance(input_.avail());
            }
            // read more characters from input
            input_.peek();
            first = input_.first_avail();
        }
        const char* lexeme = input_.first_avail();
        if (stash_.empty()) {  // the stash is empty
            input_.advance(llen);
        } else {
            if (llen >= stash_.size()) {  // all characters in stash buffer are used
                // concatenate full lexeme in stash
                std::size_t len_rest = llen - stash_.size();
                stash_.append(input_.first_avail(), input_.first_avail() + len_rest);
                input_.advance(len_rest);
            } else {  // at least one character in stash is yet unused
                      // put unused chars back to `ibuf`
                for (const char* p = stash_.curr(); p != stash_.last(); ++p) { input_.unget(); }
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
            case lex_detail::pat_escape_other: return static_cast<int>(token_t::eof);
            case lex_detail::pat_escape_unicode: {
                unsigned unicode = (dig_v(lexeme[2]) << 12) | (dig_v(lexeme[3]) << 8) | (dig_v(lexeme[4]) << 4) |
                                   dig_v(lexeme[5]);
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
            case lex_detail::pat_string_nl: return static_cast<int>(token_t::eof);
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
                return static_cast<int>(token_t::string);
            } break;

            case lex_detail::pat_null: return static_cast<int>(token_t::null);
            case lex_detail::pat_true: return static_cast<int>(token_t::true_value);
            case lex_detail::pat_false: return static_cast<int>(token_t::false_value);
            case lex_detail::pat_decimal: {
                lval = std::string_view(lexeme, llen);
                return static_cast<int>(token_t::integer_number);
            } break;
            case lex_detail::pat_neg_decimal: {
                lval = std::string_view(lexeme, llen);
                return static_cast<int>(token_t::negative_integer_number);
            } break;
            case lex_detail::pat_real: {
                lval = std::string_view(lexeme, llen);
                return static_cast<int>(token_t::floating_point_number);
            } break;

            case lex_detail::pat_comment: {  // skip till end of line or stream
                int ch = input_.get();
                while (input_ && ch != '\n') {
                    if (ch == '\0') { return static_cast<int>(token_t::eof); }
                    ch = input_.get();
                }
                ++n_ln_;
            } break;
            case lex_detail::pat_c_comment: {  // skip till `*/`
                int ch = input_.get();
                do {
                    while (input_ && ch != '*') {
                        if (ch == '\0') { return static_cast<int>(token_t::eof); }
                        if (ch == '\n') { ++n_ln_; }
                        ch = input_.get();
                    }
                    ch = input_.get();
                } while (input_ && ch != '/');
            } break;

            case lex_detail::predef_pat_default: return static_cast<unsigned char>(lexeme[0]);
            default: UNREACHABLE_CODE;
        }
    }
}

namespace json {
template UXS_EXPORT basic_value<char> reader::read(token_t, const std::allocator<char>&);
template UXS_EXPORT basic_value<wchar_t> reader::read(token_t, const std::allocator<wchar_t>&);
template UXS_EXPORT void writer::write(const basic_value<char>&, unsigned);
template UXS_EXPORT void writer::write(const basic_value<wchar_t>&, unsigned);
}  // namespace json
}  // namespace db
}  // namespace uxs
