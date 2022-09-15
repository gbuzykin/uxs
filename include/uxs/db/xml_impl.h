#pragma once

#include "exception.h"
#include "value.h"
#include "xml.h"

#include "uxs/algorithm.h"
#include "uxs/format.h"
#include "uxs/stringalg.h"

namespace uxs {
namespace db {
namespace xml {

// --------------------------

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc> reader::read(std::string_view root_element, const Alloc& al) {
    if (input_.peek() == iobuf::traits_type::eof()) { throw exception("empty input"); }

    auto text_to_value = [&al](std::string_view sval) -> basic_value<CharT, Alloc> {
        switch (classify_string(sval)) {
            case string_class::kNull: return {nullptr, al};
            case string_class::kTrue: return {true, al};
            case string_class::kFalse: return {false, al};
            case string_class::kInteger: {
                uint64_t u64 = 0;
                if (stoval(sval, u64) != 0) {
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
                return {from_string<double>(sval), al};
            } break;
            case string_class::kNegInteger: {
                int64_t i64 = 0;
                if (stoval(sval, i64) != 0) {
                    if (i64 >= static_cast<int64_t>(std::numeric_limits<int32_t>::min())) {
                        return {static_cast<int32_t>(i64), al};
                    }
                    return {i64, al};
                }
                // too big integer - treat as double
                return {from_string<double>(sval), al};
            } break;
            case string_class::kDouble: return {from_string<double>(sval), al};
            case string_class::kWsWithNl: return make_record<CharT>(al);
            case string_class::kOther: return {uxs::detail::utf8_string_converter<CharT>::from(sval), al};
            default: UNREACHABLE_CODE;
        }
    };

    inline_dynbuffer txt;
    basic_value<CharT, Alloc> result;
    std::vector<std::pair<basic_value<CharT, Alloc>*, std::string>> stack;
    std::pair<token_t, std::string_view> tk = read_next();

    stack.reserve(32);
    stack.emplace_back(&result, root_element);

    while (true) {
        auto* top = &stack.back();
        switch (tk.first) {
            case token_t::kEof: throw exception(format("{}: unexpected end of file", n_ln_));
            case token_t::kPreamble: throw exception(format("{}: unexpected document preamble", n_ln_));
            case token_t::kEntity: throw exception(format("{}: unknown entity name", n_ln_));
            case token_t::kPlainText: {
                if (!top->first->is_record()) { txt += tk.second; }
            } break;
            case token_t::kStartElement: {
                txt.clear();
                auto result = top->first->emplace_unique(uxs::detail::utf8_string_converter<CharT>::from(tk.second), al);
                stack.emplace_back(&result.first->second, tk.second);
                if (!result.second) {
                    result.first->second.convert(dtype::kArray);
                    stack.back().first = &result.first->second.emplace_back(al);
                }
            } break;
            case token_t::kEndElement: {
                if (top->second != tk.second) {
                    throw exception(format("{}: unterminated element {}", n_ln_, top->second));
                }
                if (!top->first->is_record()) {
                    *(top->first) = text_to_value(std::string_view(txt.data(), txt.size()));
                }
                stack.pop_back();
                if (stack.empty()) { return result; }
            } break;
            default: UNREACHABLE_CODE;
        }
        tk = read_next();
    }
}

// --------------------------

template<typename CharT, typename Alloc>
struct writer_stack_item_t {
    using value_t = basic_value<CharT, Alloc>;
    writer_stack_item_t() {}
    writer_stack_item_t(const value_t* p, std::string_view el, const value_t* it) : v(p), element(el), array_it(it) {}
    writer_stack_item_t(const value_t* p, std::string_view el, typename value_t::const_record_iterator it)
        : v(p), element(el), record_it(it) {}
    const value_t* v;
    decltype(uxs::detail::utf8_string_converter<CharT>::to({})) element;
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

template<typename CharT>
basic_iobuf<CharT>& print_xml_text(basic_iobuf<CharT>& out, std::basic_string_view<CharT> text) {
    const CharT *p1 = text.data(), *pend = text.data() + text.size();
    for (const CharT* p2 = text.data(); p2 != pend; ++p2) {
        std::string_view esc;
        switch (*p2) {
            case '&': esc = "&amp;"; break;
            case '<': esc = "&lt;"; break;
            case '>': esc = "&gt;"; break;
            case '\'': esc = "&apos;"; break;
            case '\"': esc = "&quot;"; break;
            default: continue;
        }
        out.write(as_span(p1, p2 - p1));
        for (char ch : esc) { out.put(ch); }
        p1 = p2 + 1;
    }
    out.write(as_span(p1, pend - p1));
    return out;
}

template<typename CharT, typename Alloc>
void writer::write(const basic_value<CharT, Alloc>& v, std::string_view root_element, unsigned indent) {
    std::vector<writer_stack_item_t<CharT, Alloc>> stack;
    decltype(uxs::detail::utf8_string_converter<CharT>::to({})) element(root_element);
    stack.reserve(32);

    auto write_value = [this, &stack, &element, &indent](const basic_value<CharT, Alloc>& v) {
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
                print_xml_text<char>(output_, uxs::detail::utf8_string_converter<CharT>().to(v.str_view()));
            } break;
            case dtype::kArray: {
                stack.emplace_back(&v, element, v.as_array().data());
                return true;
            } break;
            case dtype::kRecord: {
                indent += indent_size_;
                stack.emplace_back(&v, element, v.as_record().begin());
                return true;
            } break;
        }
        return false;
    };

    output_.put('<').write(element).put('>');
    if (!write_value(v)) {
        output_.put('<').put('/').write(element).put('>');
        return;
    }

loop:
    auto* top = &stack.back();
    if (top->v->is_array()) {
        auto range = top->v->as_array();
        const auto* el = top->array_it;
        const auto* el_end = range.data() + range.size();
        while (true) {
            if (el != range.data() && !(el - 1)->is_array()) { output_.put('<').put('/').write(element).put('>'); }
            if (el == el_end) { break; }
            if (!el->is_array()) { output_.put('\n').fill_n(indent, indent_char_).put('<').write(element).put('>'); }
            if (write_value(*el++)) {
                std::prev(stack.end(), 2)->array_it = el;
                goto loop;
            }
        }
    } else {
        auto range = top->v->as_record();
        auto el = top->record_it;
        while (true) {
            if (el != range.begin() && !std::prev(el)->second.is_array()) {
                output_.put('<').put('/').write(element).put('>');
            }
            if (el == range.end()) { break; }
            element = uxs::detail::utf8_string_converter<CharT>::to(el->first);
            if (!el->second.is_array()) {
                output_.put('\n').fill_n(indent, indent_char_).put('<').write(element).put('>');
            }
            if (write_value((el++)->second)) {
                std::prev(stack.end(), 2)->record_it = el;
                goto loop;
            }
        }
        indent -= indent_size_;
        output_.put('\n').fill_n(indent, indent_char_);
    }

    element = top->element;
    stack.pop_back();
    if (!stack.empty()) { goto loop; }
    output_.put('<').put('/').write(element).put('>');
}

}  // namespace xml
}  // namespace db
}  // namespace uxs
