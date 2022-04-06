#pragma once

#include "util/io/ostringbuf.h"

#include <algorithm>

using namespace util;

template<typename CharT>
basic_ostringbuf<CharT>::~basic_ostringbuf() {
    if (this->first() != nullptr) {
        std::allocator<char_type>().deallocate(this->first(), this->last() - this->first());
    }
}

template<typename CharT>
basic_ostringbuf<CharT>::basic_ostringbuf(basic_ostringbuf&& other) NOEXCEPT : basic_iobuf<CharT>(std::move(other)),
                                                                               top_(other.top_) {
    other.top_ = nullptr;
}

template<typename CharT>
basic_ostringbuf<CharT>& basic_ostringbuf<CharT>::operator=(basic_ostringbuf&& other) NOEXCEPT {
    if (&other == this) { return *this; }
    static_cast<basic_iobuf<CharT>&>(*this) = std::move(other), top_ = other.top_;
    other.top_ = nullptr;
    return *this;
}

template<typename CharT>
int basic_ostringbuf<CharT>::overflow(char_type ch) {
    grow(1);
    *this->curr() = ch;
    this->bump(1);
    return 0;
}

template<typename CharT>
int basic_ostringbuf<CharT>::sync() {
    return 0;
}

template<typename CharT>
typename basic_ostringbuf<CharT>::pos_type basic_ostringbuf<CharT>::seekimpl(off_type off, seekdir dir) {
    top_ = std::max(top_, this->curr());
    size_type sz = top_ - this->first();
    size_type pos = this->curr() - this->first(), new_pos = pos;
    switch (dir) {
        case seekdir::kBeg: {
            if (off < 0) { return -1; }
            new_pos = off;
        } break;
        case seekdir::kCurr: {
            if (off == 0) { return pos; }
            if (off < 0 && static_cast<size_type>(-off) >= new_pos) { return -1; }
            new_pos += off;
        } break;
        case seekdir::kEnd: {
            if (off < 0 && static_cast<size_type>(-off) >= sz) { return -1; }
            new_pos = sz + off;
        } break;
    }
    size_type capacity = this->last() - this->first();
    if (new_pos > capacity) { grow(new_pos - sz); }
    this->setcurr(this->first() + new_pos);
    if (this->curr() > top_) { std::fill(top_, this->curr(), '\0'); }
    return new_pos;
}

template<typename CharT>
void basic_ostringbuf<CharT>::grow(size_type extra) {
    top_ = std::max(top_, this->curr());
    size_type sz = top_ - this->first();
    sz = std::max<size_type>(sz + std::max(extra, sz >> 1), kMinBufSize);
    char_type* first = std::allocator<char_type>().allocate(sz);
    if (this->first() != nullptr) {
        top_ = std::copy(this->first(), top_, first);
        std::allocator<char_type>().deallocate(this->first(), this->last() - this->first());
    } else {
        top_ = first;
    }
    this->setview(first, first + static_cast<size_type>(this->curr() - this->first()), first + sz);
}
