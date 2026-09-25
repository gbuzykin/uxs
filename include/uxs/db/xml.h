#pragma once

#include "value.h"

#include "uxs/io/iomembuffer.h"

namespace uxs {
namespace db {
namespace xml {

enum class token_t : int {
    eof = 0,
    none,
    plain_text,
    start_element,
    end_element,
    entity,
    preamble,
};

enum class value_class : int {
    empty = 0,
    null_value,
    true_value,
    false_value,
    integer_number,
    negative_integer_number,
    floating_point_number,
    ws_with_nl,
    other,
};

struct xml_fmt_opts {
    std::uint8_t indent_char = ' ';
    unsigned indent_size = 2;
};

namespace detail {
enum class lex_token_t : int {
    eof = 0,
    eq = '=',
    close = '>',
    name = 256,
    predef_entity,
    entity,
    string,
    start_element_open,
    end_element_open,
    pi_open,
    comment,
    end_element_close,
    pi_close,
};

template<typename CharT>
struct lexer {
    basic_ibuf<CharT>& in;
    unsigned ln = 1;
    basic_inline_dynbuffer<CharT> stash;
    explicit lexer(basic_ibuf<CharT>& in) : in(in) {}
    lex_token_t lex(std::basic_string_view<CharT>& lval);
};

[[noreturn]] UXS_EXPORT void report_error(unsigned ln, const char* message);
}  // namespace detail

template<typename InCharT>
class parser;

template<typename CharT>
class parser_iterator : public est::iterator_facade<parser_iterator<CharT>, parser<CharT>, std::input_iterator_tag,
                                                    parser<CharT>&, parser<CharT>*> {
 public:
    using value_type = parser<CharT>;
    using reference = parser<CharT>&;

    parser_iterator() noexcept = default;
    explicit parser_iterator(parser<CharT>& parser) noexcept : parser_(&parser) {
        if (parser_->next() == token_t::eof) { parser_ = nullptr; }
    }

    void increment() {
        assert(parser_);
        if (parser_->next() == token_t::eof) { parser_ = nullptr; }
    }

    reference dereference() const noexcept {
        assert(parser_);
        return *parser_;
    }

    bool is_equal_to(const parser_iterator& it) const noexcept { return parser_ == it.parser_; }

 private:
    parser<CharT>* parser_ = nullptr;
};

template<typename InCharT>
class parser {
    static_assert(std::is_same<std::remove_cv_t<InCharT>, InCharT>::value,
                  "uxs::db::xml::parser<> must have a non-const, non-volatile character type");
    static_assert(est::is_character<InCharT>::value, "uxs::db::xml::parser<> defined for character types");

 public:
    using char_type = InCharT;
    using string_view_type = std::basic_string_view<char_type>;
    using iterator = parser_iterator<InCharT>;

    explicit parser(basic_ibuf<InCharT>& input) : lexer_(input), str_cache_(array_tag, 16) {}
    parser(const parser&) = delete;
    parser& operator=(const parser&) = delete;

    iterator begin() noexcept { return iterator(*this); }
    iterator end() noexcept { return iterator(); }

    token_t next() {
        std::tie(token_type_, lexeme_) = next_impl();
        return token_type_;
    }

    token_t token_type() const noexcept { return token_type_; }
    string_view_type name() const noexcept { return lexeme_; }
    string_view_type text() const noexcept { return lexeme_; }
    bool eof() const noexcept { return token_type_ == token_t::eof; }
    bool is_plain_text() const noexcept { return token_type_ == token_t::plain_text; }
    bool is_start_element() const noexcept { return token_type_ == token_t::start_element; }
    bool is_end_element() const noexcept { return token_type_ == token_t::end_element; }
    const basic_value<char_type>& attributes() const& noexcept { return attrs_; }
    basic_value<char_type>& attributes() & noexcept { return attrs_; }
    basic_value<char_type>&& attributes() && noexcept { return std::move(attrs_); }

    UXS_EXPORT static value_class classify_value(const string_view_type& val) noexcept;

    template<typename CharT = char, typename Alloc = std::allocator<CharT>>
    UXS_EXPORT basic_value<CharT, Alloc> parse(string_view_type root_element, const Alloc& al = Alloc());

 private:
    detail::lexer<InCharT> lexer_;
    bool is_end_element_pending_ = false;
    basic_value<char_type> str_cache_;
    token_t token_type_ = token_t::none;
    string_view_type lexeme_;
    basic_value<char_type> attrs_;

    UXS_EXPORT std::pair<token_t, string_view_type> next_impl();
};

template<std::size_t I, typename CharT, typename = std::enable_if_t<I == 0>>
auto get(const parser<CharT>& v) noexcept -> decltype(v.token_type()) {
    return v.token_type();
}
template<std::size_t I, typename CharT, typename = std::enable_if_t<I == 1>>
auto get(const parser<CharT>& v) noexcept -> decltype(v.text()) {
    return v.text();
}
template<std::size_t I, typename CharT, typename = std::enable_if_t<I == 2>>
auto get(const parser<CharT>& v) noexcept -> decltype(v.attributes()) {
    return v.attributes();
}
template<std::size_t I, typename CharT, typename = std::enable_if_t<I == 2>>
auto get(parser<CharT>& v) noexcept -> decltype(v.attributes()) {
    return v.attributes();
}
template<std::size_t I, typename CharT, typename = std::enable_if_t<I == 2>>
auto get(parser<CharT>&& v) noexcept -> decltype(std::move(v).attributes()) {
    return std::move(v).attributes();
}

namespace detail {
template<typename OutCharT, typename CharT, typename Alloc>
UXS_EXPORT void write_impl(basic_membuffer<OutCharT>& out, const basic_value<CharT, Alloc>& v,
                           std::basic_string_view<CharT> element, xml_fmt_opts opts, unsigned indent);
}

template<typename OutCharT, typename CharT, typename Alloc>
void write(basic_iobuf<OutCharT>& out, const basic_value<CharT, Alloc>& v,
           est::type_identity_t<std::basic_string_view<CharT>> element, xml_fmt_opts opts = {}, unsigned indent = 0) {
    basic_iomembuffer<OutCharT> buf(out);
    detail::write_impl<OutCharT, CharT, Alloc>(buf, v, element, opts, indent);
}

}  // namespace xml
}  // namespace db
}  // namespace uxs

namespace std {
template<typename CharT>
class tuple_size<uxs::db::xml::parser<CharT>> : public std::integral_constant<std::size_t, 3> {};
template<std::size_t I, typename CharT>
class tuple_element<I, uxs::db::xml::parser<CharT>> {
 public:
    using type = std::remove_cvref_t<decltype(uxs::db::xml::get<I>(std::declval<uxs::db::xml::parser<CharT>&>()))>;
};
}  // namespace std
