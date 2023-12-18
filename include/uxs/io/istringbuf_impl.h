#pragma once

#include "istringbuf.h"

namespace uxs {

template<typename CharT>
basic_istringbuf<CharT>::basic_istringbuf(basic_istringbuf&& other) NOEXCEPT : basic_iobuf<CharT>(std::move(other)),
                                                                               str_(std::move(other.str_)) {
    if (this->first() != &str_[0]) { redirect_ptrs(); }
}

template<typename CharT>
basic_istringbuf<CharT>& basic_istringbuf<CharT>::operator=(basic_istringbuf&& other) NOEXCEPT {
    if (&other == this) { return *this; }
    basic_iobuf<CharT>::operator=(std::move(other));
    str_ = std::move(other.str_);
    if (this->first() != &str_[0]) { redirect_ptrs(); }
    return *this;
}

template<typename CharT>
typename basic_istringbuf<CharT>::pos_type basic_istringbuf<CharT>::seekimpl(off_type off, seekdir dir) {
    size_type pos = this->curr() - this->first(), new_pos = pos;
    switch (dir) {
        case seekdir::beg: {
            if (off < 0) { return -1; }
            new_pos = static_cast<size_type>(off);
        } break;
        case seekdir::curr: {
            if (off == 0) { return pos; }
            if (off < 0 && static_cast<size_t>(-off) >= new_pos) { return -1; }
            new_pos += static_cast<size_type>(off);
        } break;
        case seekdir::end: {
            size_t sz = this->last() - this->first();
            if (off < 0 && static_cast<size_t>(-off) >= sz) { return -1; }
            new_pos = sz + static_cast<size_type>(off);
        } break;
    }
    this->setcurr(this->first() + new_pos);
    return new_pos;
}

}  // namespace uxs
