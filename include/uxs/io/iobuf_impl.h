#pragma once

#include "iobuf.h"

namespace uxs {

template<typename CharT>
basic_iobuf<CharT>& basic_iobuf<CharT>::write(uxs::span<const char_type> s) {
    return write(s.begin(), s.end());
}

template<typename CharT>
basic_iobuf<CharT>& basic_iobuf<CharT>::fill_n(size_type count, char_type ch) {
    if (!count) { return *this; }
    for (size_type n_avail = this->avail(); count > n_avail; n_avail = this->avail()) {
        if (n_avail) { this->setcurr(std::fill_n(this->curr(), n_avail, ch)), count -= n_avail; }
        if (!this->good() || overflow() < 0) {
            this->setstate(iostate_bits::bad);
            return *this;
        }
    }
    this->setcurr(std::fill_n(this->curr(), count, ch));
    return *this;
}

template<typename CharT>
basic_iobuf<CharT>& basic_iobuf<CharT>::flush() {
    if (!this->good() || this->sync() < 0) { this->setstate(iostate_bits::bad); }
    return *this;
}

template<typename CharT>
int basic_iobuf<CharT>::overflow() {
    return -1;
}

}  // namespace uxs
