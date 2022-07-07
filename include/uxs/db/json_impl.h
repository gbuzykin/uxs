#pragma once

#include "json.h"

#include "uxs/format.h"
#include "uxs/stringalg.h"

namespace uxs {
namespace db {
namespace json {

namespace detail {
template<typename CharT>
struct utf8_string_converter {
    std::basic_string_view<CharT> from(std::string_view s) { return s; }
    std::string_view to(std::basic_string_view<CharT> s) { return s; }
};
template<>
struct utf8_string_converter<wchar_t> {
    std::wstring from(std::string_view s) { return from_utf8_to_wide(s); }
    std::string to(std::wstring_view s) { return from_wide_to_utf8(s); }
};
}  // namespace detail

// --------------------------

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc> reader::read(const Alloc& al) {
    if (input_.peek() == iobuf::traits_type::eof()) { throw exception("empty input"); }

    auto token_to_value = [this, &al](int tk, std::string_view lval) -> basic_value<CharT, Alloc> {
        switch (tk) {
            case '[': return make_array<CharT>(al);
            case '{': return make_record<CharT>(al);
            case kNull: return basic_value<CharT, Alloc>(al);
            case kTrue: return {true, al};
            case kFalse: return {false, al};
            case kInteger: {
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
            case kNegInteger: {
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
            case kDouble: return {from_string<double>(lval), al};
            case kString: return {detail::utf8_string_converter<CharT>().from(lval), al};
        }
        throw exception(format("{}: invalid value or unexpected character", n_ln_));
    };

    std::string_view lval;
    basic_inline_dynbuffer<basic_value<CharT, Alloc>*, 32> stack;

    init_parser();

    int tk = parse_token(lval);
    basic_value<CharT, Alloc> result = token_to_value(tk, lval);
    if (!result.is_array() && !result.is_record()) { return result; }

    basic_value<CharT, Alloc>* top = &result;
    bool comma = false;
    tk = parse_token(lval);

loop:
    if (top->is_array()) {
        if (comma || tk != ']') {
            while (true) {
                basic_value<CharT, Alloc>& el = top->emplace_back(token_to_value(tk, lval));
                tk = parse_token(lval);
                if (el.is_array() || el.is_record()) {
                    stack.push_back(top);
                    top = &el, comma = false;
                    goto loop;
                }
                if (tk == ']') { break; }
                if (tk != ',') { throw exception(format("{}: expected `,` or `]`", n_ln_)); }
                tk = parse_token(lval);
            }
        }
    } else if (comma || tk != '}') {
        while (true) {
            if (tk != kString) { throw exception(format("{}: expected valid string", n_ln_)); }
            basic_value<CharT, Alloc>& el = top->emplace(detail::utf8_string_converter<CharT>().from(lval), al);
            tk = parse_token(lval);
            if (tk != ':') { throw exception(format("{}: expected `:`", n_ln_)); }
            tk = parse_token(lval);
            el = token_to_value(tk, lval);
            tk = parse_token(lval);
            if (el.is_array() || el.is_record()) {
                stack.push_back(top);
                top = &el, comma = false;
                goto loop;
            }
            if (tk == '}') { break; }
            if (tk != ',') { throw exception(format("{}: expected `,` or `}}`", n_ln_)); }
            tk = parse_token(lval);
        }
    }

    while (!stack.empty()) {
        top = stack.back();
        stack.pop_back();
        tk = parse_token(lval);
        char close_char = top->is_array() ? ']' : '}';
        if (tk != close_char) {
            if (tk != ',') { throw exception(format("{}: expected `,` or `{}`", n_ln_, close_char)); }
            tk = parse_token(lval), comma = true;
            goto loop;
        }
    }
    return result;
}

// --------------------------

template<typename ValTy>
struct writer_stack_item_t {
    writer_stack_item_t() {}
    writer_stack_item_t(const ValTy* p, const ValTy* el) : v(p), array_element(el) {}
    writer_stack_item_t(const ValTy* p, typename ValTy::const_record_iterator el) : v(p), record_element(el) {}
    const ValTy* v;
#if !defined(_MSC_VER) || _MSC_VER >= 1920
    union {
        const ValTy* array_element;
        typename ValTy::const_record_iterator record_element;
    };
#else   // !defined(_MSC_VER) || _MSC_VER >= 1920
    // Old versions of VS compiler don't support such unions
    const ValTy* array_element = nullptr;
    typename ValTy::const_record_iterator record_element;
#endif  // !defined(_MSC_VER) || _MSC_VER >= 1920
};

template<typename CharT, typename Alloc>
void writer::write(const basic_value<CharT, Alloc>& v) {
    unsigned indent = 0;
    basic_inline_dynbuffer<writer_stack_item_t<basic_value<CharT, Alloc>>, 32> stack;

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
                print_quoted_text<char>(output_, detail::utf8_string_converter<CharT>().to(v.str_view()));
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
    writer_stack_item_t<basic_value<CharT, Alloc>>* top = stack.curr() - 1;
    if (top->v->is_array()) {
        auto range = top->v->as_array();
        const basic_value<CharT, Alloc>* el = top->array_element;
        const basic_value<CharT, Alloc>* el_end = range.data() + range.size();
        while (el != el_end) {
            if (el != range.data()) { output_.put(',').put(' '); }
            if (write_value(*el++)) {
                (stack.curr() - 2)->array_element = el;
                goto loop;
            }
        }
        output_.put(']');
    } else {
        auto range = top->v->as_record();
        auto el = top->record_element;
        while (el != range.end()) {
            if (el != range.begin()) { output_.put(','); }
            output_.put('\n');
            output_.fill_n(indent, indent_char_);
            print_quoted_text<char>(output_, detail::utf8_string_converter<CharT>().to(el->first));
            output_.put(':').put(' ');
            if (write_value((el++)->second)) {
                (stack.curr() - 2)->record_element = el;
                goto loop;
            }
        }
        indent -= indent_size_;
        if (!top->v->empty()) {
            output_.put('\n');
            output_.fill_n(indent, indent_char_);
        }
        output_.put('}');
    }

    stack.pop_back();
    if (!stack.empty()) { goto loop; }
}

}  // namespace json
}  // namespace db
}  // namespace uxs
