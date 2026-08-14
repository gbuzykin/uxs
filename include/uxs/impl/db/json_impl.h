#pragma once

#include "uxs/db/json.h"
#include "uxs/dynarray.h"

namespace uxs {
namespace db {
namespace json {

namespace detail {
inline std::uint32_t parse_uint32(const char* p, const char* end) noexcept {
    std::uint32_t result = static_cast<unsigned>(*p - '0');
    while (++p != end) { result = 10U * result + static_cast<unsigned>(*p - '0'); }
    return result;
}
inline std::pair<std::uint64_t, bool> parse_uint64(const char* p, const char* end) noexcept {
    std::uint64_t result = static_cast<unsigned>(*p - '0');
    while (++p != end) {
        std::uint64_t result0 = result;
        result = 10U * result + static_cast<unsigned>(*p - '0');
        if (result < result0) { return {0, false}; }
    }
    return {result, true};
}
}  // namespace detail

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc> read(ibuf& in, const Alloc& al) {
    static const auto token_to_value = [](token_t tt, std::string_view lval,
                                          const Alloc& al) -> basic_value<CharT, Alloc> {
        switch (tt) {
            case token_t::null_value: return {nullptr, al};
            case token_t::true_value: return {true, al};
            case token_t::false_value: return {false, al};
            case token_t::integer_number: {
                if (lval.size() <= 9) {
                    const std::uint32_t val = detail::parse_uint32(lval.data(), lval.data() + lval.size());
                    return {static_cast<std::int32_t>(val), al};
                }
                const auto result = detail::parse_uint64(lval.data(), lval.data() + lval.size());
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
                return {from_string<double>(lval), al};
            } break;
            case token_t::negative_integer_number: {
                if (lval.size() <= 10) {
                    const std::uint32_t val = detail::parse_uint32(lval.data() + 1, lval.data() + lval.size());
                    return {static_cast<std::int32_t>(~val + 1), al};
                }
                const auto result = detail::parse_uint64(lval.data() + 1, lval.data() + lval.size());
                if (result.second) {
                    if (result.first <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) + 1) {
                        return {static_cast<std::int32_t>(~result.first + 1), al};
                    }
                    if (result.first <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1) {
                        return {static_cast<std::int64_t>(~result.first + 1), al};
                    }
                }
                // too big integer - treat as double
                return {from_string<double>(lval), al};
            } break;
            case token_t::floating_point_number: return {from_string<double>(lval), al};
            case token_t::string: return {utf_string_adapter<CharT>{}(lval), al};
            default: UXS_UNREACHABLE_CODE;
        }
    };

    inline_dynarray<basic_value<CharT, Alloc>*, 32> stack;

    basic_value<CharT, Alloc> val(al);
    auto* item = &val;

    read(
        in,
        [&stack, &item](token_t tt, std::string_view lval) {
            if (tt >= token_t::null_value) {
                *item = token_to_value(tt, lval, item->get_allocator());
            } else {
                *item = tt == token_t::array ? make_array<CharT>(item->get_allocator()) :
                                               make_record<CharT>(item->get_allocator());
                stack.push_back(item);
            }
            return parse_step::into;
        },
        [&stack, &item]() { item = &stack.back()->emplace_back(item->get_allocator()); },
        [&stack, &item](std::string_view lval) {
            item = &stack.back()->emplace(utf_string_adapter<CharT>{}(lval), item->get_allocator()).value();
        },
        [&stack] { stack.pop_back(); });

    return val;
}

// --------------------------

namespace detail {

template<typename ValueCharT, typename Alloc>
struct writer_stack_item_t {
 public:
    using value_t = basic_value<ValueCharT, Alloc>;
    using record_iterator = typename value_t::const_record_iterator;

    writer_stack_item_t(const value_t* first, const value_t* last) noexcept : is_record_(false), arr_{first, last} {}
    writer_stack_item_t(record_iterator first, record_iterator last) noexcept : is_record_(true), rec_{first, last} {}

    bool is_record() const noexcept { return is_record_; }
    bool empty() const noexcept { return is_record_ ? rec_.first == rec_.last : arr_.first == arr_.last; }
    typename value_t::key_type key() const noexcept { return rec_.first->key(); }
    const value_t& get_and_advance() noexcept { return is_record_ ? (rec_.first++)->value() : *arr_.first++; }

 private:
    struct array_range_t {
        const value_t* first;
        const value_t* last;
    };
    struct record_range_t {
        record_iterator first;
        record_iterator last;
    };

    bool is_record_;
    union {
        array_range_t arr_;
        record_range_t rec_;
    };
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
        sconv::fmt_float(out, f, fmt_flags::mandatory_frac);
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
        detail::write_text<char_type>(out, utf_string_adapter<char_type>{}(s));
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

    bool operator()(decltype(std::declval<ValueTy>().as_record()) r) const {
        if (r.empty()) {
            out += string_literal<char_type, '{', '}'>{}();
            return false;
        }
        stack.emplace_back(r.begin(), r.end());
        return true;
    }
};

template<typename ValueCharT, typename Alloc, typename StrTy, typename StackTy>
value_visitor<const basic_value<ValueCharT, Alloc>, StrTy, StackTy> make_value_visitor(StrTy& out, StackTy& stack) {
    return {out, stack};
}

}  // namespace detail

template<typename CharT, typename ValueCharT, typename Alloc>
void write(basic_membuffer<CharT>& out, const basic_value<ValueCharT, Alloc>& v) {
    inline_dynarray<detail::writer_stack_item_t<ValueCharT, Alloc>, 32> stack;

    const auto visitor = detail::make_value_visitor<ValueCharT, Alloc>(out, stack);
    if (!v.visit(visitor)) { return; }

    bool is_first_element = true;

loop:
    auto& top = stack.back();

    if (top.is_record()) {
        while (!top.empty()) {
            out += is_first_element ? '{' : ',';
            detail::write_text<CharT>(out, utf_string_adapter<CharT>{}(top.key()));
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

template<typename CharT, typename ValueCharT, typename Alloc>
void write_formatted(basic_membuffer<CharT>& out, const basic_value<ValueCharT, Alloc>& v, json_fmt_opts opts,
                     unsigned indent) {
    inline_dynarray<detail::writer_stack_item_t<ValueCharT, Alloc>, 32> stack;

    const auto visitor = detail::make_value_visitor<ValueCharT, Alloc>(out, stack);
    if (!v.visit(visitor)) { return; }

    bool is_first_element = true;

loop:
    auto& top = stack.back();
    const char ws_char = top.is_record() ? opts.object_ws_char : opts.array_ws_char;

    while (!top.empty()) {
        if (is_first_element) {
            out += top.is_record() ? '{' : '[';
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
        if (top.is_record()) {
            detail::write_text<CharT>(out, utf_string_adapter<CharT>{}(top.key()));
            out += string_literal<CharT, ':', ' '>{}();
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
    out += top.is_record() ? '}' : ']';

    stack.pop_back();
    if (!stack.empty()) { goto loop; }
}

}  // namespace json
}  // namespace db
}  // namespace uxs
