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
    plain_text,
    start_element,
    end_element,
    entity,
    preamble,
};

enum class value_class : int {
    null = 0,
    true_value,
    false_value,
    integer_number,
    negative_integer_number,
    floating_point_number,
    ws_with_nl,
    other,
};

class parser {
 public:
    UXS_EXPORT explicit parser(ibuf& input);
    UXS_EXPORT std::pair<token_t, std::string_view> next();
    UXS_EXPORT static value_class classify_value(const std::string_view& sval);

    template<typename CharT = char, typename Alloc = std::allocator<CharT>>
    UXS_EXPORT basic_value<CharT, Alloc> read(std::string_view root_element, const Alloc& al = Alloc());

    class iterator : public iterator_facade<iterator, std::pair<token_t, std::string_view>, std::input_iterator_tag,
                                            const std::pair<token_t, std::string_view>&,
                                            const std::pair<token_t, std::string_view>*> {
     public:
        using value_type = std::pair<token_t, std::string_view>;
        using reference = const value_type&;

        iterator() : val_(token_t::eof, {}) {}
        explicit iterator(parser& rd) : parser_(&rd) {
            if ((val_ = parser_->next()).first == token_t::eof) { parser_ = nullptr; }
        }

        void increment() {
            uxs_iterator_assert(parser_);
            if ((val_ = parser_->next()).first == token_t::eof) { parser_ = nullptr; }
        }

        std::map<std::string_view, std::string>& attributes() const {
            uxs_iterator_assert(parser_);
            return parser_->attrs_;
        }

        reference dereference() const { return val_; }
        value_class classify() const { return classify_value(val_.second); }
        bool is_equal_to(const iterator& it) const { return parser_ == it.parser_; }

     private:
        parser* parser_ = nullptr;
        value_type val_;
    };

 private:
    ibuf& in_;
    int ln_ = 1;
    bool is_end_element_pending_ = false;
    inline_dynbuffer str_;
    inline_dynbuffer stash_;
    inline_basic_dynbuffer<std::int8_t> stack_;
    std::forward_list<std::string> name_cache_;
    std::map<std::string_view, std::string> attrs_;

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
