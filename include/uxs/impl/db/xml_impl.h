#pragma once

#include "uxs/db/value.h"
#include "uxs/db/xml.h"
#include "uxs/dynarray.h"
#include "uxs/string_conv.h"

namespace uxs {
namespace db {
namespace xml {

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
basic_value<CharT, Alloc> parser::read(std::string_view root_element, const Alloc& al) {
    static const auto text_to_value = [](std::string_view lval, const Alloc& al) -> basic_value<CharT, Alloc> {
        switch (classify_value(lval)) {
            case value_class::empty:
            case value_class::null_value: return {nullptr, al};
            case value_class::true_value: return {true, al};
            case value_class::false_value: return {false, al};
            case value_class::integer_number: {
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
            case value_class::negative_integer_number: {
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
            case value_class::floating_point_number: return {from_string<double>(lval), al};
            case value_class::ws_with_nl: return make_record<CharT>(al);
            case value_class::other: return {utf_string_adapter<CharT>{}(lval), al};
            default: UXS_UNREACHABLE_CODE;
        }
    };

    auto tt = token_type();
    while (!eof() && !(tt == token_t::start_element && name() == root_element)) { tt = next(); }
    if (eof()) { throw database_error("no such element"); }

    inline_dynbuffer txt;
    inline_dynarray<std::pair<basic_value<CharT, Alloc>*, std::string>, 32> stack;

    basic_value<CharT, Alloc> val(al);
    stack.emplace_back(&val, root_element);

    tt = next();

    while (true) {
        auto& top = stack.back();
        switch (tt) {
            case token_t::eof: throw database_error(to_string(lexer_.ln) + ": unexpected end of file");
            case token_t::preamble: throw database_error(to_string(lexer_.ln) + ": unexpected document preamble");
            case token_t::entity: throw database_error(to_string(lexer_.ln) + ": unknown entity name");
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
                    throw database_error(to_string(lexer_.ln) + ": unterminated element " + top.second);
                }
                if (!top.first->is_record() && !txt.empty()) {
                    *(top.first) = text_to_value(std::string_view(txt.data(), txt.size()), al);
                }
                stack.pop_back();
                if (stack.empty()) { return val; }
            } break;
            default: break;
        }
        tt = next();
    }
}

// --------------------------

namespace detail {

template<typename CharT, typename Alloc>
struct writer_stack_item_t {
 public:
    using value_t = basic_value<CharT, Alloc>;
    using record_iterator = typename value_t::const_record_iterator;

    writer_stack_item_t(const value_t* first, const value_t* last) noexcept : is_record_(false), arr_{first, last} {}
    writer_stack_item_t(record_iterator first, record_iterator last) noexcept : is_record_(true), rec_{first, last} {}

    std::basic_string_view<CharT> element() const noexcept { return element_; }
    void set_element(std::basic_string_view<CharT> element) noexcept { element_ = element; }

    bool is_record() const noexcept { return is_record_; }
    bool empty() const noexcept { return is_record_ ? rec_.first == rec_.last : arr_.first == arr_.last; }
    typename value_t::key_type key() const noexcept { return rec_.first->key(); }
    const value_t& get_and_advance() noexcept { return is_record_ ? (rec_.first++)->value() : *arr_.first++; }
    const value_t& prev() const noexcept {
        return is_record_ ? std::prev(rec_.first)->value() : *std::prev(arr_.first);
    }

 private:
    struct array_range_t {
        const value_t* first;
        const value_t* last;
    };
    struct record_range_t {
        record_iterator first;
        record_iterator last;
    };

    std::basic_string_view<CharT> element_;
    bool is_record_;
    union {
        array_range_t arr_;
        record_range_t rec_;
    };
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
        stack.emplace_back(r.data(), r.data() + r.size());
        return true;
    }

    bool operator()(decltype(std::declval<ValueTy>().as_record()) r) const {
        stack.emplace_back(r.begin(), r.end());
        return true;
    }
};

template<typename CharT, typename Alloc, typename StrTy, typename StackTy>
value_visitor<const basic_value<CharT, Alloc>, StrTy, StackTy> make_value_visitor(StrTy& out, StackTy& stack) {
    return {out, stack};
}

template<typename OutCharT, typename CharT, typename Alloc>
void write(basic_membuffer<OutCharT>& out, const basic_value<CharT, Alloc>& v, std::basic_string_view<CharT> element,
           xml_fmt_opts opts, unsigned indent) {
    inline_dynarray<detail::writer_stack_item_t<CharT, Alloc>, 32> stack;

    const auto visitor = detail::make_value_visitor<CharT, Alloc>(out, stack);

    out += '<';
    utf_string_adapter<OutCharT>{}.append(out, element);
    out += '>';
    if (!v.visit(visitor)) {
        out += string_literal<OutCharT, '<', '/'>{}();
        utf_string_adapter<OutCharT>{}.append(out, element);
        out += '>';
        return;
    }

    bool is_first_element = true;
    stack.back().set_element(element);

loop:
    auto& top = stack.back();

    if (is_first_element && top.is_record()) { indent += opts.indent_size; }

    while (true) {
        if (!is_first_element && !top.prev().is_array()) {
            out += string_literal<OutCharT, '<', '/'>{}();
            utf_string_adapter<OutCharT>{}.append(out, element);
            out += '>';
        }
        if (top.empty()) { break; }
        if (top.is_record()) { element = top.key(); }
        const auto& value = top.get_and_advance();
        if (!value.is_array()) {
            out += '\n';
            out.append(indent, opts.indent_char);
            out += '<';
            utf_string_adapter<OutCharT>{}.append(out, element);
            out += '>';
        }
        if (value.visit(visitor)) {
            is_first_element = true;
            stack.back().set_element(element);
            goto loop;
        }
        is_first_element = false;
    }

    if (top.is_record()) {
        indent -= opts.indent_size;
        out += '\n';
        out.append(indent, opts.indent_char);
    }

    is_first_element = false;
    element = top.element();
    stack.pop_back();
    if (!stack.empty()) { goto loop; }

    out += string_literal<OutCharT, '<', '/'>{}();
    utf_string_adapter<OutCharT>{}.append(out, element);
    out += '>';
}

}  // namespace detail
}  // namespace xml
}  // namespace db
}  // namespace uxs
