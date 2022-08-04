#pragma once

#include "json.h"
#include "value.h"

#include "uxs/stringalg.h"

namespace uxs {
namespace db {
namespace json {

namespace detail {
template<typename CharT>
struct utf8_string_converter;
template<>
struct utf8_string_converter<char> {
    static std::string_view from(std::string_view s) { return s; }
    static std::string_view to(std::string_view s) { return s; }
};
template<>
struct utf8_string_converter<wchar_t> {
    static std::wstring from(std::string_view s) { return from_utf8_to_wide(s); }
    static std::string to(std::wstring_view s) { return from_wide_to_utf8(s); }
};
}  // namespace detail

// --------------------------

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc> reader::read(token_t tk_val, const Alloc& al) {
    auto token_to_value = [](token_t tt, std::string_view lval, const Alloc& al) -> basic_value<CharT, Alloc> {
        switch (tt) {
            case token_t::kNull: return {nullptr, al};
            case token_t::kTrue: return {true, al};
            case token_t::kFalse: return {false, al};
            case token_t::kInteger: {
                uint64_t u64 = 0;
                if (stoval(lval, u64) != 0) {
                    if (u64 <= static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
                        return {static_cast<int32_t>(u64), al};
                    } else if (u64 <= static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
                        return {static_cast<uint32_t>(u64), al};
                    } else if (u64 <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                        return {static_cast<int64_t>(u64), al};
                    }
                    return {u64, al};
                }
                // too big integer - treat as double
                return {from_string<double>(lval), al};
            } break;
            case token_t::kNegInteger: {
                int64_t i64 = 0;
                if (stoval(lval, i64) != 0) {
                    if (i64 >= static_cast<int64_t>(std::numeric_limits<int32_t>::min())) {
                        return {static_cast<int32_t>(i64), al};
                    }
                    return {i64, al};
                }
                // too big integer - treat as double
                return {from_string<double>(lval), al};
            } break;
            case token_t::kDouble: return {from_string<double>(lval), al};
            case token_t::kString: return {detail::utf8_string_converter<CharT>::from(lval), al};
            default: UNREACHABLE_CODE;
        }
    };

    basic_value<CharT, Alloc> result;
    basic_inline_dynbuffer<basic_value<CharT, Alloc>*, 32> stack;

    auto* val = &result;
    stack.push_back(val);
    read(
        [&al, &stack, &val, &token_to_value](token_t tt, std::string_view lval) {
            if (tt >= token_t::kNull) {
                *val = token_to_value(tt, lval, al);
            } else {
                *val = tt == token_t::kArray ? make_array<CharT>(al) : make_record<CharT>(al);
                stack.push_back(val);
            }
            return item_ret_code_t::kStepInto;
        },
        [&al, &stack, &val]() { val = &stack.back()->emplace_back(al); },
        [&al, &stack, &val](std::string_view lval) {
            val = &stack.back()->emplace(detail::utf8_string_converter<CharT>::from(lval), al)->second;
        },
        [&stack] { stack.pop_back(); }, tk_val);
    return result;
}

// --------------------------

template<typename CharT, typename Alloc>
struct writer_stack_item_t {
    using value_t = basic_value<CharT, Alloc>;
    writer_stack_item_t() {}
    writer_stack_item_t(const value_t* p, const value_t* it) : v(p), array_it(it) {}
    writer_stack_item_t(const value_t* p, typename value_t::const_record_iterator it) : v(p), record_it(it) {}
    const value_t* v;
#if !defined(_MSC_VER) || _MSC_VER >= 1920
    union {
        const value_t* array_it;
        typename value_t::const_record_iterator record_it;
    };
#else   // !defined(_MSC_VER) || _MSC_VER >= 1920
        // Old versions of VS compiler don't support such unions
    const value_t* array_it = nullptr;
    typename value_t::const_record_iterator record_it;
#endif  // !defined(_MSC_VER) || _MSC_VER >= 1920
};

template<typename CharT, typename Alloc>
void writer::write(const basic_value<CharT, Alloc>& v, unsigned indent) {
    basic_inline_dynbuffer<writer_stack_item_t<CharT, Alloc>, 32> stack;

    auto write_value = [this, &stack, &indent](const basic_value<CharT, Alloc>& v) {
        switch (v.type_) {
            case dtype::kNull: output_.write("null"); break;
            case dtype::kBoolean: output_.write(v.value_.b ? "true" : "false"); break;
            case dtype::kInteger: {
                inline_dynbuffer buf;
                basic_to_string(buf.base(), v.value_.i, fmt_state());
                output_.write(as_span(buf.data(), buf.size()));
            } break;
            case dtype::kUInteger: {
                inline_dynbuffer buf;
                basic_to_string(buf.base(), v.value_.u, fmt_state());
                output_.write(as_span(buf.data(), buf.size()));
            } break;
            case dtype::kInteger64: {
                inline_dynbuffer buf;
                basic_to_string(buf.base(), v.value_.i64, fmt_state());
                output_.write(as_span(buf.data(), buf.size()));
            } break;
            case dtype::kUInteger64: {
                inline_dynbuffer buf;
                basic_to_string(buf.base(), v.value_.u64, fmt_state());
                output_.write(as_span(buf.data(), buf.size()));
            } break;
            case dtype::kDouble: {
                inline_dynbuffer buf;
                basic_to_string(buf.base(), v.value_.dbl, fmt_state(fmt_flags::kAlternate));
                output_.write(as_span(buf.data(), buf.size()));
            } break;
            case dtype::kString: {
                print_quoted_text<char>(output_, detail::utf8_string_converter<CharT>::to(v.str_view()));
            } break;
            case dtype::kArray: {
                output_.put('[');
                stack.emplace_back(&v, v.as_array().data());
                return true;
            } break;
            case dtype::kRecord: {
                output_.put('{');
                indent += indent_size_;
                stack.emplace_back(&v, v.as_record().begin());
                return true;
            } break;
        }
        return false;
    };

    if (!write_value(v)) { return; }

loop:
    auto* top = &stack.back();
    if (top->v->is_array()) {
        auto range = top->v->as_array();
        const auto* el = top->array_it;
        const auto* el_end = range.data() + range.size();
        while (el != el_end) {
            if (el != range.data()) { output_.put(',').put(' '); }
            if (write_value(*el++)) {
                (stack.curr() - 2)->array_it = el;
                goto loop;
            }
        }
        output_.put(']');
    } else {
        auto range = top->v->as_record();
        auto el = top->record_it;
        while (el != range.end()) {
            if (el != range.begin()) { output_.put(','); }
            output_.put('\n').fill_n(indent, indent_char_);
            print_quoted_text<char>(output_, detail::utf8_string_converter<CharT>::to(el->first));
            output_.put(':').put(' ');
            if (write_value((el++)->second)) {
                (stack.curr() - 2)->record_it = el;
                goto loop;
            }
        }
        indent -= indent_size_;
        if (!top->v->empty()) { output_.put('\n').fill_n(indent, indent_char_); }
        output_.put('}');
    }

    stack.pop_back();
    if (!stack.empty()) { goto loop; }
}

}  // namespace json
}  // namespace db
}  // namespace uxs