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

detail::lexer::lexer(ibuf& in) : in(in) { stack.push_back(lex_detail::sc_initial); }

token_t detail::lexer::lex(std::string_view& lval) {
    unsigned surrogate = 0;

    while (in.peek() != ibuf::traits_type::eof()) {
        using tbl = uxs::detail::char_tbl_t;
        std::int8_t state = 0;

        if (stack[0] == lex_detail::sc_initial) {
            const char* curr = in.curr();
            if (tbl{}.flags()[static_cast<std::uint8_t>(*curr)] & tbl::is_json_ws) {  // skip whitespaces
                if (*curr == '\n') { ++ln; }
                curr = std::find_if(curr + 1, in.last(), [this](std::uint8_t ch) {
                    if (ch != '\n') { return !(tbl{}.flags()[ch] & tbl::is_json_ws); }
                    ++ln;
                    return false;
                });
                in.setpos(curr - in.first());
                if (!in.avail()) { continue; }
            }

            // process the first character
            state = lex_detail::Dtran[lex_detail::dtran_width * static_cast<int>(lex_detail::sc_initial) +
                                      lex_detail::symb2meta[static_cast<std::uint8_t>(*curr)]];
            if (state < 0) {  // just process a single character if it can't be recognized with analyzer
                in.advance(1);
                if (*curr != '\"') { return token_t(static_cast<std::uint8_t>(*curr)); }
                stack[0] = lex_detail::sc_string;
                continue;
            }
        } else {  // read string
            const char* curr0 = in.curr();
            const char* curr = std::find_if(
                curr0, in.last(), [](std::uint8_t ch) { return !!(tbl{}.flags()[ch] & tbl::is_string_special); });

            in.setpos(curr - in.first());
            if (!in.avail()) {
                str.append(curr0, curr);
                continue;
            }

            if (*curr == '\"') {
                if (str.empty()) {
                    lval = to_string_view(curr0, curr);
                } else {
                    str.append(curr0, curr);
                    lval = std::string_view(str.data(), str.size());
                    str.clear();  // it resets end pointer, but retains the contents
                }
                in.advance(1);
                stack[0] = lex_detail::sc_initial;
                return token_t::string;
            }

            if (*curr != '\\') { throw database_error(to_string(ln) + ": unterminated string"); }

            str.append(curr0, curr);

            // process '\\' character
            state = lex_detail::Dtran[lex_detail::dtran_width * static_cast<int>(lex_detail::sc_string) +
                                      lex_detail::symb2meta[static_cast<int>('\\')]];
        }

        int pat = 0;

        // accept the first character
        std::size_t llen = 1;
        const char* first = in.curr() + 1;
        stack.push_back(state);

        while (true) {
            const char* last = in.last();
            if (stack.avail() < static_cast<std::size_t>(last - first)) { last = first + stack.avail(); }
            auto* sptr = stack.endp();
            pat = lex_detail::lex(first, last, &sptr, &llen, last != in.last() || in ? lex_detail::flag_has_more : 0);
            stack.setsize(sptr - stack.data());
            if (pat >= lex_detail::predef_pat_default) { break; }
            if (last != in.last()) {
                // enlarge state stack and continue analysis
                stack.reserve(llen);
                first = last;
                continue;
            }
            if (!in) { return token_t::eof; }  // end of sequence, first == last
            // append read buffer to stash
            stash.append(in.curr(), in.last());
            in.setpos(in.capacity());
            // read more characters from input
            in.peek();
            first = in.curr();
        }

        const char* lexeme = in.curr();
        if (stash.empty()) {  // the stash is empty
            in.advance(llen);
        } else {
            if (llen >= stash.size()) {
                // all characters in stash buffer are used concatenate full lexeme in stash
                const std::size_t len_rest = llen - stash.size();
                stash.append(in.curr(), len_rest);
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
            case lex_detail::pat_escape_invalid: {
                throw database_error(to_string(ln) + ": invalid escape sequence");
            } break;

            // ------ values
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

            // ------ C++ comment
            case lex_detail::pat_comment: {  // skip till end of line or end of file
                int ch = 0;
                while (true) {
                    ch = in.get();
                    if (ch == ibuf::traits_type::eof() || ch == 0) { return token_t::eof; }
                    if (ch == '\n') { break; }
                }
                ++ln;
            } break;

            // ------ C comment
            case lex_detail::pat_c_comment: {  // skip till `*/`
                int ch = 0;
                bool star = false;
                while (true) {
                    ch = in.get();
                    if (ch == ibuf::traits_type::eof() || ch == 0) {
                        throw database_error(to_string(ln) + ": unterminated C-style comment");
                    }
                    if (ch == '\n') { ++ln; }
                    if (star && ch == '/') { break; }
                    star = (ch == '*');
                }
            } break;

            // ------ other single character
            case lex_detail::predef_pat_default: return token_t(static_cast<std::uint8_t>(lexeme[0]));

            default: UXS_UNREACHABLE_CODE;
        }
    }

    return token_t::eof;
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
