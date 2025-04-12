#include "uxs/impl/db/json_impl.h"

namespace lex_detail {
#include "json_lex_defs.h"
}
namespace lex_detail {
#include "json_lex_analyzer.inl"
}

namespace uxs {
namespace db {
namespace json {

detail::parser::parser(ibuf& in) : in(in) { stack.push_back(lex_detail::sc_initial); }

token_t detail::parser::lex(std::string_view& lval) {
    unsigned surrogate = 0;
    while (true) {
        int pat = 0;
        unsigned llen = 0;
        const char* first = in.first_avail();
        if (stack.back() == lex_detail::sc_initial) {
            while (true) {  // skip whitespaces
                first = std::find_if(first, in.last_avail(), [this](char ch) {
                    if (ch != '\n') {
                        return !(uxs::detail::char_tbl_t{}.flags()[static_cast<std::uint8_t>(ch)] &
                                 uxs::detail::char_bits::is_json_ws);
                    }
                    ++ln;
                    return false;
                });
                in.advance(first - in.first_avail());
                if (first != in.last_avail()) { break; }
                if (in.peek() == ibuf::traits_type::eof()) { return token_t::eof; }
                first = in.first_avail();
            }
            // just process a single character if it can't be recognized with analyzer
            if (lex_detail::Dtran[lex_detail::symb2meta[static_cast<std::uint8_t>(*first)]] < 0) {
                in.advance(1);
                if (*first != '\"') { return token_t(static_cast<std::uint8_t>(*first)); }
                stack.push_back(lex_detail::sc_string);
                continue;
            }
        }
        while (true) {
            bool stack_limitation = false;
            const char* last = in.last_avail();
            if (stack.avail() < static_cast<std::size_t>(last - first)) {
                last = first + stack.avail();
                stack_limitation = true;
            }
            auto* sptr = stack.endp();
            pat = lex_detail::lex(first, last, &sptr, &llen, stack_limitation || in);
            stack.setsize(sptr - stack.data());
            if (pat >= lex_detail::predef_pat_default) { break; }
            if (stack_limitation) {
                // enlarge state stack and continue analysis
                stack.reserve(llen);
                first = last;
                continue;
            }
            if (!in) { return token_t::eof; }  // end of sequence, first_ == last_
            if (in.avail()) {
                // append read buffer to stash
                stash.append(in.first_avail(), in.last_avail());
                in.advance(in.avail());
            }
            // read more characters from input
            in.peek();
            first = in.first_avail();
        }
        const char* lexeme = in.first_avail();
        if (stash.empty()) {  // the stash is empty
            in.advance(llen);
        } else {
            if (llen >= stash.size()) {
                // all characters in stash buffer are used
                // concatenate full lexeme in stash
                const std::size_t len_rest = llen - stash.size();
                stash.append(in.first_avail(), len_rest);
                in.advance(len_rest);
            } else {
                // at least one character in stash is yet unused
                // put unused chars back to `ibuf`
                for (std::size_t n = 0; n < stash.size() - llen; ++n) { in.unget(); }
            }
            lexeme = stash.data();
            stash.clear();  // it resets end pointer, but retains the contents
        }
        switch (pat) {
            // ------ escape sequences
            case lex_detail::pat_escape_quot: str += '\"'; break;
            case lex_detail::pat_escape_rev_sol: str += '\\'; break;
            case lex_detail::pat_escape_sol: str += '/'; break;
            case lex_detail::pat_escape_b: str += '\b'; break;
            case lex_detail::pat_escape_f: str += '\f'; break;
            case lex_detail::pat_escape_n: str += '\n'; break;
            case lex_detail::pat_escape_r: str += '\r'; break;
            case lex_detail::pat_escape_t: str += '\t'; break;
            case lex_detail::pat_escape_other: return token_t::eof;
            case lex_detail::pat_escape_unicode: {
                unsigned unicode = (dig_v(lexeme[2]) << 12) | (dig_v(lexeme[3]) << 8) | (dig_v(lexeme[4]) << 4) |
                                   dig_v(lexeme[5]);
                if (surrogate != 0) {
                    if ((unicode & 0xfc00) == 0xdc00) {
                        unicode = 0x10000 + (((surrogate & 0x3ff) << 10) | (unicode & 0x3ff));
                    } else {
                        to_utf8(surrogate, std::back_inserter(str));
                    }
                    surrogate = 0;
                } else if ((unicode & 0xfc00) == 0xd800) {
                    surrogate = unicode;
                    break;
                }
                to_utf8(unicode, std::back_inserter(str));
            } break;

            // ------ strings
            case lex_detail::pat_string_nl: return token_t::eof;
            case lex_detail::pat_string_seq: {
                str.append(lexeme, llen);
            } break;
            case lex_detail::pat_string_close: {
                if (str.empty()) {
                    lval = std::string_view(lexeme, llen - 1);
                } else {
                    str.append(lexeme, llen - 1);
                    lval = std::string_view(str.data(), str.size());
                    str.clear();  // it resets end pointer, but retains the contents
                }
                stack.pop_back();
                return token_t::string;
            } break;

            case lex_detail::pat_null: return token_t::null_value;
            case lex_detail::pat_true: return token_t::true_value;
            case lex_detail::pat_false: return token_t::false_value;
            case lex_detail::pat_decimal: {
                lval = std::string_view(lexeme, llen);
                return token_t::integer_number;
            } break;
            case lex_detail::pat_neg_decimal: {
                lval = std::string_view(lexeme, llen);
                return token_t::negative_integer_number;
            } break;
            case lex_detail::pat_real: {
                lval = std::string_view(lexeme, llen);
                return token_t::floating_point_number;
            } break;

            case lex_detail::pat_comment: {  // skip till end of line or out
                int ch = in.get();
                while (in && ch != '\n') {
                    if (ch == '\0') { return token_t::eof; }
                    ch = in.get();
                }
                ++ln;
            } break;
            case lex_detail::pat_c_comment: {  // skip till `*/`
                int ch = in.get();
                do {
                    while (in && ch != '*') {
                        if (ch == '\0') { return token_t::eof; }
                        if (ch == '\n') { ++ln; }
                        ch = in.get();
                    }
                    ch = in.get();
                } while (in && ch != '/');
            } break;

            case lex_detail::predef_pat_default: {
                return token_t(static_cast<std::uint8_t>(lexeme[0]));
            } break;
            default: UXS_UNREACHABLE_CODE;
        }
    }
}

template UXS_EXPORT basic_value<char> read(ibuf&, const std::allocator<char>&);
template UXS_EXPORT basic_value<wchar_t> read(ibuf&, const std::allocator<wchar_t>&);
template UXS_EXPORT void detail::writer<char>::do_write(const basic_value<char>&, unsigned);
template UXS_EXPORT void detail::writer<char>::do_write(const basic_value<wchar_t>&, unsigned);
template UXS_EXPORT void detail::writer<wchar_t>::do_write(const basic_value<char>&, unsigned);
template UXS_EXPORT void detail::writer<wchar_t>::do_write(const basic_value<wchar_t>&, unsigned);
}  // namespace json
}  // namespace db
}  // namespace uxs
