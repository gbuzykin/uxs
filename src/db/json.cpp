#include "uxs/format.h"
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
template UXS_EXPORT value parse(ibuf&, const std::allocator<char>&);
template UXS_EXPORT wvalue parse(ibuf&, const std::allocator<wchar_t>&);

namespace detail {

void lexer::report_error(const char* message) { throw database_error(format("{}: {}", ln, message)); }

token_t lexer::lex(std::string_view& lval) {
    std::uint32_t surrogate = 0;
    bool is_parsing_string = false;

    const auto get_next_state = [](int state, std::uint8_t ch) {
        return lex_detail::Dtran[lex_detail::dtran_width * state + lex_detail::symb2meta[ch]];
    };

    while (in.peek() != ibuf::traits_type::eof()) {
        using char_tbl_t = uxs::detail::char_tbl_t;
        std::int8_t state = lex_detail::sc_initial;

        if (!is_parsing_string) {
            const char* curr = in.curr();
            if (char_tbl_t::flags()[static_cast<std::uint8_t>(*curr)] &
                char_tbl_t::bits::json_ws) {  // skip whitespaces
                if (*curr == '\n') { ++ln; }
                curr = std::find_if(curr + 1, in.last(), [this](std::uint8_t ch) {
                    if (ch != '\n') { return !(char_tbl_t::flags()[ch] & char_tbl_t::bits::json_ws); }
                    ++ln;
                    return false;
                });
                in.setpos(curr - in.first());
                if (!in.avail()) { continue; }
            }

            // process the first character
            state = get_next_state(lex_detail::sc_initial, *curr);
            if (state < 0) {  // process a single character
                in.advance(1);
                if (*curr != '\"') { return token_t(static_cast<std::uint8_t>(*curr)); }
                is_parsing_string = true;
                continue;
            }
        } else {  // parse string
            const char* curr0 = in.curr();
            const char* curr = std::find_if(curr0, in.last(), [](std::uint8_t ch) {
                return !!(char_tbl_t::flags()[ch] & char_tbl_t::bits::json_string_special);
            });

            in.setpos(curr - in.first());
            if (!in.avail()) {
                surrogate = 0;
                stash.append(curr0, curr);
                continue;
            }

            if (*curr == '\"') {
                if (stash.empty()) {
                    lval = to_string_view(curr0, curr);
                } else {
                    stash.append(curr0, curr);
                    lval = std::string_view(stash.data(), stash.size());
                    stash.clear();  // it resets the stash, but retains the contents
                }
                in.advance(1);
                return token_t::string;
            }

            if (*curr != '\\') { break; }

            if (curr != curr0) {
                surrogate = 0;
                stash.append(curr0, curr);
            }

            // process '\\' character
            state = get_next_state(lex_detail::sc_string, '\\');
        }

        int pat = 0;

        // accept the first character
        std::size_t llen = 1;
        std::size_t stash_sz0 = stash.size();
        const char* first0 = in.curr() + 1;
        (void)lex_detail::lex;  // unused

        while (true) {
            const char* first = first0;
            while (first != in.last()) {
                const std::int8_t next_state = get_next_state(state, *first);
                if (next_state < 0) { break; }
                state = next_state, ++first;
            }

            llen += static_cast<std::size_t>(first - first0);

            if (first != in.last() || !in) {
                pat = lex_detail::accept[state];
                if (pat <= 0) { report_error("invalid token or string escape sequence"); }
                break;
            }

            // append lexeme in stash
            stash.append(in.curr(), in.last());
            in.setpos(in.capacity());
            // read more characters from input
            in.peek();
            first0 = in.curr();
        }

        const char* lexeme = in.curr();
        if (stash.size() == stash_sz0) {  // no stashed lexeme parts
            in.advance(llen);
        } else {
            if (llen >= stash.size() - stash_sz0) {  // concatenate full lexeme in stash
                const std::size_t len_rest = stash_sz0 + llen - stash.size();
                stash.append(in.curr(), len_rest);
                in.advance(len_rest);
            }
            lexeme = stash.endp() - llen;
            stash.setsize(stash_sz0);  // it restores stash position, but retains the contents
        }

        switch (pat) {
            // ------ escape sequences
            case lex_detail::pat_escape_quot: surrogate = 0, stash += '\"'; break;
            case lex_detail::pat_escape_rev_sol: surrogate = 0, stash += '\\'; break;
            case lex_detail::pat_escape_sol: surrogate = 0, stash += '/'; break;
            case lex_detail::pat_escape_b: surrogate = 0, stash += '\b'; break;
            case lex_detail::pat_escape_f: surrogate = 0, stash += '\f'; break;
            case lex_detail::pat_escape_n: surrogate = 0, stash += '\n'; break;
            case lex_detail::pat_escape_r: surrogate = 0, stash += '\r'; break;
            case lex_detail::pat_escape_t: surrogate = 0, stash += '\t'; break;
            case lex_detail::pat_escape_unicode: {
                std::uint32_t unicode = (dig_v{}(lexeme[2]) << 12) | (dig_v{}(lexeme[3]) << 8) |
                                        (dig_v{}(lexeme[4]) << 4) | dig_v{}(lexeme[5]);
                if (surrogate != 0) {
                    if ((unicode & 0xfc00) == 0xdc00) {
                        unicode = 0x10000 + (((surrogate & 0x3ff) << 10) | (unicode & 0x3ff));
                        stash.advance(-3);  // replace \xef\xbf\xbd sequence with the surrogate
                    }
                    surrogate = 0;
                } else if ((unicode & 0xfc00) == 0xd800) {
                    surrogate = unicode;
                    stash += string_literal<char, '\xef', '\xbf', '\xbd'>{}();  // replacement char
                    break;
                }
                stash.reserve(4);
                stash.advance(to_utf8(unicode, stash.endp()).count);
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
                bool backslash = false;
                while (true) {
                    const int ch = in.get();
                    if (ch == ibuf::traits_type::eof() || ch == 0) { return token_t::eof; }
                    if (ch == '\n') {
                        ++ln;
                        if (!backslash) { break; }
                    }
                    backslash = (ch == '\\');
                }
            } break;

            // ------ C comment
            case lex_detail::pat_c_comment: {  // skip till `*/`
                bool backslash = false;
                bool star = false;
                while (true) {
                    const int ch = in.get();
                    if (ch == ibuf::traits_type::eof() || ch == 0) { report_error("unterminated C-style comment"); }
                    if (ch == '\n') { ++ln; }
                    if (star && ch == '/') { break; }
                    star = !backslash && (ch == '*');
                    backslash = (ch == '\\');
                }
            } break;

            default: UXS_UNREACHABLE_CODE;
        }
    }

    if (is_parsing_string) { report_error("unterminated string"); }

    return token_t::eof;
}

template UXS_EXPORT void write_impl(membuffer& out, const value&);
template UXS_EXPORT void write_impl(membuffer& out, const wvalue&);
template UXS_EXPORT void write_impl(wmembuffer& out, const value&);
template UXS_EXPORT void write_impl(wmembuffer& out, const wvalue&);
template UXS_EXPORT void write_formatted_impl(membuffer& out, const value&, json_fmt_opts, unsigned);
template UXS_EXPORT void write_formatted_impl(membuffer& out, const wvalue&, json_fmt_opts, unsigned);
template UXS_EXPORT void write_formatted_impl(wmembuffer& out, const value&, json_fmt_opts, unsigned);
template UXS_EXPORT void write_formatted_impl(wmembuffer& out, const wvalue&, json_fmt_opts, unsigned);
}  // namespace detail
}  // namespace json
}  // namespace db
}  // namespace uxs
