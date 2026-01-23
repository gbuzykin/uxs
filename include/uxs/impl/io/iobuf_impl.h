#pragma once

#include "uxs/io/iobuf.h"

namespace uxs {

template<typename CharT>
basic_iobuf<CharT>& basic_iobuf<CharT>::write(est::span<const char_type> s) {
    return write(s.begin(), s.end());
}

template<typename CharT>
basic_iobuf<CharT>& basic_iobuf<CharT>::write_with_endian(est::span<const char_type> s, size_type element_sz) {
    if (!(this->mode() & iomode::invert_endian) || element_sz <= 1) { return write(s.begin(), s.end()); }
    auto p = s.begin();
    while (element_sz < static_cast<size_type>(s.end() - p)) {
        write(std::make_reverse_iterator(p + element_sz), std::make_reverse_iterator(p));
        p += element_sz;
    }
    return write(std::make_reverse_iterator(s.end()), std::make_reverse_iterator(p));
}

template<typename CharT>
basic_iobuf<CharT>& basic_iobuf<CharT>::fill_n(size_type count, char_type ch) {
    if (!count) { return *this; }
    for (size_type n_avail = this->avail(); count > n_avail; n_avail = this->avail()) {
        std::fill_n(curr(), n_avail, ch);
        this->setpos(this->capacity());
        count -= n_avail;
        if (!this->good() || overflow() < 0) {
            this->setstate(iostate_bits::bad);
            return *this;
        }
    }
    std::fill_n(curr(), count, ch);
    this->advance(count);
    return *this;
}

template<typename CharT>
basic_iobuf<CharT>& basic_iobuf<CharT>::flush() {
    if (!this->good() || this->sync() < 0) { this->setstate(iostate_bits::bad); }
    return *this;
}

template<typename CharT>
void basic_iobuf<CharT>::truncate() {
    if (!flush() || truncate_impl() < 0) { this->setstate(iostate_bits::bad); }
}

template<typename CharT>
int basic_iobuf<CharT>::overflow() {
    return -1;
}

template<typename CharT>
int basic_iobuf<CharT>::truncate_impl() {
    return -1;
}

}  // namespace uxs
