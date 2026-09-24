#pragma once

#include "uxs/chars.h"
#include "uxs/db/xml.h"
#include "uxs/dynarray.h"
#include "uxs/string_conv.h"

namespace uxs {
namespace db {
namespace xml {

namespace lex_detail {
#include "xml_lex_defs.h"
extern std::uint8_t symb2meta[];
extern std::int8_t Dtran[];
extern int accept[];
}  // namespace lex_detail

namespace detail {

template<typename CharT>
UXS_FORCE_INLINE std::uint32_t parse_uint32(const CharT* p, const CharT* end) noexcept {
    std::uint32_t result = static_cast<unsigned>(*p - '0');
    while (++p != end) { result = 10U * result + static_cast<unsigned>(*p - '0'); }
    return result;
}

template<typename CharT>
UXS_FORCE_INLINE std::pair<std::uint64_t, bool> parse_uint64(const CharT* p, const CharT* end) noexcept {
    std::uint64_t result = static_cast<unsigned>(*p - '0');
    while (++p != end) {
        std::uint64_t result0 = result;
        result = 10U * result + static_cast<unsigned>(*p - '0');
        if (result < result0) { return {0, false}; }
    }
    return {result, true};
}

template<typename CharT>
UXS_FORCE_INLINE bool is_equal_to_string_nocase(std::basic_string_view<CharT> s, std::basic_string_view<CharT> ref) {
    if (s.size() != ref.size()) { return false; }
    return std::equal(s.begin(), s.end(), ref.begin(), [](CharT l, CharT r) { return to_lower{}(l) == r; });
}

template<typename CharT>
UXS_FORCE_INLINE std::int8_t get_next_state_dispatch(int state, CharT ch, std::true_type /* one byte */) noexcept {
    return lex_detail::Dtran[lex_detail::dtran_width * state + lex_detail::symb2meta[static_cast<std::uint8_t>(ch)]];
}

template<typename CharT>
UXS_FORCE_INLINE std::int8_t get_next_state_dispatch(int state, CharT ch, std::false_type /* one byte */) noexcept {
    if ((ch & 0xff) != ch) { return -1; }
    return lex_detail::Dtran[lex_detail::dtran_width * state + lex_detail::symb2meta[static_cast<std::uint8_t>(ch)]];
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_FORCE_INLINE std::int8_t get_next_state(int state, CharT ch) noexcept {
    return get_next_state_dispatch(state, ch, std::bool_constant<sizeof(CharT) == 1>());
}

template<typename CharT>
CharT* normalize_string(CharT* dst, const CharT* first, const CharT* last) {
    CharT* dst0 = dst;
    for (; first != last; ++dst) {
        if (*first == '\n') {
            using char_tbl_t = uxs::detail::char_tbl_t;
            while (dst != dst0 && char_tbl_t::has_bits(*(dst - 1), char_tbl_t::bits::json_ws)) { --dst; }
            while (++first != last && char_tbl_t::has_bits(*first, char_tbl_t::bits::json_ws)) {}
            *dst = ' ';
        } else {
            *dst = *first++;
        }
    }
    return dst;
}

template<typename CharT>
lex_token_t lexer<CharT>::lex(std::basic_string_view<CharT>& lval) {
    CharT string_quot = '\0';
    bool need_to_normalize_string = false;

    while (in.peek() != ibuf::traits_type::eof()) {
        using char_tbl_t = uxs::detail::char_tbl_t;
        std::int8_t state = 0;

        if (!string_quot) {
            const CharT* curr = in.curr();
            if (char_tbl_t::has_bits(*curr, char_tbl_t::bits::json_ws)) {  // skip whitespaces
                if (*curr == '\n') { ++ln; }
                curr = std::find_if(curr + 1, in.last(), [this](CharT ch) {
                    if (ch != '\n') { return !char_tbl_t::has_bits(ch, char_tbl_t::bits::json_ws); }
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
                if (*curr != '\"' && *curr != '\'') { return lex_token_t(*curr); }
                string_quot = *curr;
                continue;
            }
        } else {  // parse string
            const CharT* curr0 = in.curr();
            const CharT* curr = std::find_if(curr0, in.last(), [this, &need_to_normalize_string, string_quot](CharT ch) {
                if (ch != '\n') { return char_tbl_t::has_bits(ch, char_tbl_t::bits::xml_special) || ch == string_quot; }
                ++ln;
                need_to_normalize_string = true;
                return false;
            });

            in.setpos(curr - in.first());
            if (!in.avail()) {
                stash.append(curr0, curr);
                continue;
            }

            if (*curr == string_quot) {
                if (stash.empty()) {
                    lval = to_string_view(curr0, curr);
                } else {
                    stash.append(curr0, curr);
                    lval = to_string_view(stash.data(), stash.size());
                    stash.clear();  // it resets the stash, but retains the contents
                }
                if (need_to_normalize_string) {
                    stash.reserve(lval.size());
                    CharT* endp = normalize_string(stash.data(), lval.data(), lval.data() + lval.size());
                    lval = to_string_view(stash.data(), endp);
                }
                in.advance(1);
                return lex_token_t::string;
            }

            stash.append(curr0, curr);

            if (*curr != '&') { break; }

            // process '&' character
            state = get_next_state(lex_detail::sc_initial, '&');
        }

        int pat = 0;

        // accept the first character
        std::size_t llen = 1;
        std::size_t stash_sz0 = stash.size();
        const CharT* first0 = in.curr() + 1;

        while (true) {
            const CharT* first = first0;
            while (first != in.last()) {
                const std::int8_t next_state = get_next_state(state, *first);
                if (next_state < 0) { break; }
                state = next_state, ++first;
            }

            llen += static_cast<std::size_t>(first - first0);

            if (first != in.last() || !in) {
                pat = lex_detail::accept[state];
                if (pat <= 0) { report_error(ln, "invalid token or escape sequence"); }
                break;
            }

            // append lexeme in stash
            stash.append(in.curr(), in.last());
            in.setpos(in.capacity());
            // read more characters from input
            in.peek();
            first0 = in.curr();
        }

        const CharT* lexeme = in.curr();
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
                if (!string_quot) {
                    lval = string_literal<CharT, '&'>{}();
                    return lex_token_t::predef_entity;
                }
                stash += '&';
            } break;
            case lex_detail::pat_lt: {
                if (!string_quot) {
                    lval = string_literal<CharT, '<'>{}();
                    return lex_token_t::predef_entity;
                }
                stash += '<';
            } break;
            case lex_detail::pat_gt: {
                if (!string_quot) {
                    lval = string_literal<CharT, '>'>{}();
                    return lex_token_t::predef_entity;
                }
                stash += '>';
            } break;
            case lex_detail::pat_apos: {
                if (!string_quot) {
                    lval = string_literal<CharT, '\''>{}();
                    return lex_token_t::predef_entity;
                }
                stash += '\'';
            } break;
            case lex_detail::pat_quot: {
                if (!string_quot) {
                    lval = string_literal<CharT, '\"'>{}();
                    return lex_token_t::predef_entity;
                }
                stash += '\"';
            } break;
            case lex_detail::pat_entity: {
                if (!string_quot) {
                    lval = to_string_view(lexeme + 1, llen - 2);
                    return lex_token_t::entity;
                }
                report_error(ln, "unknown entity name");
            } break;
            case lex_detail::pat_dcode:
            case lex_detail::pat_hcode: {
                std::uint32_t unicode = 0;
                if (pat == lex_detail::pat_dcode) {
                    for (const CharT ch : to_string_view(lexeme + 2, llen - 3)) {
                        unicode = 10 * unicode + dig_v{}(ch);
                    }
                } else {
                    for (const CharT ch : to_string_view(lexeme + 3, llen - 4)) {
                        unicode = (unicode << 4) + dig_v{}(ch);
                    }
                }
                stash.reserve(stash.size() + utf_codec<CharT>::max_code_length);
                const std::size_t length = utf_codec<CharT>{}.encode(unicode, stash.endp()).count;
                if (!string_quot) {
                    lval = to_string_view(stash.endp(), length);
                    return lex_token_t::predef_entity;
                }
                stash.advance(length);
            } break;

            // ------ tags
            case lex_detail::pat_name: {
                lval = to_string_view(lexeme, llen);
                return lex_token_t::name;
            } break;
            case lex_detail::pat_start_element_open: {
                lval = to_string_view(lexeme + 1, llen - 1);
                return lex_token_t::start_element_open;
            } break;
            case lex_detail::pat_end_element_open: {
                lval = to_string_view(lexeme + 2, llen - 2);
                return lex_token_t::end_element_open;
            } break;
            case lex_detail::pat_pi_open: {
                lval = to_string_view(lexeme + 2, llen - 2);
                return lex_token_t::pi_open;
            } break;
            case lex_detail::pat_end_element_close: return lex_token_t::end_element_close;
            case lex_detail::pat_pi_close: return lex_token_t::pi_close;

            // ------ comment
            case lex_detail::pat_comment: return lex_token_t::comment;

            default: UXS_UNREACHABLE_CODE;
        }
    }

    if (string_quot) { report_error(ln, "unterminated string or unexpected string character"); }

    return lex_token_t::eof;
}

}  // namespace detail

template<typename CharT>
auto parser<CharT>::next_impl() -> std::pair<token_t, string_view_type> {
    if (is_end_element_pending_) {
        is_end_element_pending_ = false;
        return {token_t::end_element, str_cache_[0].as_string_view()};
    }

    while (lexer_.in.peek() != ibuf::traits_type::eof()) {
        string_view_type lval;
        const CharT* curr0 = lexer_.in.curr();

        if (*curr0 == '<') {
            std::size_t str_cache_idx = 1;

            attrs_.clear();

            const auto parse_attribute = [this, &str_cache_idx](string_view_type lval) {
                auto& item = attrs_.emplace(lval).value();

                if (lexer_.lex(lval) != detail::lex_token_t::eq) { detail::report_error(lexer_.ln, "expected `=`"); }
                if (lexer_.lex(lval) != detail::lex_token_t::string) {
                    detail::report_error(lexer_.ln, "expected valid attribute value");
                }

                if (str_cache_idx < str_cache_.size()) {
                    str_cache_[str_cache_idx] = lval;
                } else {
                    str_cache_.emplace_back(lval);
                }
                item = str_cache_[str_cache_idx++];
            };

            switch (lexer_.lex(lval)) {
                case detail::lex_token_t::start_element_open: {  // <name n1=v1 n2=v2...> or <name n1=v1 n2=v2.../>
                    str_cache_[0] = lval;
                    while (true) {
                        auto tt = lexer_.lex(lval);
                        if (tt == detail::lex_token_t::name) {
                            parse_attribute(lval);
                        } else if (tt == detail::lex_token_t::close) {
                            return {token_t::start_element, str_cache_[0].as_string_view()};
                        } else if (tt == detail::lex_token_t::end_element_close) {
                            is_end_element_pending_ = true;
                            return {token_t::start_element, str_cache_[0].as_string_view()};
                        } else {
                            detail::report_error(lexer_.ln, "expected name, `>` or `/>`");
                        }
                    }
                } break;

                case detail::lex_token_t::end_element_open: {  // </name>
                    if (lexer_.lex(lval) != detail::lex_token_t::close) {
                        detail::report_error(lexer_.ln, "expected `>`");
                    }
                    return {token_t::end_element, lval};
                } break;

                case detail::lex_token_t::pi_open: {  // <?xml n1=v1 n2=v2...?>
                    if (!detail::is_equal_to_string_nocase(lval, string_literal<CharT, 'x', 'm', 'l'>{}())) {
                        detail::report_error(lexer_.ln, "invalid document declaration");
                    }
                    str_cache_[0] = lval;
                    while (true) {
                        auto tt = lexer_.lex(lval);
                        if (tt == detail::lex_token_t::name) {
                            parse_attribute(lval);
                        } else if (tt == detail::lex_token_t::pi_close) {
                            return {token_t::preamble, str_cache_[0].as_string_view()};
                        } else {
                            detail::report_error(lexer_.ln, "expected name or `?>`");
                        }
                    }
                } break;

                case detail::lex_token_t::comment: {  // comment <!--....-->
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
        } else if (*curr0 == '&') {
            if (lexer_.lex(lval) == detail::lex_token_t::predef_entity) { return {token_t::plain_text, lval}; }
            return {token_t::entity, lval};
        } else if (*curr0 == '\0') {
            return {token_t::eof, {}};
        } else {
            using char_tbl_t = uxs::detail::char_tbl_t;
            if (*curr0 == '\n') { ++lexer_.ln; }
            const CharT* curr = std::find_if(curr0 + 1, lexer_.in.last(), [this](CharT ch) {
                if (ch != '\n') { return char_tbl_t::has_bits(ch, char_tbl_t::bits::xml_special); }
                ++lexer_.ln;
                return false;
            });
            lexer_.in.setpos(curr - lexer_.in.first());
            return {token_t::plain_text, to_string_view(curr0, curr)};
        }
    }

    return {token_t::eof, {}};
}

template<typename CharT>
value_class parser<CharT>::classify_value(const string_view_type& val) noexcept {
    std::int8_t state = lex_detail::sc_value;
    for (const CharT ch : val) { state = detail::get_next_state(state, ch); }
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

template<typename InCharT>
template<typename CharT, typename Alloc>
basic_value<CharT, Alloc> parser<InCharT>::parse(string_view_type root_element, const Alloc& al) {
    static const auto text_to_value = [](string_view_type val, const Alloc& al) -> basic_value<CharT, Alloc> {
        switch (classify_value(val)) {
            case value_class::empty:
            case value_class::null_value: return {nullptr, al};
            case value_class::true_value: return {true, al};
            case value_class::false_value: return {false, al};
            case value_class::integer_number: {
                if (val.size() <= 9) {
                    const std::uint32_t i = detail::parse_uint32(val.data(), val.data() + val.size());
                    return {static_cast<std::int32_t>(i), al};
                }
                const auto result = detail::parse_uint64(val.data(), val.data() + val.size());
                if (result.second) {
                    if (result.first <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
                        return {static_cast<std::int32_t>(result.first), al};
                    }
                    if (result.first <= std::numeric_limits<std::uint32_t>::max()) {
                        return {static_cast<std::uint32_t>(result.first), al};
                    }
                    if (result.first <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                        return {static_cast<std::int64_t>(result.first), al};
                    }
                    return {result.first, al};
                }
                // too big integer - treat as double
                return {from_string<double>(val), al};
            } break;
            case value_class::negative_integer_number: {
                if (val.size() <= 10) {
                    const std::uint32_t u = detail::parse_uint32(val.data() + 1, val.data() + val.size());
                    return {static_cast<std::int32_t>(~u + 1), al};
                }
                const auto result = detail::parse_uint64(val.data() + 1, val.data() + val.size());
                if (result.second) {
                    if (result.first <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) + 1) {
                        return {static_cast<std::int32_t>(~result.first + 1), al};
                    }
                    if (result.first <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1) {
                        return {static_cast<std::int64_t>(~result.first + 1), al};
                    }
                }
                // too big integer - treat as double
                return {from_string<double>(val), al};
            } break;
            case value_class::floating_point_number: return {from_string<double>(val), al};
            case value_class::ws_with_nl: return basic_value<CharT, Alloc>(object_tag, al);
            case value_class::other: return utf_string_adapter<CharT>{}(val);
            default: UXS_UNREACHABLE_CODE;
        }
    };

    auto tt = token_type();
    while (!eof() && !(tt == token_t::start_element && name() == root_element)) { tt = next(); }
    if (eof()) { throw database_error("no such element"); }

    basic_inline_dynbuffer<InCharT> txt;
    inline_dynarray<std::pair<basic_value<CharT, Alloc>*, std::basic_string<InCharT>>, 64> stack;

    basic_value<CharT, Alloc> val(al);
    stack.emplace_back(&val, root_element);

    tt = next();

    while (true) {
        auto& top = stack.back();
        switch (tt) {
            case token_t::eof: detail::report_error(lexer_.ln, "unexpected end of file");
            case token_t::preamble: detail::report_error(lexer_.ln, "unexpected document preamble");
            case token_t::entity: detail::report_error(lexer_.ln, "unknown entity name");
            case token_t::plain_text: {
                if (!top.first->is_object()) { txt += text(); }
            } break;
            case token_t::start_element: {
                txt.clear();
                auto result = top.first->emplace_unique(utf_string_adapter<CharT>{}(name()), al);
                stack.emplace_back(&result.first.value(), name());
                if (!result.second) { stack.back().first = &result.first.value().emplace_back(al); }
                for (const auto& attr : attributes()) {
                    stack.back().first->emplace_unique(utf_string_adapter<CharT>{}(attr.key()),
                                                       text_to_value(attr.value().as_string_view(), al));
                }
            } break;
            case token_t::end_element: {
                if (top.second != name()) { detail::report_error(lexer_.ln, "unexpected end element"); }
                if (!top.first->is_object() && !txt.empty()) {
                    *(top.first) = text_to_value(string_view_type(txt.data(), txt.size()), al);
                }
                stack.pop_back();
                if (stack.empty()) { return val; }
            } break;
            default: break;
        }
        tt = next();
    }
}

// --------------------------

namespace detail {

template<typename CharT, typename Alloc>
struct writer_stack_item_t {
 public:
    using value_t = basic_value<CharT, Alloc>;
    using object_iterator = typename value_t::const_object_iterator;

    writer_stack_item_t(const value_t* first, const value_t* last) noexcept : is_object_(false), arr_{first, last} {}
    writer_stack_item_t(object_iterator first, object_iterator last) noexcept : is_object_(true), obj_{first, last} {}

    std::basic_string_view<CharT> element() const noexcept { return element_; }
    void set_element(std::basic_string_view<CharT> element) noexcept { element_ = element; }

    bool is_object() const noexcept { return is_object_; }
    bool empty() const noexcept { return is_object_ ? obj_.first == obj_.last : arr_.first == arr_.last; }
    typename value_t::key_type key() const noexcept { return obj_.first->key(); }
    const value_t& get_and_advance() noexcept { return is_object_ ? (obj_.first++)->value() : *arr_.first++; }
    const value_t& prev() const noexcept {
        return is_object_ ? std::prev(obj_.first)->value() : *std::prev(arr_.first);
    }

 private:
    struct array_range_t {
        const value_t* first;
        const value_t* last;
    };
    struct object_range_t {
        object_iterator first;
        object_iterator last;
    };

    std::basic_string_view<CharT> element_;
    bool is_object_;
    union {
        array_range_t arr_;
        object_range_t obj_;
    };
};

template<typename OutCharT, typename CharT>
void write_text(basic_membuffer<OutCharT>& out, std::basic_string_view<CharT> text) {
    std::basic_string_view<OutCharT> special;
    auto it0 = text.begin();
    for (auto it = it0; it != text.end(); ++it) {
        using char_tbl_t = uxs::detail::char_tbl_t;
        if (!char_tbl_t::has_bits(*it, char_tbl_t::bits::xml_special) && *it != '>') { continue; }
        switch (*it) {
            case '&': special = string_literal<OutCharT, '&', 'a', 'm', 'p', ';'>{}(); break;
            case '<': special = string_literal<OutCharT, '&', 'l', 't', ';'>{}(); break;
            case '>': special = string_literal<OutCharT, '&', 'g', 't', ';'>{}(); break;
            case '\0': special = string_literal<OutCharT, '&', '#', '0', ';'>{}(); break;
        }
        utf_string_adapter<OutCharT>{}.append(out, to_string_view(it0, it));
        out += special;
        it0 = it + 1;
    }
    utf_string_adapter<OutCharT>{}.append(out, to_string_view(it0, text.end()));
}

template<typename ValueTy, typename StrTy, typename StackTy>
struct value_visitor {
    using char_type = typename StrTy::value_type;

    StrTy& out;
    StackTy& stack;

    value_visitor(StrTy& out, StackTy& stack) : out(out), stack(stack) {}

    template<typename Ty>
    bool operator()(Ty v) const {
        sconv::fmt_integer(out, v);
        return false;
    }

    bool operator()(double f) const {
        sconv::fmt_float(out, f, fmt_flags::mandatory_frac | fmt_flags::throw_on_inf_nan);
        return false;
    }

    bool operator()(std::nullptr_t) const {
        out += string_literal<char_type, 'n', 'u', 'l', 'l'>{}();
        return false;
    }

    bool operator()(bool b) const {
        out += b ? string_literal<char_type, 't', 'r', 'u', 'e'>{}() :
                   string_literal<char_type, 'f', 'a', 'l', 's', 'e'>{}();
        return false;
    }

    bool operator()(decltype(std::declval<ValueTy>().as_string_view()) s) const {
        detail::write_text<char_type>(out, s);
        return false;
    }

    bool operator()(decltype(std::declval<ValueTy>().as_array()) r) const {
        stack.emplace_back(r.data(), r.data() + r.size());
        return true;
    }

    bool operator()(decltype(std::declval<ValueTy>().as_object()) r) const {
        stack.emplace_back(r.begin(), r.end());
        return true;
    }
};

template<typename CharT, typename Alloc, typename StrTy, typename StackTy>
value_visitor<const basic_value<CharT, Alloc>, StrTy, StackTy> make_value_visitor(StrTy& out, StackTy& stack) {
    return {out, stack};
}

template<typename OutCharT, typename CharT, typename Alloc>
void write_impl(basic_membuffer<OutCharT>& out, const basic_value<CharT, Alloc>& v,
                std::basic_string_view<CharT> element, xml_fmt_opts opts, unsigned indent) {
    inline_dynarray<detail::writer_stack_item_t<CharT, Alloc>, 64> stack;

    const auto visitor = detail::make_value_visitor<CharT, Alloc>(out, stack);

    out += '<';
    utf_string_adapter<OutCharT>{}.append(out, element);
    out += '>';
    if (!v.visit(visitor)) {
        out += string_literal<OutCharT, '<', '/'>{}();
        utf_string_adapter<OutCharT>{}.append(out, element);
        out += '>';
        return;
    }

    bool is_first_element = true;
    stack.back().set_element(element);

loop:
    auto& top = stack.back();

    if (is_first_element && top.is_object()) { indent += opts.indent_size; }

    while (true) {
        if (!is_first_element && !top.prev().is_array()) {
            out += string_literal<OutCharT, '<', '/'>{}();
            utf_string_adapter<OutCharT>{}.append(out, element);
            out += '>';
        }
        if (top.empty()) { break; }
        if (top.is_object()) { element = top.key(); }
        const auto& value = top.get_and_advance();
        if (!value.is_array()) {
            out += '\n';
            out.append(indent, opts.indent_char);
            out += '<';
            utf_string_adapter<OutCharT>{}.append(out, element);
            out += '>';
        }
        if (value.visit(visitor)) {
            is_first_element = true;
            stack.back().set_element(element);
            goto loop;
        }
        is_first_element = false;
    }

    if (top.is_object()) {
        indent -= opts.indent_size;
        out += '\n';
        out.append(indent, opts.indent_char);
    }

    is_first_element = false;
    element = top.element();
    stack.pop_back();
    if (!stack.empty()) { goto loop; }

    out += string_literal<OutCharT, '<', '/'>{}();
    utf_string_adapter<OutCharT>{}.append(out, element);
    out += '>';
}

}  // namespace detail
}  // namespace xml
}  // namespace db
}  // namespace uxs
