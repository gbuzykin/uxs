#pragma once

#include "uxs/io/iomembuffer.h"

#include <forward_list>
#include <map>

namespace uxs {
namespace db {
template<typename CharT, typename Alloc>
class basic_value;

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

class attributes_t : public std::map<std::string_view, std::string> {
 public:
    using underlying_t = std::map<std::string_view, std::string>;

    attributes_t() = default;
#if __cplusplus < 201703L
    ~attributes_t() = default;
    attributes_t(const attributes_t&) = default;
    attributes_t& operator=(const attributes_t&) = default;
    attributes_t(attributes_t&& other) noexcept : underlying_t(std::move(other)) {}
    attributes_t& operator=(attributes_t&& other) noexcept {
        underlying_t::operator=(std::move(other));
        return *this;
    }
#endif  // __cplusplus < 201703L

    bool contains(std::string_view key) const { return find(key) != end(); }

    std::string_view value_or(std::string_view key, std::string_view default_value) const {
        auto it = find(key);
        return it != end() ? it->second : default_value;
    }

    std::string_view value(std::string_view key) const { return value_or(key, std::string_view()); }

    template<typename Ty, typename U, typename = std::enable_if_t<uxs::convertible_from_string<Ty>::value>>
    Ty value_or(std::string_view key, U&& default_value) const {
        auto it = find(key);
        return it != end() ? from_string<Ty>(it->second) : Ty(std::forward<U>(default_value));
    }

    template<typename Ty, typename = std::enable_if_t<uxs::convertible_from_string<Ty>::value>>
    Ty value(std::string_view key) const {
        return value_or<Ty>(key, Ty());
    }
};

class parser {
 public:
    using value_type = std::pair<token_t, std::string_view>;

    UXS_EXPORT explicit parser(ibuf& input);
    UXS_EXPORT static value_class classify_value(const std::string_view& sval);

    token_t next() {
        token_ = next_impl();
        return token_.first;
    }

    token_t token_type() const { return token_.first; }
    const value_type& token() const { return token_; }
    std::string_view name() const { return token_.second; }
    std::string_view text() const { return token_.second; }
    bool eof() const { return token_.first == token_t::eof; }
    bool is_plain_text() const { return token_.first == token_t::plain_text; }
    bool is_start_element() const { return token_.first == token_t::start_element; }
    bool is_end_element() const { return token_.first == token_t::end_element; }
    const attributes_t& attributes() const { return attrs_; }
    attributes_t& attributes() { return attrs_; }

    template<typename CharT = char, typename Alloc = std::allocator<CharT>>
    UXS_EXPORT basic_value<CharT, Alloc> read(std::string_view root_element, const Alloc& al = Alloc());

    class iterator
        : public iterator_facade<iterator, value_type, std::input_iterator_tag, const value_type&, const value_type*> {
     public:
        using value_type = value_type;
        using reference = const value_type&;

        iterator() = default;
        explicit iterator(parser& parser) : parser_(&parser) {
            if (parser_->next() == token_t::eof) { parser_ = nullptr; }
        }

        void increment() {
            uxs_iterator_assert(parser_);
            if (parser_->next() == token_t::eof) { parser_ = nullptr; }
        }

        const attributes_t& attributes() const {
            uxs_iterator_assert(parser_);
            return parser_->attributes();
        }

        attributes_t& attributes() {
            uxs_iterator_assert(parser_);
            return parser_->attributes();
        }

        reference dereference() const {
            uxs_iterator_assert(parser_);
            return parser_->token();
        }

        bool is_equal_to(const iterator& it) const { return parser_ == it.parser_; }

     private:
        parser* parser_ = nullptr;
    };

 private:
    ibuf& in_;
    int ln_ = 1;
    bool is_end_element_pending_ = false;
    inline_dynbuffer str_;
    inline_dynbuffer stash_;
    inline_basic_dynbuffer<std::int8_t> stack_;
    std::forward_list<std::string> name_cache_;
    std::pair<token_t, std::string_view> token_;
    attributes_t attrs_;

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

    UXS_EXPORT std::pair<token_t, std::string_view> next_impl();
    lex_token_t lex(std::string_view& lval);
};

namespace detail {
template<typename CharT>
struct writer {
    basic_membuffer<CharT>& out;
    unsigned indent_size;
    char indent_char;
    template<typename ValueCharT, typename Alloc>
    UXS_EXPORT void do_write(const basic_value<ValueCharT, Alloc>& v, std::basic_string_view<ValueCharT> element,
                             unsigned indent);
};
}  // namespace detail

template<typename CharT, typename ValueCharT, typename Alloc>
void write(basic_membuffer<CharT>& out, const basic_value<ValueCharT, Alloc>& v,
           est::type_identity_t<std::basic_string_view<ValueCharT>> element, unsigned indent_size = 0,
           char indent_char = ' ', unsigned indent = 0) {
    detail::writer<CharT> writer{out, indent_size, indent_char};
    writer.do_write(v, element, indent);
}

template<typename CharT, typename ValueCharT, typename Alloc>
void write(basic_iobuf<CharT>& out, const basic_value<ValueCharT, Alloc>& v,
           est::type_identity_t<std::basic_string_view<ValueCharT>> element, unsigned indent_size = 0,
           char indent_char = ' ', unsigned indent = 0) {
    basic_iomembuffer<CharT> buf(out);
    detail::writer<CharT> writer{buf, indent_size, indent_char};
    writer.do_write(v, element, indent);
}

}  // namespace xml
}  // namespace db
}  // namespace uxs
