#include "uxs/impl/db/xml_impl.h"
#include "uxs/string_alg.h"

namespace lex_detail {
#include "xml_lex_defs.h"
}
namespace lex_detail {
#include "xml_lex_analyzer.inl"
}

namespace uxs {
namespace db {
namespace xml {

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

                const auto read_attribute = [this, &name_cache_it, &name_cache_prev_it](std::string_view lval) {
                    if (name_cache_it != name_cache_.end()) {
                        name_cache_it->assign(lval.data(), lval.size());
                    } else {
                        name_cache_it = name_cache_.emplace_after(name_cache_prev_it, lval);
                    }
                    if (lexer_.lex(lval) != detail::lex_token_t::eq) {
                        throw database_error(to_string(lexer_.ln) + ": expected `=`");
                    }
                    if (lexer_.lex(lval) != detail::lex_token_t::string) {
                        throw database_error(to_string(lexer_.ln) + ": expected valid attribute value");
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
                                read_attribute(lval);
                            } else if (tt == detail::lex_token_t::close) {
                                return {token_t::start_element, name_cache_.front()};
                            } else if (tt == detail::lex_token_t::end_element_close) {
                                is_end_element_pending_ = true;
                                return {token_t::start_element, name_cache_.front()};
                            } else {
                                throw database_error(to_string(lexer_.ln) + ": expected name, `>` or `/>`");
                            }
                        }
                    } break;

                    case detail::lex_token_t::end_element_open: {  // </name>
                        if (lexer_.lex(lval) != detail::lex_token_t::close) {
                            throw database_error(to_string(lexer_.ln) + ": expected `>`");
                        }
                        return {token_t::end_element, lval};
                    } break;

                    case detail::lex_token_t::pi_open: {  // <?xml n1=v1 n2=v2...?>
                        if (compare_strings_nocase(lval, string_literal<char, 'x', 'm', 'l'>{}()) != 0) {
                            throw database_error(to_string(lexer_.ln) + ": invalid document declaration");
                        }
                        name_cache_it->assign(lval.data(), lval.size());
                        ++name_cache_it;
                        while (true) {
                            auto tt = lexer_.lex(lval);
                            if (tt == detail::lex_token_t::name) {
                                read_attribute(lval);
                            } else if (tt == detail::lex_token_t::pi_close) {
                                return {token_t::preamble, name_cache_.front()};
                            } else {
                                throw database_error(to_string(lexer_.ln) + ": expected name or `?>`");
                            }
                        }
                    } break;

                    case detail::lex_token_t::comment: {  // comment <!--....-->: skip till `-->`
                        int ch = 0;
                        std::size_t dash_count = 0;
                        while (true) {
                            ch = lexer_.in.get();
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
                    using tbl = uxs::detail::char_tbl_t;
                    if (ch != '\n') { return !!(tbl{}.flags()[ch] & tbl::is_xml_special); }
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

/*static*/ value_class parser::classify_value(const std::string_view& sval) {
    int state = lex_detail::sc_value;
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

detail::lexer::lexer(ibuf& in) : in(in) { stack.push_back(lex_detail::sc_initial); }

detail::lex_token_t detail::lexer::lex(std::string_view& lval) {
    char current_string_quot = '\0';
    bool need_to_normalize_string = false;

    const auto normalize_string = [](char* p, std::size_t sz) {
        char* p0 = p;
        char* p_to = p;
        char* p_end = p + sz;
        while (p != p_end) {
            if (*p == '\n') {
                using tbl = uxs::detail::char_tbl_t;
                while (p_to != p0 && (tbl{}.flags()[static_cast<std::uint8_t>(*(p_to - 1))] & tbl::is_json_ws)) {
                    --p_to;
                }
                while (++p != p_end && (tbl{}.flags()[static_cast<std::uint8_t>(*p)] & tbl::is_json_ws)) {}
                *p_to++ = ' ';
            } else {
                *p_to++ = *p++;
            }
        }
        return static_cast<std::size_t>(p_to - p0);
    };

    while (in.peek() != ibuf::traits_type::eof()) {
        using tbl = uxs::detail::char_tbl_t;
        std::int8_t state = 0;

        if (!current_string_quot) {
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
                if (*curr != '\"' && *curr != '\'') { return lex_token_t(static_cast<std::uint8_t>(*curr)); }
                current_string_quot = *curr;
                need_to_normalize_string = false;
                continue;
            }
        } else {  // read string
            const char other_quot = (current_string_quot == '\"' ? '\'' : '\"');
            const char* curr0 = in.curr();
            const char* curr = std::find_if(curr0, in.last(), [other_quot](std::uint8_t ch) {
                return !!(tbl{}.flags()[ch] & tbl::is_xml_string_special) && ch != other_quot;
            });

            in.setpos(curr - in.first());
            if (!in.avail()) {
                str.append(curr0, curr);
                continue;
            }

            if (*curr == current_string_quot) {
                if (str.empty()) {
                    lval = to_string_view(curr0, curr);
                } else {
                    str.append(curr0, curr);
                    if (need_to_normalize_string) { str.setsize(normalize_string(str.data(), str.size())); }
                    lval = std::string_view(str.data(), str.size());
                    str.clear();  // it resets end pointer, but retains the contents
                }
                in.advance(1);
                current_string_quot = '\0';
                return lex_token_t::string;
            }

            str.append(curr0, curr);

            if (*curr != '&') {
                if (*curr != '\n') {
                    throw database_error(to_string(ln) + ": unterminated string or unexpected string character");
                }
                str.push_back('\n');
                in.advance(1);
                need_to_normalize_string = true;
                continue;
            }

            // process '&' character
            state = lex_detail::Dtran[lex_detail::dtran_width * static_cast<int>(lex_detail::sc_initial) +
                                      lex_detail::symb2meta[static_cast<int>('&')]];
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
            if (!in) { return lex_token_t::eof; }  // end of sequence, first == last
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
            // ------ entities
            case lex_detail::pat_amp: {
                if (!current_string_quot) {
                    lval = string_literal<char, '&'>{}();
                    return detail::lex_token_t::predef_entity;
                }
                str += '&';
            } break;
            case lex_detail::pat_lt: {
                if (!current_string_quot) {
                    lval = string_literal<char, '<'>{}();
                    return detail::lex_token_t::predef_entity;
                }
                str += '<';
            } break;
            case lex_detail::pat_gt: {
                if (!current_string_quot) {
                    lval = string_literal<char, '>'>{}();
                    return detail::lex_token_t::predef_entity;
                }
                str += '>';
            } break;
            case lex_detail::pat_apos: {
                if (!current_string_quot) {
                    lval = string_literal<char, '\''>{}();
                    return detail::lex_token_t::predef_entity;
                }
                str += '\'';
            } break;
            case lex_detail::pat_quot: {
                if (!current_string_quot) {
                    lval = string_literal<char, '\"'>{}();
                    return detail::lex_token_t::predef_entity;
                }
                str += '\"';
            } break;
            case lex_detail::pat_entity: {
                if (!current_string_quot) {
                    lval = std::string_view(lexeme + 1, llen - 2);
                    return detail::lex_token_t::entity;
                }
                throw database_error(to_string(ln) + ": unknown entity name");
            } break;
            case lex_detail::pat_dcode: {
                unsigned unicode = 0;
                for (const char ch : std::string_view(lexeme + 2, llen - 3)) { unicode = 10 * unicode + dig_v(ch); }
                if (!current_string_quot) {
                    lval = std::string_view(str.data(), to_utf8(unicode, str.data()));
                    return detail::lex_token_t::entity;
                }
                to_utf8(unicode, std::back_inserter(str));
            } break;
            case lex_detail::pat_hcode: {
                unsigned unicode = 0;
                for (const char ch : std::string_view(lexeme + 3, llen - 4)) { unicode = (unicode << 4) + dig_v(ch); }
                if (!current_string_quot) {
                    lval = std::string_view(str.data(), to_utf8(unicode, str.data()));
                    return detail::lex_token_t::entity;
                }
                to_utf8(unicode, std::back_inserter(str));
            } break;
            case lex_detail::pat_ent_invalid: {
                throw database_error(to_string(ln) + ": single `&` character or invalid entity format");
            } break;

            // ------ tags
            case lex_detail::pat_name: {
                lval = std::string_view(lexeme, llen);
                return detail::lex_token_t::name;
            } break;
            case lex_detail::pat_start_element_open: {
                lval = std::string_view(lexeme + 1, llen - 1);
                return detail::lex_token_t::start_element_open;
            } break;
            case lex_detail::pat_end_element_open: {
                lval = std::string_view(lexeme + 2, llen - 2);
                return detail::lex_token_t::end_element_open;
            } break;
            case lex_detail::pat_pi_open: {
                lval = std::string_view(lexeme + 2, llen - 2);
                return detail::lex_token_t::pi_open;
            } break;
            case lex_detail::pat_end_element_close: return detail::lex_token_t::end_element_close;
            case lex_detail::pat_pi_close: return detail::lex_token_t::pi_close;

            // ------ comment
            case lex_detail::pat_comment: return detail::lex_token_t::comment;

            // ------ other single character
            case lex_detail::predef_pat_default: return lex_token_t(static_cast<std::uint8_t>(lexeme[0]));

            default: UXS_UNREACHABLE_CODE;
        }
    }

    return lex_token_t::eof;
}

template UXS_EXPORT basic_value<char> parser::read(std::string_view, const std::allocator<char>&);
template UXS_EXPORT basic_value<wchar_t> parser::read(std::string_view, const std::allocator<wchar_t>&);
template UXS_EXPORT void write(membuffer& out, const basic_value<char>&, std::string_view, xml_fmt_opts, unsigned);
template UXS_EXPORT void write(membuffer& out, const basic_value<wchar_t>&, std::wstring_view, xml_fmt_opts, unsigned);
template UXS_EXPORT void write(wmembuffer& out, const basic_value<char>&, std::string_view, xml_fmt_opts, unsigned);
template UXS_EXPORT void write(wmembuffer& out, const basic_value<wchar_t>&, std::wstring_view, xml_fmt_opts, unsigned);
}  // namespace xml
}  // namespace db
}  // namespace uxs
