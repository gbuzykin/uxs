#pragma once

#include "iobuf.h"

namespace uxs {

template<typename CharT>
class basic_ibuf_iterator : public iterator_facade<basic_ibuf_iterator<CharT>, CharT,
                                                   std::input_iterator_tag,  //
                                                   CharT, CharT, typename basic_iobuf<CharT>::traits_type::off_type> {
 public:
    using char_type = CharT;
    using ibuf_type = basic_iobuf<CharT>;
    using traits_type = typename ibuf_type::traits_type;
    using int_type = typename traits_type::int_type;

    basic_ibuf_iterator() = default;
    explicit basic_ibuf_iterator(ibuf_type& buf) : buf_(&buf) {
        if ((val_ = buf_->peek()) == traits_type::eof()) { buf_ = nullptr; }
    }

    void increment() {
        iterator_assert(buf_);
        buf_->advance(1);
        if ((val_ = buf_->peek()) == traits_type::eof()) { buf_ = nullptr; }
    }

    char_type dereference() const { return traits_type::to_char_type(val_); }
    bool is_equal_to(const basic_ibuf_iterator& it) const { return buf_ == it.buf_; }

 private:
    ibuf_type* buf_ = nullptr;
    int_type val_ = traits_type::eof();
};

using ibuf_iterator = basic_ibuf_iterator<char>;
using wibuf_iterator = basic_ibuf_iterator<wchar_t>;
using u8ibuf_iterator = basic_ibuf_iterator<uint8_t>;

#if UXS_USE_CHECKED_ITERATORS != 0
template<typename CharT>
struct std::_Is_checked_helper<basic_ibuf_iterator<CharT>> : std::true_type {};
#endif  // UXS_USE_CHECKED_ITERATORS

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

    explicit basic_obuf_iterator(obuf_type& buf) : buf_(&buf) {}

    basic_obuf_iterator& operator=(char_type ch) {
        iterator_assert(buf_);
        buf_->put(ch);
        return *this;
    }

    bool failed() const { return buf_->eof(); }

    basic_obuf_iterator& operator*() { return *this; }
    basic_obuf_iterator& operator++() { return *this; }
    basic_obuf_iterator operator++(int) { return *this; }

 private:
    obuf_type* buf_;
};

using obuf_iterator = basic_obuf_iterator<char>;
using wobuf_iterator = basic_obuf_iterator<wchar_t>;
using u8obuf_iterator = basic_obuf_iterator<uint8_t>;

#if UXS_USE_CHECKED_ITERATORS != 0
template<typename CharT>
struct std::_Is_checked_helper<basic_obuf_iterator<CharT>> : std::true_type {};
#endif  // UXS_USE_CHECKED_ITERATORS

}  // namespace uxs
