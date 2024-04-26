#pragma once

#include "iobuf.h"

namespace uxs {

template<typename CharT>
class basic_obuf_iterator {
 public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using reference = void;
    using pointer = void;
    using char_type = CharT;
    using obuf_type = basic_iobuf<CharT>;
    using traits_type = typename obuf_type::traits_type;
    using int_type = typename traits_type::int_type;

    explicit basic_obuf_iterator(obuf_type& buf) noexcept : buf_(&buf) {}

    basic_obuf_iterator& operator=(char_type ch) {
        iterator_assert(buf_);
        buf_->put(ch);
        return *this;
    }

    bool failed() const noexcept { return buf_->eof(); }

    basic_obuf_iterator& operator*() { return *this; }
    basic_obuf_iterator& operator++() { return *this; }
    basic_obuf_iterator operator++(int) { return *this; }

 private:
    obuf_type* buf_;
};

using obuf_iterator = basic_obuf_iterator<char>;
using wobuf_iterator = basic_obuf_iterator<wchar_t>;
using u8obuf_iterator = basic_obuf_iterator<std::uint8_t>;

}  // namespace uxs
