#pragma once

#include "json.h"
#include "value.h"

#include "uxs/string_alg.h"

namespace uxs {
namespace db {
namespace json {

// --------------------------

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc> reader::read(token_t tk_val, const Alloc& al) {
    auto token_to_value = [](token_t tt, std::string_view lval, const Alloc& al) -> basic_value<CharT, Alloc> {
        switch (tt) {
            case token_t::null: return {nullptr, al};
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
        [&al, &stack, &val, &token_to_value](token_t tt, std::string_view lval) {
            if (tt >= token_t::null) {
                *val = token_to_value(tt, lval, al);
            } else {
                *val = tt == token_t::array ? make_array<CharT>(al) : make_record<CharT>(al);
                stack.push_back(val);
            }
            return next_action_type::step_into;
        },
        [&al, &stack, &val]() { val = &stack.back()->emplace_back(al); },
        [&al, &stack, &val](std::string_view lval) {
            val = &stack.back()->emplace(utf_string_adapter<CharT>{}(lval), al).value();
        },
        [&stack] { stack.pop_back(); }, tk_val);
    return result;
}

// --------------------------

namespace detail {
template<typename CharT, typename Alloc>
struct writer_stack_item_t {
    using value_t = basic_value<CharT, Alloc>;
    using iterator = typename value_t::const_iterator;
    writer_stack_item_t() = default;
    writer_stack_item_t(iterator f, iterator l) : first(f), last(l) {}
    iterator first;
    iterator last;
};

template<typename CharT>
basic_membuffer<CharT>& print_json_text(basic_membuffer<CharT>& out, std::basic_string_view<CharT> text) {
    auto it0 = text.begin();
    out.push_back('\"');
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
                    out.push_back('0' + (*it >> 4));
                    out.push_back("0123456789ABCDEF"[*it & 15]);
                    it0 = it + 1;
                }
                continue;
            } break;
        }
        out += to_string_view(it0, it);
        out.push_back('\\');
        out.push_back(esc);
        it0 = it + 1;
    }
    out += to_string_view(it0, text.end());
    out.push_back('\"');
    return out;
}
}  // namespace detail

template<typename CharT, typename Alloc>
void writer::write(const basic_value<CharT, Alloc>& v, unsigned indent) {
    inline_basic_dynbuffer<detail::writer_stack_item_t<CharT, Alloc>, 32> stack;

    auto write_value = [this, &stack](const basic_value<CharT, Alloc>& v) {
        switch (v.type_) {
            case dtype::null: {
                output_ += string_literal<char, 'n', 'u', 'l', 'l'>{}();
            } break;
            case dtype::boolean: {
                output_ += v.value_.b ? string_literal<char, 't', 'r', 'u', 'e'>{}() :
                                        string_literal<char, 'f', 'a', 'l', 's', 'e'>{}();
            } break;
            case dtype::integer: {
                to_basic_string(output_, v.value_.i);
            } break;
            case dtype::unsigned_integer: {
                to_basic_string(output_, v.value_.u);
            } break;
            case dtype::long_integer: {
                to_basic_string(output_, v.value_.i64);
            } break;
            case dtype::unsigned_long_integer: {
                to_basic_string(output_, v.value_.u64);
            } break;
            case dtype::double_precision: {
                to_basic_string(output_, v.value_.dbl, fmt_opts{fmt_flags::json_compat});
            } break;
            case dtype::string: {
                detail::print_json_text<char>(output_, utf8_string_adapter{}(v.str_view()));
            } break;
            case dtype::array:
            case dtype::record: {
                detail::writer_stack_item_t<CharT, Alloc> item(v.begin(), v.end());
                if (item.first != item.last) {
                    stack.emplace_back(item);
                    return true;
                }
                output_ += item.first.is_record() ? string_literal<char, '{', '}'>{}() :
                                                    string_literal<char, '[', ']'>{}();
            } break;
        }
        return false;
    };

    if (!write_value(v)) {
        output_.flush();
        return;
    }

    bool is_first_element = true;

loop:
    auto& top = stack.back();
    const char ws_char = top.first.is_record() ? record_ws_char_ : array_ws_char_;

    if (is_first_element) {
        output_.push_back(top.first.is_record() ? '{' : '[');
        if (ws_char == '\n') {
            indent += indent_size_;
            output_.push_back('\n');
            output_.append(indent, indent_char_);
        }
    }

    while (top.first != top.last) {
        if (!is_first_element) {
            output_.push_back(',');
            output_.push_back(ws_char);
            if (ws_char == '\n') { output_.append(indent, indent_char_); }
        }
        if (top.first.is_record()) {
            detail::print_json_text<char>(output_, utf8_string_adapter{}(top.first.key()));
            output_ += string_literal<char, ':', ' '>{}();
        }
        if (write_value((top.first++).value())) {
            is_first_element = true;
            goto loop;
        }
        is_first_element = false;
    }

    if (ws_char == '\n') {
        indent -= indent_size_;
        output_.push_back('\n');
        output_.append(indent, indent_char_);
    }
    output_.push_back(top.first.is_record() ? '}' : ']');

    stack.pop_back();
    if (!stack.empty()) { goto loop; }

    output_.flush();
}

}  // namespace json
}  // namespace db
}  // namespace uxs
