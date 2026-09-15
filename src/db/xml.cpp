#include "uxs/format.h"
#include "uxs/impl/db/xml_impl.h"

namespace lex_detail {
#include "xml_lex_defs.h"
}
namespace lex_detail {
#include "xml_lex_analyzer.inl"
}

namespace uxs {
namespace db {
namespace xml {

namespace detail {
inline bool is_equal_to_string_nocase(std::string_view s, std::string_view ref) {
    if (s.size() != ref.size()) { return false; }
    return std::equal(s.begin(), s.end(), ref.begin(), [](char l, char r) { return to_lower{}(l) == r; });
}
}  // namespace detail

parser::parser(ibuf& in) : lexer_(in), name_cache_(16), token_{token_t::none, {}} {}

std::pair<token_t, std::string_view> parser::next_impl() {
    if (is_end_element_pending_) {
        is_end_element_pending_ = false;
        return {token_t::end_element, name_cache_.front()};
    }

    while (lexer_.in.peek() != ibuf::traits_type::eof()) {
        std::string_view lval;

        switch (*lexer_.in.curr()) {
            case '<': {  // found '<'
                auto name_cache_it = name_cache_.begin();
                auto name_cache_prev_it = name_cache_it;

                attrs_.clear();

                const auto parse_attribute = [this, &name_cache_it, &name_cache_prev_it](std::string_view lval) {
                    if (name_cache_it != name_cache_.end()) {
                        name_cache_it->assign(lval.data(), lval.size());
                    } else {
                        name_cache_it = name_cache_.emplace_after(name_cache_prev_it, lval);
                    }
                    if (lexer_.lex(lval) != detail::lex_token_t::eq) { lexer_.report_error("expected `=`"); }
                    if (lexer_.lex(lval) != detail::lex_token_t::string) {
                        lexer_.report_error("expected valid attribute value");
                    }
                    name_cache_prev_it = name_cache_it;
                    attrs_.emplace(*name_cache_it++, lval);
                };

                switch (lexer_.lex(lval)) {
                    case detail::lex_token_t::start_element_open: {  // <name n1=v1 n2=v2...> or <name n1=v1 n2=v2.../>
                        name_cache_it->assign(lval.data(), lval.size());
                        ++name_cache_it;
                        while (true) {
                            auto tt = lexer_.lex(lval);
                            if (tt == detail::lex_token_t::name) {
                                parse_attribute(lval);
                            } else if (tt == detail::lex_token_t::close) {
                                return {token_t::start_element, name_cache_.front()};
                            } else if (tt == detail::lex_token_t::end_element_close) {
                                is_end_element_pending_ = true;
                                return {token_t::start_element, name_cache_.front()};
                            } else {
                                lexer_.report_error("expected name, `>` or `/>`");
                            }
                        }
                    } break;

                    case detail::lex_token_t::end_element_open: {  // </name>
                        if (lexer_.lex(lval) != detail::lex_token_t::close) { lexer_.report_error("expected `>`"); }
                        return {token_t::end_element, lval};
                    } break;

                    case detail::lex_token_t::pi_open: {  // <?xml n1=v1 n2=v2...?>
                        if (!detail::is_equal_to_string_nocase(lval, string_literal<char, 'x', 'm', 'l'>{}())) {
                            lexer_.report_error("invalid document declaration");
                        }
                        name_cache_it->assign(lval.data(), lval.size());
                        ++name_cache_it;
                        while (true) {
                            auto tt = lexer_.lex(lval);
                            if (tt == detail::lex_token_t::name) {
                                parse_attribute(lval);
                            } else if (tt == detail::lex_token_t::pi_close) {
                                return {token_t::preamble, name_cache_.front()};
                            } else {
                                lexer_.report_error("expected name or `?>`");
                            }
                        }
                    } break;

                    case detail::lex_token_t::comment: {  // comment <!--....-->: skip till `-->`
                        std::size_t dash_count = 0;
                        while (true) {
                            const int ch = lexer_.in.get();
                            if (ch == ibuf::traits_type::eof() || ch == 0) { return {token_t::eof, {}}; }
                            if (ch == '\n') { ++lexer_.ln; }
                            if (dash_count >= 2 && ch == '>') { break; }
                            dash_count = (ch == '-' ? dash_count + 1 : 0);
                        }
                    } break;

                    default: break;
                }
            } break;

            case '&': {  // parse entity
                if (lexer_.lex(lval) == detail::lex_token_t::predef_entity) { return {token_t::plain_text, lval}; }
                return {token_t::entity, lval};
            } break;

            case '\0': return {token_t::eof, {}};

            default: {
                const char* curr0 = lexer_.in.curr();
                const char* curr = std::find_if(curr0, lexer_.in.last(), [this](std::uint8_t ch) {
                    using char_tbl_t = uxs::detail::char_tbl_t;
                    if (ch != '\n') { return !!(char_tbl_t::flags()[ch] & char_tbl_t::bits::xml_special); }
                    ++lexer_.ln;
                    return false;
                });
                lexer_.in.setpos(curr - lexer_.in.first());
                return {token_t::plain_text, to_string_view(curr0, curr)};
            } break;
        }
    }

    return {token_t::eof, {}};
}

value_class parser::classify_value(const std::string_view& sval) {
    std::int8_t state = lex_detail::sc_value;
    for (const std::uint8_t ch : sval) {
        state = lex_detail::Dtran[lex_detail::dtran_width * state + lex_detail::symb2meta[ch]];
    }
    switch (lex_detail::accept[state]) {
        case lex_detail::pat_null: return value_class::null_value;
        case lex_detail::pat_true: return value_class::true_value;
        case lex_detail::pat_false: return value_class::false_value;
        case lex_detail::pat_decimal: return value_class::integer_number;
        case lex_detail::pat_neg_decimal: return value_class::negative_integer_number;
        case lex_detail::pat_real: return value_class::floating_point_number;
        case lex_detail::pat_ws_with_nl: return value_class::ws_with_nl;
        case lex_detail::pat_other_value: return value_class::other;
        default: return value_class::empty;
    }
}

template UXS_EXPORT value parser::parse(std::string_view, const std::allocator<char>&);
template UXS_EXPORT wvalue parser::parse(std::string_view, const std::allocator<wchar_t>&);

namespace detail {

void lexer::report_error(const char* message) { throw database_error(format("{}: {}", ln, message)); }

lex_token_t lexer::lex(std::string_view& lval) {
    char current_string_quot = '\0';
    bool need_to_normalize_string = false;

    const auto get_next_state = [](int state, std::uint8_t ch) {
        return lex_detail::Dtran[lex_detail::dtran_width * state + lex_detail::symb2meta[ch]];
    };

    const auto normalize_string = [](char* p, std::size_t sz) {
        char* p0 = p;
        char* p_to = p;
        char* p_end = p + sz;
        while (p != p_end) {
            if (*p == '\n') {
                using char_tbl_t = uxs::detail::char_tbl_t;
                while (p_to != p0 &&
                       (char_tbl_t::flags()[static_cast<std::uint8_t>(*(p_to - 1))] & char_tbl_t::bits::json_ws)) {
                    --p_to;
                }
                while (++p != p_end &&
                       (char_tbl_t::flags()[static_cast<std::uint8_t>(*p)] & char_tbl_t::bits::json_ws)) {}
                *p_to++ = ' ';
            } else {
                *p_to++ = *p++;
            }
        }
        return static_cast<std::size_t>(p_to - p0);
    };

    while (in.peek() != ibuf::traits_type::eof()) {
        using char_tbl_t = uxs::detail::char_tbl_t;
        std::int8_t state = 0;

        if (!current_string_quot) {
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
            if (state < 0) {  // just process a single character if it can't be recognized with analyzer
                in.advance(1);
                if (*curr != '\"' && *curr != '\'') { return lex_token_t(static_cast<std::uint8_t>(*curr)); }
                current_string_quot = *curr;
                continue;
            }
        } else {  // parse string
            const char other_quot = (current_string_quot == '\"' ? '\'' : '\"');
            const char* curr0 = in.curr();
            const char* curr = std::find_if(curr0, in.last(), [other_quot](std::uint8_t ch) {
                return !!(char_tbl_t::flags()[ch] & char_tbl_t::bits::xml_string_special) && ch != other_quot;
            });

            in.setpos(curr - in.first());
            if (!in.avail()) {
                stash.append(curr0, curr);
                continue;
            }

            if (*curr == current_string_quot) {
                if (stash.empty()) {
                    lval = to_string_view(curr0, curr);
                } else {
                    stash.append(curr0, curr);
                    if (need_to_normalize_string) { stash.setsize(normalize_string(stash.data(), stash.size())); }
                    lval = std::string_view(stash.data(), stash.size());
                    stash.clear();  // it resets the stash, but retains the contents
                }
                in.advance(1);
                return lex_token_t::string;
            }

            stash.append(curr0, curr);

            if (*curr != '&') {
                if (*curr != '\n') { break; }
                stash.push_back('\n');
                in.advance(1);
                need_to_normalize_string = true;
                continue;
            }

            // process '&' character
            state = get_next_state(lex_detail::sc_initial, '&');
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
                if (pat <= 0) { report_error("invalid token or escape sequence"); }
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
            // ------ entities
            case lex_detail::pat_amp: {
                if (!current_string_quot) {
                    lval = string_literal<char, '&'>{}();
                    return lex_token_t::predef_entity;
                }
                stash += '&';
            } break;
            case lex_detail::pat_lt: {
                if (!current_string_quot) {
                    lval = string_literal<char, '<'>{}();
                    return lex_token_t::predef_entity;
                }
                stash += '<';
            } break;
            case lex_detail::pat_gt: {
                if (!current_string_quot) {
                    lval = string_literal<char, '>'>{}();
                    return lex_token_t::predef_entity;
                }
                stash += '>';
            } break;
            case lex_detail::pat_apos: {
                if (!current_string_quot) {
                    lval = string_literal<char, '\''>{}();
                    return lex_token_t::predef_entity;
                }
                stash += '\'';
            } break;
            case lex_detail::pat_quot: {
                if (!current_string_quot) {
                    lval = string_literal<char, '\"'>{}();
                    return lex_token_t::predef_entity;
                }
                stash += '\"';
            } break;
            case lex_detail::pat_entity: {
                if (!current_string_quot) {
                    lval = std::string_view(lexeme + 1, llen - 2);
                    return lex_token_t::entity;
                }
                report_error("unknown entity name");
            } break;
            case lex_detail::pat_dcode: {
                std::uint32_t unicode = 0;
                for (const char ch : std::string_view(lexeme + 2, llen - 3)) { unicode = 10 * unicode + dig_v{}(ch); }
                if (!current_string_quot) {
                    const std::size_t count = to_utf8(unicode, stash.data()).count;
                    lval = std::string_view(stash.data(), count);
                    return lex_token_t::entity;
                }
                stash.reserve(4);
                stash.advance(to_utf8(unicode, stash.endp()).count);
            } break;
            case lex_detail::pat_hcode: {
                std::uint32_t unicode = 0;
                for (const char ch : std::string_view(lexeme + 3, llen - 4)) { unicode = (unicode << 4) + dig_v{}(ch); }
                if (!current_string_quot) {
                    const std::size_t count = to_utf8(unicode, stash.data()).count;
                    lval = std::string_view(stash.data(), count);
                    return lex_token_t::entity;
                }
                stash.reserve(4);
                stash.advance(to_utf8(unicode, stash.endp()).count);
            } break;

            // ------ tags
            case lex_detail::pat_name: {
                lval = std::string_view(lexeme, llen);
                return lex_token_t::name;
            } break;
            case lex_detail::pat_start_element_open: {
                lval = std::string_view(lexeme + 1, llen - 1);
                return lex_token_t::start_element_open;
            } break;
            case lex_detail::pat_end_element_open: {
                lval = std::string_view(lexeme + 2, llen - 2);
                return lex_token_t::end_element_open;
            } break;
            case lex_detail::pat_pi_open: {
                lval = std::string_view(lexeme + 2, llen - 2);
                return lex_token_t::pi_open;
            } break;
            case lex_detail::pat_end_element_close: return lex_token_t::end_element_close;
            case lex_detail::pat_pi_close: return lex_token_t::pi_close;

            // ------ comment
            case lex_detail::pat_comment: return lex_token_t::comment;

            default: UXS_UNREACHABLE_CODE;
        }
    }

    if (current_string_quot) { report_error("unterminated string or unexpected string character"); }

    return lex_token_t::eof;
}

template UXS_EXPORT void write_impl(membuffer& out, const value&, std::string_view, xml_fmt_opts, unsigned);
template UXS_EXPORT void write_impl(membuffer& out, const wvalue&, std::wstring_view, xml_fmt_opts, unsigned);
template UXS_EXPORT void write_impl(wmembuffer& out, const value&, std::string_view, xml_fmt_opts, unsigned);
template UXS_EXPORT void write_impl(wmembuffer& out, const wvalue&, std::wstring_view, xml_fmt_opts, unsigned);
}  // namespace detail
}  // namespace xml
}  // namespace db
}  // namespace uxs
