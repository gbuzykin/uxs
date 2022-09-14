#pragma once

#include "uxs/io/iobuf.h"
#include "uxs/iterator.h"
#include "uxs/stringcvt.h"

#include <forward_list>
#include <map>

namespace uxs {
namespace db {
template<typename CharT, typename Alloc>
class basic_value;

namespace xml {

enum class token_t : int {
    kEof = 0,
    kPlainText,
    kStartElement,
    kEndElement,
    kEntity,
    kPreamble,
};

class UXS_EXPORT reader {
 public:
    explicit reader(iobuf& input);

    class iterator : public iterator_facade<iterator, std::pair<token_t, std::string_view>, std::input_iterator_tag,
                                            const std::pair<token_t, std::string_view>&,
                                            const std::pair<token_t, std::string_view>*> {
     public:
        using value_type = std::pair<token_t, std::string_view>;
        using reference = const value_type&;

        iterator() : val_(token_t::kEof, {}) {}
        explicit iterator(reader& rd) : rd_(&rd) {
            if ((val_ = rd_->read_next()).first == token_t::kEof) { rd_ = nullptr; }
        }

        void increment() {
            iterator_assert(rd_);
            if ((val_ = rd_->read_next()).first == token_t::kEof) { rd_ = nullptr; }
        }

        std::map<std::string_view, std::string>& attributes() const {
            iterator_assert(rd_);
            return rd_->attrs_;
        }

        reference dereference() const { return val_; }
        bool is_equal_to(const iterator& it) const { return rd_ == it.rd_; }

     private:
        reader* rd_ = nullptr;
        value_type val_;
    };

    template<typename CharT = char, typename Alloc = std::allocator<CharT>>
    basic_value<CharT, Alloc> read(std::string_view root_element, const Alloc& al = Alloc());

    std::pair<token_t, std::string_view> read_next();

 private:
    iobuf& input_;
    int n_ln_ = 1;
    bool is_end_element_pending_ = false;
    inline_dynbuffer str_;
    inline_dynbuffer stash_;
    basic_inline_dynbuffer<int8_t> state_stack_;
    std::forward_list<std::string> name_cache_;
    std::map<std::string_view, std::string> attrs_;

    enum {
        kEof = 0,
        kName = 256,
        kPredefEntity,
        kEntity,
        kString,
        kStartElementOpen,
        kEndElementOpen,
        kPiOpen,
        kComment,
        kEndElementClose,
        kPiClose
    };

    int parse_token(std::string_view& lval);

    enum class string_class : int { kNull = 0, kTrue, kFalse, kInteger, kNegInteger, kDouble, kWsWithNl, kOther };

    static string_class classify_string(const std::string_view& sval);
};

class UXS_EXPORT writer {
 public:
    explicit writer(iobuf& output, unsigned indent_sz = 4, char indent_ch = ' ')
        : output_(output), indent_size_(indent_sz), indent_char_(indent_ch) {}

    template<typename CharT, typename Alloc>
    void write(const basic_value<CharT, Alloc>& v, std::string_view root_element, unsigned indent = 0);

 private:
    iobuf& output_;
    unsigned indent_size_;
    char indent_char_;
};

}  // namespace xml
}  // namespace db
}  // namespace uxs
