#pragma once

#include "ostringbuf.h"

#include <algorithm>

namespace uxs {

template<typename CharT, typename Alloc>
basic_ostringbuf<CharT, Alloc>::~basic_ostringbuf() {
    if (this->first() != nullptr) { this->deallocate(this->first(), this->last() - this->first()); }
}

template<typename CharT, typename Alloc>
basic_ostringbuf<CharT, Alloc>::basic_ostringbuf(basic_ostringbuf&& other) NOEXCEPT
    : alloc_type(std::move(other)),
      basic_iobuf<CharT>(std::move(other)),
      top_(other.top_) {}

template<typename CharT, typename Alloc>
basic_ostringbuf<CharT, Alloc>& basic_ostringbuf<CharT, Alloc>::operator=(basic_ostringbuf&& other) NOEXCEPT {
    if (&other == this) { return *this; }
    alloc_type::operator=(std::move(other));
    basic_iobuf<CharT>::operator=(std::move(other));
    top_ = other.top_;
    return *this;
}

template<typename CharT, typename Alloc>
int basic_ostringbuf<CharT, Alloc>::overflow() {
    grow(1);
    return 0;
}

template<typename CharT, typename Alloc>
int basic_ostringbuf<CharT, Alloc>::sync() {
    return 0;
}

template<typename CharT, typename Alloc>
typename basic_ostringbuf<CharT, Alloc>::pos_type basic_ostringbuf<CharT, Alloc>::seekimpl(off_type off, seekdir dir) {
    top_ = std::max(top_, this->curr());
    size_type sz = top_ - this->first();
    size_type pos = this->curr() - this->first(), new_pos = pos;
    switch (dir) {
        case seekdir::kBeg: {
            if (off < 0) { return -1; }
            new_pos = static_cast<size_type>(off);
        } break;
        case seekdir::kCurr: {
            if (off == 0) { return pos; }
            if (off < 0 && static_cast<size_type>(-off) >= new_pos) { return -1; }
            new_pos += static_cast<size_type>(off);
        } break;
        case seekdir::kEnd: {
            if (off < 0 && static_cast<size_type>(-off) >= sz) { return -1; }
            new_pos = sz + static_cast<size_type>(off);
        } break;
    }
    size_type capacity = this->last() - this->first();
    if (new_pos > capacity) { grow(new_pos - sz); }
    this->setcurr(this->first() + new_pos);
    if (this->curr() > top_) { std::fill(top_, this->curr(), '\0'); }
    return new_pos;
}

template<typename CharT, typename Alloc>
void basic_ostringbuf<CharT, Alloc>::grow(size_type extra) {
    top_ = std::max(top_, this->curr());
    size_type sz = top_ - this->first(), delta_sz = std::max(extra, sz >> 1);
    const size_type max_avail = std::allocator_traits<alloc_type>::max_size(*this) - sz;
    if (delta_sz > max_avail) {
        if (extra > max_avail) { throw std::length_error("too much to reserve"); }
        delta_sz = std::max(extra, max_avail >> 1);
    }
    sz = std::max<size_type>(sz + delta_sz, kMinBufSize);
    char_type* first = this->allocate(sz);
    if (this->first() != nullptr) {
        top_ = std::copy(this->first(), top_, first);
        this->deallocate(this->first(), this->last() - this->first());
    } else {
        top_ = first;
    }
    this->setview(first, first + static_cast<size_type>(this->curr() - this->first()), first + sz);
}

}  // namespace uxs
