#pragma once

#include "uxs/db/value.h"
#include "uxs/db/xml.h"

#include <vector>

namespace uxs {
namespace db {
namespace xml {

// --------------------------

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc> parser::read(std::string_view root_element, const Alloc& al) {
    if (in_.peek() == ibuf::traits_type::eof()) { throw database_error("empty input"); }

    static const auto text_to_value = [](std::string_view sval, const Alloc& al) -> basic_value<CharT, Alloc> {
        switch (classify_value(sval)) {
            case value_class::empty:
            case value_class::null_value: return {nullptr, al};
            case value_class::true_value: return {true, al};
            case value_class::false_value: return {false, al};
            case value_class::integer_number: {
                std::uint64_t u64 = 0;
                if (from_string(sval, u64) != 0) {
                    if (u64 <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
                        return {static_cast<std::int32_t>(u64), al};
                    }
                    if (u64 <= static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
                        return {static_cast<std::uint32_t>(u64), al};
                    }
                    if (u64 <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                        return {static_cast<std::int64_t>(u64), al};
                    }
                    return {u64, al};
                }
                // too big integer - treat as double
                return {from_string<double>(sval), al};
            } break;
            case value_class::negative_integer_number: {
                std::int64_t i64 = 0;
                if (from_string(sval, i64) != 0) {
                    if (i64 >= static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min())) {
                        return {static_cast<std::int32_t>(i64), al};
                    }
                    return {i64, al};
                }
                // too big integer - treat as double
                return {from_string<double>(sval), al};
            } break;
            case value_class::floating_point_number: return {from_string<double>(sval), al};
            case value_class::ws_with_nl: return make_record<CharT>(al);
            case value_class::other: return {utf_string_adapter<CharT>{}(sval), al};
            default: UXS_UNREACHABLE_CODE;
        }
    };

    auto tt = token_type();
    while (!eof() && !(tt == token_t::start_element && name() == root_element)) { tt = next(); }
    if (eof()) { throw database_error("no such element"); }

    inline_dynbuffer txt;
    basic_value<CharT, Alloc> result(al);
    std::vector<std::pair<basic_value<CharT, Alloc>*, std::string>> stack;

    stack.reserve(32);
    stack.emplace_back(&result, root_element);

    tt = next();

    while (true) {
        auto& top = stack.back();
        switch (tt) {
            case token_t::eof: throw database_error(to_string(ln_) + ": unexpected end of file");
            case token_t::preamble: throw database_error(to_string(ln_) + ": unexpected document preamble");
            case token_t::entity: throw database_error(to_string(ln_) + ": unknown entity name");
            case token_t::plain_text: {
                if (!top.first->is_record()) { txt += text(); }
            } break;
            case token_t::start_element: {
                txt.clear();
                auto result = top.first->emplace_unique(utf_string_adapter<CharT>{}(name()), al);
                stack.emplace_back(&result.first.value(), name());
                if (!result.second) { stack.back().first = &result.first.value().emplace_back(al); }
                for (const auto& attr : attributes()) {
                    stack.back().first->emplace_unique(utf_string_adapter<CharT>{}(attr.first),
                                                       text_to_value(attr.second, al));
                }
            } break;
            case token_t::end_element: {
                if (top.second != name()) {
                    throw database_error(to_string(ln_) + ": unterminated element " + top.second);
                }
                if (!top.first->is_record() && !txt.empty()) {
                    *(top.first) = text_to_value(std::string_view(txt.data(), txt.size()), al);
                }
                stack.pop_back();
                if (stack.empty()) { return result; }
            } break;
            default: break;
        }
        tt = next();
    }
}

// --------------------------

namespace detail {

template<typename ValueCharT, typename Alloc>
struct writer_stack_item_t {
    using value_t = basic_value<ValueCharT, Alloc>;
    using iterator = typename value_t::const_iterator;
    writer_stack_item_t() = default;
    writer_stack_item_t(iterator f, iterator l) : first(f), last(l) {}
    std::basic_string_view<ValueCharT> element;
    iterator first;
    iterator last;
};

template<typename CharT>
basic_membuffer<CharT>& write_text(basic_membuffer<CharT>& out, std::basic_string_view<CharT> text) {
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

template<typename StrTy, typename StackTy>
struct value_visitor {
    using char_type = typename StrTy::value_type;

    StrTy& out;
    StackTy& stack;

    value_visitor(StrTy& out, StackTy& stack) : out(out), stack(stack) {}

    template<typename Ty>
    bool operator()(Ty v) const {
        to_basic_string(out, v);
        return false;
    }

    bool operator()(double f) const {
        to_basic_string(out, f, fmt_opts{fmt_flags::json_compat});
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
void writer<CharT>::do_write(const basic_value<ValueCharT, Alloc>& v, std::basic_string_view<ValueCharT> element,
                             unsigned indent) {
    using stack_item_t = detail::writer_stack_item_t<ValueCharT, Alloc>;
    inline_basic_dynbuffer<stack_item_t, 32> stack;

    auto visitor = make_value_visitor(out, stack);

    out += '<';
    utf_string_adapter<CharT>{}.append(out, element);
    out += '>';
    if (!v.visit(visitor)) {
        out += string_literal<CharT, '<', '/'>{}();
        utf_string_adapter<CharT>{}.append(out, element);
        out += '>';
        return;
    }

    bool is_first_element = true;
    stack.back().element = element;

loop:
    auto& top = stack.back();

    if (is_first_element && top.first.is_record()) { indent += indent_size; }

    while (true) {
        if (!is_first_element && !std::prev(top.first).value().is_array()) {
            out += string_literal<CharT, '<', '/'>{}();
            utf_string_adapter<CharT>{}.append(out, element);
            out += '>';
        }
        if (top.first == top.last) { break; }
        if (top.first.is_record()) { element = top.first.key(); }
        if (!top.first.value().is_array()) {
            out += '\n';
            out.append(indent, indent_char);
            out += '<';
            utf_string_adapter<CharT>{}.append(out, element);
            out += '>';
        }
        if ((top.first++).value().visit(visitor)) {
            is_first_element = true;
            stack.back().element = element;
            goto loop;
        }
        is_first_element = false;
    }

    if (top.first.is_record()) {
        indent -= indent_size;
        out += '\n';
        out.append(indent, indent_char);
    }

    is_first_element = false;
    element = top.element;
    stack.pop_back();
    if (!stack.empty()) { goto loop; }

    out += string_literal<CharT, '<', '/'>{}();
    utf_string_adapter<CharT>{}.append(out, element);
    out += '>';
}

}  // namespace detail

}  // namespace xml
}  // namespace db
}  // namespace uxs
