#pragma once

#include "uxs/io/iflatbuf.h"

namespace uxs {

template<typename CharT>
auto basic_iflatbuf<CharT>::seek_impl(off_type off, seekdir dir) -> pos_type {
    const size_type sz = this->capacity();
    size_type pos = this->pos();
    switch (dir) {
        case seekdir::beg: {
            pos = off > 0 ? (off < static_cast<off_type>(sz) ? off : static_cast<size_type>(sz)) : size_type(0);
        } break;
        case seekdir::curr: {
            if (off == 0) { return pos; }
            pos = off > 0 ? (off < static_cast<off_type>(sz - pos) ? static_cast<size_type>(pos + off) : sz) :
                            (-off < static_cast<off_type>(pos) ? static_cast<size_type>(pos + off) : size_type(0));
        } break;
        case seekdir::end: {
            pos = off < 0 ? (-off < static_cast<off_type>(sz) ? static_cast<size_type>(sz + off) : size_type(0)) : sz;
        } break;
    }
    this->setpos(pos);
    return static_cast<pos_type>(pos);
}

}  // namespace uxs
