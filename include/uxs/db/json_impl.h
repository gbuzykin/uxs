#pragma once

#include "json.h"
#include "value.h"

#include "uxs/string_alg.h"

namespace uxs {
namespace db {
namespace json {

// --------------------------

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc> read(ibuf& in, const Alloc& al) {
    auto token_to_value = [](token_t tt, std::string_view lval, const Alloc& al) -> basic_value<CharT, Alloc> {
        switch (tt) {
            case token_t::null_value: return {nullptr, al};
            case token_t::true_value: return {true, al};
            case token_t::false_value: return {false, al};
            case token_t::integer_number: {
                std::uint64_t u64 = 0;
                if (stoval(lval, u64) != 0) {
                    if (u64 <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
                        return {static_cast<std::int32_t>(u64), al};
                    } else if (u64 <= static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
                        return {static_cast<std::uint32_t>(u64), al};
                    } else if (u64 <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                        return {static_cast<std::int64_t>(u64), al};
                    }
                    return {u64, al};
                }
                // too big integer - treat as double
                return {from_string<double>(lval), al};
            } break;
            case token_t::negative_integer_number: {
                std::int64_t i64 = 0;
                if (stoval(lval, i64) != 0) {
                    if (i64 >= static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min())) {
                        return {static_cast<std::int32_t>(i64), al};
                    }
                    return {i64, al};
                }
                // too big integer - treat as double
                return {from_string<double>(lval), al};
            } break;
            case token_t::floating_point_number: return {from_string<double>(lval), al};
            case token_t::string: return {utf_string_adapter<CharT>{}(lval), al};
            default: UXS_UNREACHABLE_CODE;
        }
    };

    basic_value<CharT, Alloc> result;
    inline_basic_dynbuffer<basic_value<CharT, Alloc>*, 32> stack;

    auto* val = &result;
    stack.push_back(val);
    read(
        in,
        [&al, &stack, &val, &token_to_value](token_t tt, std::string_view lval) {
            if (tt >= token_t::null_value) {
                *val = token_to_value(tt, lval, al);
            } else {
                *val = tt == token_t::array ? make_array<CharT>(al) : make_record<CharT>(al);
                stack.push_back(val);
            }
            return parse_step::into;
        },
        [&al, &stack, &val]() { val = &stack.back()->emplace_back(al); },
        [&al, &stack, &val](std::string_view lval) {
            val = &stack.back()->emplace(utf_string_adapter<CharT>{}(lval), al).value();
        },
        [&stack] { stack.pop_back(); });
    return result;
}

// --------------------------

namespace detail {

template<typename ValueCharT, typename Alloc>
struct writer_stack_item_t {
    using value_t = basic_value<ValueCharT, Alloc>;
    using iterator = typename value_t::const_iterator;
    writer_stack_item_t() = default;
    writer_stack_item_t(iterator f, iterator l) : first(f), last(l) {}
    iterator first;
    iterator last;
};

template<typename CharT>
basic_membuffer<CharT>& write_text(basic_membuffer<CharT>& out, std::basic_string_view<CharT> text) {
    auto it0 = text.begin();
    out += '\"';
    for (auto it = it0; it != text.end(); ++it) {
        char esc = '\0';
        switch (*it) {
            case '\"': esc = '\"'; break;
            case '\\': esc = '\\'; break;
            case '\b': esc = 'b'; break;
            case '\f': esc = 'f'; break;
            case '\n': esc = 'n'; break;
            case '\r': esc = 'r'; break;
            case '\t': esc = 't'; break;
            default: {
                if (static_cast<unsigned char>(*it) < 32) {
                    out += to_string_view(it0, it);
                    out += string_literal<CharT, '\\', 'u', '0', '0'>{}();
                    out += '0' + (*it >> 4);
                    out += "0123456789ABCDEF"[*it & 15];
                    it0 = it + 1;
                }
                continue;
            } break;
        }
        out += to_string_view(it0, it);
        out += '\\';
        out += esc;
        it0 = it + 1;
    }
    out += to_string_view(it0, text.end());
    out += '\"';
    return out;
}

template<typename StrTy, typename StackTy>
struct value_visitor {
    using char_type = typename StrTy::value_type;

    StrTy& out;
    StackTy& stack;

    value_visitor(StrTy& out, StackTy& stack) : out(out), stack(stack) {}

    template<typename Ty>
    bool operator()(Ty v) const {
        to_basic_string(out, v, fmt_opts{fmt_flags::json_compat});
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

    template<typename CharT>
    bool operator()(std::basic_string_view<CharT> s) const {
        detail::write_text<char_type>(out, utf_string_adapter<char_type>{}(s));
        return false;
    }

    template<typename Iter>
    bool operator()(iterator_range<Iter> r) const {
        if (r.empty()) {
            out += r.begin().is_record() ? string_literal<char_type, '{', '}'>{}() :
                                           string_literal<char_type, '[', ']'>{}();
            return false;
        }
        stack.emplace_back(r.begin(), r.end());
        return true;
    }
};

template<typename StrTy, typename StackTy>
value_visitor<StrTy, StackTy> make_value_visitor(StrTy& out, StackTy& stack) {
    return {out, stack};
}

template<typename CharT>
template<typename ValueCharT, typename Alloc>
void writer<CharT>::do_write(const basic_value<ValueCharT, Alloc>& v, unsigned indent) {
    using stack_item_t = detail::writer_stack_item_t<ValueCharT, Alloc>;
    inline_basic_dynbuffer<stack_item_t, 32> stack;

    auto visitor = make_value_visitor(out, stack);
    if (!v.visit(visitor)) { return; }

    bool is_first_element = true;

loop:
    auto& top = stack.back();
    const char ws_char = top.first.is_record() ? object_ws_char : array_ws_char;

    if (is_first_element) {
        out += top.first.is_record() ? '{' : '[';
        if (ws_char == '\n') {
            indent += indent_size;
            out += '\n';
            out.append(indent, indent_char);
        }
    }

    while (top.first != top.last) {
        if (!is_first_element) {
            out += ',';
            out += ws_char;
            if (ws_char == '\n') { out.append(indent, indent_char); }
        }
        if (top.first.is_record()) {
            detail::write_text<CharT>(out, utf_string_adapter<CharT>{}(top.first.key()));
            out += string_literal<CharT, ':', ' '>{}();
        }
        if ((top.first++).value().visit(visitor)) {
            is_first_element = true;
            goto loop;
        }
        is_first_element = false;
    }

    if (ws_char == '\n') {
        indent -= indent_size;
        out += '\n';
        out.append(indent, indent_char);
    }
    out += top.first.is_record() ? '}' : ']';

    stack.pop_back();
    if (!stack.empty()) { goto loop; }
}

}  // namespace detail

}  // namespace json
}  // namespace db
}  // namespace uxs
