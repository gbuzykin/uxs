#pragma once

#include "uxs/format.h"

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

class reader {
 public:
    UXS_EXPORT explicit reader(ibuf& input);

    class iterator : public iterator_facade<iterator, std::pair<token_t, std::string_view>, std::input_iterator_tag,
                                            const std::pair<token_t, std::string_view>&,
                                            const std::pair<token_t, std::string_view>*> {
     public:
        using value_type = std::pair<token_t, std::string_view>;
        using reference = const value_type&;

        iterator() : val_(token_t::eof, {}) {}
        explicit iterator(reader& rd) : rd_(&rd) {
            if ((val_ = rd_->read_next()).first == token_t::eof) { rd_ = nullptr; }
        }

        void increment() {
            uxs_iterator_assert(rd_);
            if ((val_ = rd_->read_next()).first == token_t::eof) { rd_ = nullptr; }
        }

        std::map<std::string_view, std::string>& attributes() const {
            uxs_iterator_assert(rd_);
            return rd_->attrs_;
        }

        reference dereference() const { return val_; }
        bool is_equal_to(const iterator& it) const { return rd_ == it.rd_; }

     private:
        reader* rd_ = nullptr;
        value_type val_;
    };

    template<typename CharT = char, typename Alloc = std::allocator<CharT>>
    UXS_EXPORT basic_value<CharT, Alloc> read(std::string_view root_element, const Alloc& al = Alloc());

    UXS_EXPORT std::pair<token_t, std::string_view> read_next();

 private:
    ibuf& input_;
    int n_ln_ = 1;
    bool is_end_element_pending_ = false;
    inline_dynbuffer str_;
    inline_dynbuffer stash_;
    inline_basic_dynbuffer<std::int8_t> state_stack_;
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
        pi_close
    };

    lex_token_t parse_token(std::string_view& lval);

    enum class string_class : int {
        null = 0,
        true_value,
        false_value,
        integer_number,
        negative_integer_number,
        floating_point_number,
        ws_with_nl,
        other
    };

    static string_class classify_string(const std::string_view& sval);
};

class writer {
 public:
    explicit writer(iobuf& output, unsigned indent_sz = 4, char indent_ch = ' ')
        : output_(output), indent_size_(indent_sz), indent_char_(indent_ch) {}

    template<typename CharT, typename Alloc>
    UXS_EXPORT void write(const basic_value<CharT, Alloc>& v, std::string_view root_element, unsigned indent = 0);

 private:
    basic_membuffer_for_iobuf<char> output_;
    unsigned indent_size_;
    char indent_char_;
};

}  // namespace xml
}  // namespace db
}  // namespace uxs
