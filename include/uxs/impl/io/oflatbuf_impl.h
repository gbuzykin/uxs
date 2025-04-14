#pragma once

#include "uxs/io/oflatbuf.h"

#include <stdexcept>

namespace uxs {

template<typename CharT, typename Alloc>
basic_oflatbuf<CharT, Alloc>::~basic_oflatbuf() {
    if (this->first() != nullptr) { this->deallocate(this->first(), this->capacity()); }
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
int basic_oflatbuf<CharT, Alloc>::truncate_impl() {
    top_ = this->pos();
    return 0;
}

template<typename CharT, typename Alloc>
auto basic_oflatbuf<CharT, Alloc>::seek_impl(off_type off, seekdir dir) -> pos_type {
    top_ = size();
    size_type pos = this->pos();
    switch (dir) {
        case seekdir::beg: {
            pos = off > 0 ? static_cast<size_type>(off) : 0;
        } break;
        case seekdir::curr: {
            if (off == 0) { return pos; }
            pos = off > 0 || -off < static_cast<off_type>(pos) ? static_cast<size_type>(pos + off) : size_type(0);
        } break;
        case seekdir::end: {
            pos = off > 0 || -off < static_cast<off_type>(top_) ? static_cast<size_type>(top_ + off) : size_type(0);
        } break;
    }
    if (pos > this->capacity()) { grow(pos - top_); }
    if (pos > top_) { std::fill_n(this->first() + top_, pos - top_, '\0'); }
    this->setpos(pos);
    return static_cast<pos_type>(pos);
}

template<typename CharT, typename Alloc>
void basic_oflatbuf<CharT, Alloc>::grow(size_type extra) {
    top_ = size();
    size_type delta_sz = std::max(extra, top_ >> 1);
    const size_type max_avail = std::allocator_traits<alloc_type>::max_size(*this) - top_;
    if (delta_sz > max_avail) {
        if (extra > max_avail) { throw std::length_error("too much to reserve"); }
        delta_sz = std::max(extra, max_avail >> 1);
    }
    const size_type sz = std::max<size_type>(top_ + delta_sz, min_buf_size);
    char_type* first = this->allocate(sz);
    if (this->first() != nullptr) {
        std::copy_n(this->first(), top_, first);
        this->deallocate(this->first(), this->capacity());
    }
    this->reset(first, this->pos(), sz);
}

}  // namespace uxs
