#pragma once

#include "util/io/istringbuf.h"

using namespace util;

template<typename CharT>
basic_istringbuf<CharT>::basic_istringbuf(basic_istringbuf&& other) NOEXCEPT : basic_iobuf<CharT>(std::move(other)),
                                                                               str_(std::move(other.str_)) {
    if (this->first() != &str_[0]) { redirect_ptrs(); }
}

template<typename CharT>
basic_istringbuf<CharT>& basic_istringbuf<CharT>::operator=(basic_istringbuf&& other) NOEXCEPT {
    if (&other == this) { return *this; }
    static_cast<basic_iobuf<CharT>&>(*this) = std::move(other), str_ = std::move(other.str_);
    if (this->first() != &str_[0]) { redirect_ptrs(); }
    return *this;
}

template<typename CharT>
typename basic_istringbuf<CharT>::pos_type basic_istringbuf<CharT>::seekimpl(off_type off, seekdir dir) {
    size_t pos = this->curr() - this->first(), new_pos = pos;
    switch (dir) {
        case seekdir::kBeg: {
            if (off < 0) { return -1; }
            new_pos = off;
        } break;
        case seekdir::kCurr: {
            if (off == 0) { return pos; }
            if (off < 0 && static_cast<size_t>(-off) >= new_pos) { return -1; }
            new_pos += off;
        } break;
        case seekdir::kEnd: {
            size_t sz = this->last() - this->first();
            if (off < 0 && static_cast<size_t>(-off) >= sz) { return -1; }
            new_pos = sz + off;
        } break;
    }
    this->setcurr(this->first() + new_pos);
    return new_pos;
}

template<typename CharT>
void basic_istringbuf<CharT>::redirect_ptrs() {
    this->setview(&str_[0], &str_[this->curr() - this->first()], &str_[str_.size()]);
}
