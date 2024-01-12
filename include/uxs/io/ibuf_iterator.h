#pragma once

#include "ibuf.h"

#include "uxs/iterator.h"

namespace uxs {

template<typename CharT>
class basic_ibuf_iterator : public iterator_facade<basic_ibuf_iterator<CharT>, CharT,
                                                   std::input_iterator_tag,  //
                                                   CharT, CharT, typename basic_ibuf<CharT>::traits_type::off_type> {
 public:
    using char_type = CharT;
    using ibuf_type = basic_ibuf<CharT>;
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

}  // namespace uxs
