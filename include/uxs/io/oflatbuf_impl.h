#pragma once

#include "oflatbuf.h"

#include <stdexcept>

namespace uxs {

template<typename CharT, typename Alloc>
basic_oflatbuf<CharT, Alloc>::~basic_oflatbuf() {
    if (this->first() != nullptr) { this->deallocate(this->first(), this->last() - this->first()); }
}

template<typename CharT, typename Alloc>
basic_oflatbuf<CharT, Alloc>::basic_oflatbuf(basic_oflatbuf&& other) noexcept
    : alloc_type(std::move(other)), basic_iobuf<CharT>(std::move(other)), top_(other.top_) {}

template<typename CharT, typename Alloc>
basic_oflatbuf<CharT, Alloc>& basic_oflatbuf<CharT, Alloc>::operator=(basic_oflatbuf&& other) noexcept {
    if (&other == this) { return *this; }
    alloc_type::operator=(std::move(other));
    basic_iobuf<CharT>::operator=(std::move(other));
    top_ = other.top_;
    return *this;
}

template<typename CharT, typename Alloc>
int basic_oflatbuf<CharT, Alloc>::overflow() {
    grow(1);
    return 0;
}

template<typename CharT, typename Alloc>
int basic_oflatbuf<CharT, Alloc>::sync() {
    return 0;
}

template<typename CharT, typename Alloc>
typename basic_oflatbuf<CharT, Alloc>::pos_type basic_oflatbuf<CharT, Alloc>::seekimpl(off_type off, seekdir dir) {
    top_ = std::max(top_, this->curr());
    const size_type sz = top_ - this->first();
    size_type pos = this->curr() - this->first();
    switch (dir) {
        case seekdir::beg: {
            pos = off > 0 ? static_cast<size_type>(off) : 0;
        } break;
        case seekdir::curr: {
            if (off == 0) { return pos; }
            pos = static_cast<size_t>(off > 0 || static_cast<size_type>(-off) < pos ? pos + off : 0);
        } break;
        case seekdir::end: {
            pos = static_cast<size_t>(off > 0 || static_cast<size_type>(-off) < sz ? sz + off : 0);
        } break;
    }
    const size_type capacity = this->last() - this->first();
    if (pos > capacity) { grow(pos - sz); }
    this->setcurr(this->first() + pos);
    if (this->curr() > top_) { std::fill(top_, this->curr(), '\0'); }
    return static_cast<pos_type>(pos);
}

template<typename CharT, typename Alloc>
void basic_oflatbuf<CharT, Alloc>::grow(size_type extra) {
    top_ = std::max(top_, this->curr());
    size_type sz = top_ - this->first(), delta_sz = std::max(extra, sz >> 1);
    const size_type max_avail = std::allocator_traits<alloc_type>::max_size(*this) - sz;
    if (delta_sz > max_avail) {
        if (extra > max_avail) { throw std::length_error("too much to reserve"); }
        delta_sz = std::max(extra, max_avail >> 1);
    }
    sz = std::max<size_type>(sz + delta_sz, min_buf_size);
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
