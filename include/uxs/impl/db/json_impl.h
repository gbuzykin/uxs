#pragma once

#include "uxs/chars.h"
#include "uxs/db/json.h"
#include "uxs/dynarray.h"
#include "uxs/string_conv.h"

namespace uxs {
namespace db {
namespace json {

namespace lex_detail {
#include "json_lex_defs.h"
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
token_t lexer<CharT>::lex(std::basic_string_view<CharT>& lval) {
    std::uint32_t lower_surrogate = 0;
    std::size_t surrogate_pair_pos = 0;
    unsigned last_utf_code_length = 0;
    bool is_parsing_string = false;

    while (in.peek() != ibuf::traits_type::eof()) {
        using char_tbl_t = uxs::detail::char_tbl_t;
        std::int8_t state = lex_detail::sc_initial;

        if (!is_parsing_string) {
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
            if (state < 0) {  // process a single character
                in.advance(1);
                if (*curr != '\"') { return token_t(*curr); }
                is_parsing_string = true;
                continue;
            }
        } else {  // parse string
            const CharT* curr0 = in.curr();
            const CharT* curr = std::find_if(
                curr0, in.last(), [](CharT ch) { return char_tbl_t::has_bits(ch, char_tbl_t::bits::json_special); });

            in.setpos(curr - in.first());
            if (!in.avail()) {
                stash.append(curr0, curr);
                continue;
            }

            if (*curr == '\"') {
                if (stash.empty()) {
                    lval = to_string_view(curr0, curr);
                } else {
                    stash.append(curr0, curr);
                    lval = to_string_view(stash.data(), stash.size());
                    stash.clear();  // it resets the stash, but retains the contents
                }
                in.advance(1);
                return token_t::string;
            }

            if (*curr != '\\') { break; }

            stash.append(curr0, curr);

            // process '\\' character
            state = get_next_state(lex_detail::sc_string, '\\');
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
            // ------ escape sequences
            case lex_detail::pat_escape_quot: stash += '\"'; break;
            case lex_detail::pat_escape_rev_sol: stash += '\\'; break;
            case lex_detail::pat_escape_sol: stash += '/'; break;
            case lex_detail::pat_escape_b: stash += '\b'; break;
            case lex_detail::pat_escape_f: stash += '\f'; break;
            case lex_detail::pat_escape_n: stash += '\n'; break;
            case lex_detail::pat_escape_r: stash += '\r'; break;
            case lex_detail::pat_escape_t: stash += '\t'; break;
            case lex_detail::pat_escape_unicode: {
                std::uint32_t unicode = (dig_v{}(lexeme[2]) << 12) | (dig_v{}(lexeme[3]) << 8) |
                                        (dig_v{}(lexeme[4]) << 4) | dig_v{}(lexeme[5]);
                if (lower_surrogate != 0) {
                    if (is_utf_upper_surrogate(unicode) && stash.size() == surrogate_pair_pos + last_utf_code_length) {
                        unicode = combine_utf_surrogate_code(lower_surrogate, unicode);
                        stash.setsize(surrogate_pair_pos);
                    }
                    lower_surrogate = 0;
                } else if (is_utf_lower_surrogate(unicode)) {
                    lower_surrogate = unicode;
                    surrogate_pair_pos = stash.size();
                }
                stash.reserve(stash.size() + utf_codec<CharT>::max_code_length);
                last_utf_code_length = utf_codec<CharT>{}.encode(unicode, stash.endp()).count;
                stash.advance(last_utf_code_length);
            } break;

            // ------ values
            case lex_detail::pat_null: return token_t::null_value;
            case lex_detail::pat_true: return token_t::true_value;
            case lex_detail::pat_false: return token_t::false_value;
            case lex_detail::pat_decimal: {
                lval = to_string_view(lexeme, llen);
                return token_t::integer_number;
            } break;
            case lex_detail::pat_neg_decimal: {
                lval = to_string_view(lexeme, llen);
                return token_t::negative_integer_number;
            } break;
            case lex_detail::pat_real: {
                lval = to_string_view(lexeme, llen);
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
                bool star = false;
                while (true) {
                    const int ch = in.get();
                    if (ch == ibuf::traits_type::eof() || ch == 0) { report_error(ln, "unterminated C-style comment"); }
                    if (ch == '\n') { ++ln; }
                    if (star && ch == '/') { break; }
                    star = (ch == '*');
                }
            } break;

            default: UXS_UNREACHABLE_CODE;
        }
    }

    if (is_parsing_string) { report_error(ln, "unterminated string or unexpected string character"); }

    return token_t::eof;
}

}  // namespace detail

template<typename CharT, typename Alloc, typename InCharT>
basic_value<CharT, Alloc> parse(basic_ibuf<InCharT>& in, const Alloc& al) {
    static const auto token_to_value = [](token_t tt, std::basic_string_view<InCharT> val,
                                          const Alloc& al) -> basic_value<CharT, Alloc> {
        switch (tt) {
            case token_t::null_value: return {nullptr, al};
            case token_t::true_value: return {true, al};
            case token_t::false_value: return {false, al};
            case token_t::integer_number: {
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
            case token_t::negative_integer_number: {
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
            case token_t::floating_point_number: return {from_string<double>(val), al};
            case token_t::string: {
                return {string_tag, utf_string_adapter<CharT>{}.count(val.begin(), val.end()),
                        [val](est::span<CharT> s) {
                            utf_string_adapter<CharT>{}.transform(val.begin(), val.end(), s.data());
                            return s.size();
                        }};
            } break;
            default: UXS_UNREACHABLE_CODE;
        }
    };

    inline_dynarray<basic_value<CharT, Alloc>*, 64> stack;

    basic_value<CharT, Alloc> val(al);
    auto* item = &val;

    parse(
        in,
        [&stack, &item](token_t tt, std::basic_string_view<InCharT> val) {
            if (tt >= token_t::null_value) {
                *item = token_to_value(tt, val, item->get_allocator());
            } else {
                *item = tt == token_t::array ? basic_value<CharT, Alloc>(array_tag, item->get_allocator()) :
                                               basic_value<CharT, Alloc>(object_tag, item->get_allocator());
                stack.push_back(item);
            }
            return parse_step::into;
        },
        [&stack, &item]() { item = &stack.back()->emplace_back(item->get_allocator()); },
        [&stack, &item](std::basic_string_view<InCharT> val) {
            item = &stack.back()
                        ->emplace_fill_key(
                            utf_string_adapter<CharT>{}.count(val.begin(), val.end()),
                            [val](est::span<CharT> s) {
                                utf_string_adapter<CharT>{}.transform(val.begin(), val.end(), s.data());
                                return s.size();
                            },
                            item->get_allocator())
                        .value();
        },
        [&stack] { stack.pop_back(); });

    return val;
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

    bool is_object() const noexcept { return is_object_; }
    bool empty() const noexcept { return is_object_ ? obj_.first == obj_.last : arr_.first == arr_.last; }
    typename value_t::key_type key() const noexcept { return obj_.first->key(); }
    const value_t& get_and_advance() noexcept { return is_object_ ? (obj_.first++)->value() : *arr_.first++; }

 private:
    struct array_range_t {
        const value_t* first;
        const value_t* last;
    };
    struct object_range_t {
        object_iterator first;
        object_iterator last;
    };

    bool is_object_;
    union {
        array_range_t arr_;
        object_range_t obj_;
    };
};

template<typename OutCharT, typename CharT>
void write_text(basic_membuffer<OutCharT>& out, std::basic_string_view<CharT> text) {
    std::basic_string_view<OutCharT> special;
    std::array<OutCharT, 6> special_buf{'\\', 'u', '0', '0', '0', '0'};
    auto it0 = text.begin();
    out += '\"';
    for (auto it = it0; it != text.end(); ++it) {
        using char_tbl_t = uxs::detail::char_tbl_t;
        if (!char_tbl_t::has_bits(*it, char_tbl_t::bits::json_special)) { continue; }
        switch (*it) {
            case '\"': special = string_literal<OutCharT, '\\', '\"'>{}(); break;
            case '\\': special = string_literal<OutCharT, '\\', '\\'>{}(); break;
            case '\b': special = string_literal<OutCharT, '\\', 'b'>{}(); break;
            case '\f': special = string_literal<OutCharT, '\\', 'f'>{}(); break;
            case '\n': special = string_literal<OutCharT, '\\', 'n'>{}(); break;
            case '\r': special = string_literal<OutCharT, '\\', 'r'>{}(); break;
            case '\t': special = string_literal<OutCharT, '\\', 't'>{}(); break;
            default: {
                special_buf[4] = '0' + (*it >> 4);
                special_buf[5] = "0123456789ABCDEF"[*it & 15];
                special = to_string_view(special_buf.data(), special_buf.size());
            } break;
        }
        utf_string_adapter<OutCharT>{}.append(out, to_string_view(it0, it));
        out += special;
        it0 = it + 1;
    }
    utf_string_adapter<OutCharT>{}.append(out, to_string_view(it0, text.end()));
    out += '\"';
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
        if (r.empty()) {
            out += string_literal<char_type, '[', ']'>{}();
            return false;
        }
        stack.emplace_back(r.data(), r.data() + r.size());
        return true;
    }

    bool operator()(decltype(std::declval<ValueTy>().as_object()) r) const {
        if (r.empty()) {
            out += string_literal<char_type, '{', '}'>{}();
            return false;
        }
        stack.emplace_back(r.begin(), r.end());
        return true;
    }
};

template<typename CharT, typename Alloc, typename StrTy, typename StackTy>
value_visitor<const basic_value<CharT, Alloc>, StrTy, StackTy> make_value_visitor(StrTy& out, StackTy& stack) {
    return {out, stack};
}

template<typename OutCharT, typename CharT, typename Alloc>
void write_impl(basic_membuffer<OutCharT>& out, const basic_value<CharT, Alloc>& v) {
    inline_dynarray<detail::writer_stack_item_t<CharT, Alloc>, 64> stack;

    const auto visitor = detail::make_value_visitor<CharT, Alloc>(out, stack);
    if (!v.visit(visitor)) { return; }

    bool is_first_element = true;

loop:
    auto& top = stack.back();

    if (top.is_object()) {
        while (!top.empty()) {
            out += is_first_element ? '{' : ',';
            detail::write_text<OutCharT>(out, top.key());
            out += ':';
            if (top.get_and_advance().visit(visitor)) {
                is_first_element = true;
                goto loop;
            }
            is_first_element = false;
        }
        out += '}';
    } else {
        while (!top.empty()) {
            out += is_first_element ? '[' : ',';
            if (top.get_and_advance().visit(visitor)) {
                is_first_element = true;
                goto loop;
            }
            is_first_element = false;
        }
        out += ']';
    }

    stack.pop_back();
    if (!stack.empty()) { goto loop; }
}

template<typename OutCharT, typename CharT, typename Alloc>
void write_formatted_impl(basic_membuffer<OutCharT>& out, const basic_value<CharT, Alloc>& v, json_fmt_opts opts,
                          unsigned indent) {
    inline_dynarray<detail::writer_stack_item_t<CharT, Alloc>, 64> stack;

    const auto visitor = detail::make_value_visitor<CharT, Alloc>(out, stack);
    if (!v.visit(visitor)) { return; }

    bool is_first_element = true;

loop:
    auto& top = stack.back();
    const OutCharT ws_char = top.is_object() ? opts.object_ws_char : opts.array_ws_char;

    while (!top.empty()) {
        if (is_first_element) {
            out += top.is_object() ? '{' : '[';
            if (ws_char == '\n') {
                out += '\n';
                indent += opts.indent_size;
                out.append(indent, opts.indent_char);
            }
        } else {
            out += ',';
            out += ws_char;
            if (ws_char == '\n') { out.append(indent, opts.indent_char); }
        }
        if (top.is_object()) {
            detail::write_text<OutCharT>(out, top.key());
            out += string_literal<OutCharT, ':', ' '>{}();
        }
        if (top.get_and_advance().visit(visitor)) {
            is_first_element = true;
            goto loop;
        }
        is_first_element = false;
    }

    if (ws_char == '\n') {
        out += '\n';
        indent -= opts.indent_size;
        out.append(indent, opts.indent_char);
    }
    out += top.is_object() ? '}' : ']';

    stack.pop_back();
    if (!stack.empty()) { goto loop; }
}

}  // namespace detail
}  // namespace json
}  // namespace db
}  // namespace uxs
