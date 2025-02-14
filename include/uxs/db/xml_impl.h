#pragma once

#include "value.h"
#include "xml.h"

#include "uxs/string_alg.h"

namespace uxs {
namespace db {
namespace xml {

// --------------------------

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc> reader::read(std::string_view root_element, const Alloc& al) {
    if (input_.peek() == ibuf::traits_type::eof()) { throw database_error("empty input"); }

    auto text_to_value = [&al](std::string_view sval) -> basic_value<CharT, Alloc> {
        switch (classify_string(sval)) {
            case string_class::null: return {nullptr, al};
            case string_class::true_value: return {true, al};
            case string_class::false_value: return {false, al};
            case string_class::integer_number: {
                std::uint64_t u64 = 0;
                if (stoval(sval, u64) != 0) {
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
                return {from_string<double>(sval), al};
            } break;
            case string_class::negative_integer_number: {
                std::int64_t i64 = 0;
                if (stoval(sval, i64) != 0) {
                    if (i64 >= static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min())) {
                        return {static_cast<std::int32_t>(i64), al};
                    }
                    return {i64, al};
                }
                // too big integer - treat as double
                return {from_string<double>(sval), al};
            } break;
            case string_class::floating_point_number: return {from_string<double>(sval), al};
            case string_class::ws_with_nl: return make_record<CharT>(al);
            case string_class::other: return {utf_string_adapter<CharT>{}(sval), al};
            default: UXS_UNREACHABLE_CODE;
        }
    };

    inline_dynbuffer txt;
    basic_value<CharT, Alloc> result;
    std::vector<std::pair<basic_value<CharT, Alloc>*, std::string>> stack;
    std::pair<token_t, std::string_view> tk = read_next();

    stack.reserve(32);
    stack.emplace_back(&result, root_element);

    while (true) {
        auto& top = stack.back();
        switch (tk.first) {
            case token_t::eof: throw database_error(format("{}: unexpected end of file", n_ln_));
            case token_t::preamble: throw database_error(format("{}: unexpected document preamble", n_ln_));
            case token_t::entity: throw database_error(format("{}: unknown entity name", n_ln_));
            case token_t::plain_text: {
                if (!top.first->is_record()) { txt += tk.second; }
            } break;
            case token_t::start_element: {
                txt.clear();
                auto result = top.first->emplace_unique(utf_string_adapter<CharT>{}(tk.second), al);
                stack.emplace_back(&result.first.value(), tk.second);
                if (!result.second) { stack.back().first = &result.first.value().emplace_back(al); }
            } break;
            case token_t::end_element: {
                if (top.second != tk.second) {
                    throw database_error(format("{}: unterminated element {}", n_ln_, top.second));
                }
                if (!top.first->is_record()) { *(top.first) = text_to_value(std::string_view(txt.data(), txt.size())); }
                stack.pop_back();
                if (stack.empty()) { return result; }
            } break;
            default: UXS_UNREACHABLE_CODE;
        }
        tk = read_next();
    }
}

// --------------------------

namespace detail {
template<typename CharT, typename ValueCharT, typename Alloc>
struct writer_stack_item_t {
    using value_t = basic_value<ValueCharT, Alloc>;
    using iterator = typename value_t::const_iterator;
    using string_type = std::conditional_t<std::is_same<ValueCharT, CharT>::value, std::basic_string_view<CharT>,
                                           std::basic_string<CharT>>;
    writer_stack_item_t() = default;
    writer_stack_item_t(std::basic_string_view<CharT> el, iterator f, iterator l) : element(el), first(f), last(l) {}
    string_type element;
    iterator first;
    iterator last;
};

template<typename CharT>
basic_membuffer<CharT>& print_xml_text(basic_membuffer<CharT>& out, std::basic_string_view<CharT> text) {
    auto it0 = text.begin();
    for (auto it = it0; it != text.end(); ++it) {
        std::basic_string_view<CharT> esc;
        switch (*it) {
            case '&': esc = string_literal<CharT, '&', 'a', 'm', 'p', ';'>{}(); break;
            case '<': esc = string_literal<CharT, '&', 'l', 't', ';'>{}(); break;
            case '>': esc = string_literal<CharT, '&', 'g', 't', ';'>{}(); break;
            case '\'': esc = string_literal<CharT, '&', 'a', 'p', 'o', 's', ';'>{}(); break;
            case '\"': esc = string_literal<CharT, '&', 'q', 'u', 'o', 't', ';'>{}(); break;
            default: continue;
        }
        out += to_string_view(it0, it);
        out += esc;
        it0 = it + 1;
    }
    out += to_string_view(it0, text.end());
    return out;
}
}  // namespace detail

template<typename CharT>
template<typename ValueCharT, typename Alloc>
void writer<CharT>::write(const basic_value<ValueCharT, Alloc>& v, std::basic_string_view<CharT> root_element,
                          unsigned indent) {
    using stack_item_t = detail::writer_stack_item_t<CharT, ValueCharT, Alloc>;
    std::vector<stack_item_t> stack;
    typename stack_item_t::string_type element(root_element);
    stack.reserve(32);

    auto write_value = [this, &stack, &element](const basic_value<ValueCharT, Alloc>& v) {
        switch (v.type_) {
            case dtype::null: {
                output_ += string_literal<CharT, 'n', 'u', 'l', 'l'>{}();
            } break;
            case dtype::boolean: {
                output_ += v.value_.b ? string_literal<CharT, 't', 'r', 'u', 'e'>{}() :
                                        string_literal<CharT, 'f', 'a', 'l', 's', 'e'>{}();
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
                detail::print_xml_text<CharT>(output_, utf_string_adapter<CharT>{}(v.str_view()));
            } break;
            case dtype::array:
            case dtype::record: {
                stack.emplace_back(element, v.begin(), v.end());
                return true;
            } break;
        }
        return false;
    };

    output_.push_back('<');
    output_ += element;
    output_.push_back('>');
    if (!write_value(v)) {
        output_ += string_literal<CharT, '<', '/'>{}();
        output_ += element;
        output_.push_back('>');
        output_.flush();
        return;
    }

    bool is_first_element = true;

loop:
    auto& top = stack.back();

    if (is_first_element && top.first.is_record()) { indent += indent_size_; }

    while (true) {
        if (!is_first_element && !std::prev(top.first).value().is_array()) {
            output_ += string_literal<CharT, '<', '/'>{}();
            output_ += element;
            output_.push_back('>');
        }
        if (top.first == top.last) { break; }
        if (top.first.is_record()) { element = utf_string_adapter<CharT>{}(top.first.key()); }
        if (!top.first.value().is_array()) {
            output_.push_back('\n');
            output_.append(indent, indent_char_);
            output_.push_back('<');
            output_ += element;
            output_.push_back('>');
        }
        if (write_value((top.first++).value())) {
            is_first_element = true;
            goto loop;
        }
        is_first_element = false;
    }

    if (top.first.is_record()) {
        indent -= indent_size_;
        output_.push_back('\n');
        output_.append(indent, indent_char_);
    }

    is_first_element = false;
    element = top.element;
    stack.pop_back();
    if (!stack.empty()) { goto loop; }

    output_ += string_literal<CharT, '<', '/'>{}();
    output_ += element;
    output_.push_back('>');
    output_.flush();
}

}  // namespace xml
}  // namespace db
}  // namespace uxs
