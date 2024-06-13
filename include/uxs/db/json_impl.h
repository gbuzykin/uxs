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
            val = &stack.back()->emplace(utf_string_adapter<CharT>{}(lval), al)->second;
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

template<typename CharT>
basic_membuffer<CharT>& print_json_text(basic_membuffer<CharT>& out, std::basic_string_view<CharT> text) {
    auto it0 = text.begin();
    out.push_back('\"');
    for (auto it = it0; it != text.end(); ++it) {
        char esc = '\0';
        switch (*it) {
            case '\"': esc = '\"'; break;
            case '\\': esc = '\\'; break;
            case '\a': esc = 'a'; break;
            case '\b': esc = 'b'; break;
            case '\f': esc = 'f'; break;
            case '\n': esc = 'n'; break;
            case '\r': esc = 'r'; break;
            case '\t': esc = 't'; break;
            case '\v': esc = 'v'; break;
            default: continue;
        }
        out.append(it0, it).push_back('\\');
        out.push_back(esc);
        it0 = it + 1;
    }
    out.append(it0, text.end()).push_back('\"');
    return out;
}

template<typename CharT, typename Alloc>
void writer::write(const basic_value<CharT, Alloc>& v, unsigned indent) {
    inline_basic_dynbuffer<writer_stack_item_t<CharT, Alloc>, 32> stack;

    auto write_value = [this, &stack, &indent](const basic_value<CharT, Alloc>& v) {
        switch (v.type_) {
            case dtype::null: output_.append("null", 4); break;
            case dtype::boolean: {
                output_.append(v.value_.b ? std::string_view("true", 4) : std::string_view("false", 5));
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
                print_json_text<char>(output_, utf_string_adapter<char>{}(v.str_view()));
            } break;
            case dtype::array: {
                output_.push_back('[');
                stack.emplace_back(&v, v.as_array().data());
                return true;
            } break;
            case dtype::record: {
                output_.push_back('{');
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
            if (el != range.data()) { output_.append(", ", 2); }
            if (write_value(*el++)) {
                (stack.curr() - 2)->array_it = el;
                goto loop;
            }
        }
        output_.push_back(']');
    } else {
        auto range = top->v->as_record();
        auto el = top->record_it;
        while (el != range.end()) {
            if (el != range.begin()) { output_.push_back(','); }
            output_.push_back('\n');
            output_.append(indent, indent_char_);
            print_json_text<char>(output_, utf_string_adapter<char>{}(el->first));
            output_.append(": ", 2);
            if (write_value((el++)->second)) {
                (stack.curr() - 2)->record_it = el;
                goto loop;
            }
        }
        indent -= indent_size_;
        if (!top->v->empty()) {
            output_.push_back('\n');
            output_.append(indent, indent_char_);
        }
        output_.push_back('}');
    }

    stack.pop_back();
    if (!stack.empty()) { goto loop; }
}

}  // namespace json
}  // namespace db
}  // namespace uxs
