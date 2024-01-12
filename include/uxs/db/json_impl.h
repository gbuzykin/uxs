#pragma once

#include "json.h"
#include "value.h"

#include "uxs/stringalg.h"

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
            case token_t::negative_integer_number: {
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
            case token_t::real_number: return {from_string<double>(lval), al};
            case token_t::string: return {uxs::detail::utf8_string_converter<CharT>::from(lval), al};
            default: UNREACHABLE_CODE;
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
            val = &stack.back()->emplace(uxs::detail::utf8_string_converter<CharT>::from(lval), al)->second;
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
    union {
        const value_t* array_it;
        typename value_t::const_record_iterator record_it;
    };
};

template<typename CharT, typename Alloc>
void writer::write(const basic_value<CharT, Alloc>& v, unsigned indent) {
    inline_basic_dynbuffer<writer_stack_item_t<CharT, Alloc>, 32> stack;

    auto write_value = [this, &stack, &indent](const basic_value<CharT, Alloc>& v) {
        switch (v.type_) {
            case dtype::null: output_.write(std::string_view("null", 4)); break;
            case dtype::boolean: {
                output_.write(v.value_.b ? std::string_view("true", 4) : std::string_view("false", 5));
            } break;
            case dtype::integer: {
                membuffer_for_iobuf buf(output_);
                to_basic_string(buf, v.value_.i);
            } break;
            case dtype::unsigned_integer: {
                membuffer_for_iobuf buf(output_);
                to_basic_string(buf, v.value_.u);
            } break;
            case dtype::long_integer: {
                membuffer_for_iobuf buf(output_);
                to_basic_string(buf, v.value_.i64);
            } break;
            case dtype::unsigned_long_integer: {
                membuffer_for_iobuf buf(output_);
                to_basic_string(buf, v.value_.u64);
            } break;
            case dtype::double_precision: {
                membuffer_for_iobuf buf(output_);
                to_basic_string(buf, v.value_.dbl, fmt_opts{fmt_flags::json_compat});
            } break;
            case dtype::string: {
                print_quoted_text(output_,
                                  std::string_view{uxs::detail::utf8_string_converter<CharT>::to(v.str_view())});
            } break;
            case dtype::array: {
                output_.put('[');
                stack.emplace_back(&v, v.as_array().data());
                return true;
            } break;
            case dtype::record: {
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
            print_quoted_text(output_, std::string_view{uxs::detail::utf8_string_converter<CharT>::to(el->first)});
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
