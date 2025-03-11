#pragma once

#include "uxs/io/iflatbuf.h"

namespace uxs {

template<typename CharT>
typename basic_iflatbuf<CharT>::pos_type basic_iflatbuf<CharT>::seek_impl(off_type off, seekdir dir) {
    const size_type sz = this->last() - this->first();
    size_type pos = this->curr() - this->first();
    switch (dir) {
        case seekdir::beg: {
            pos = static_cast<std::size_t>(off > 0 ? (static_cast<size_type>(off) < sz ? off : sz) : 0);
        } break;
        case seekdir::curr: {
            if (off == 0) { return pos; }
            pos = static_cast<std::size_t>(off > 0 ? (static_cast<size_type>(off) < sz - pos ? pos + off : sz) :
                                                     (static_cast<size_type>(-off) < pos ? pos + off : 0));
        } break;
        case seekdir::end: {
            pos = static_cast<std::size_t>(off < 0 ? (static_cast<size_type>(-off) < sz ? sz + off : 0) : sz);
        } break;
    }
    this->setcurr(this->first() + pos);
    return static_cast<pos_type>(pos);
}

}  // namespace uxs
