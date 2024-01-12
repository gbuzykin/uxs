#pragma once

#include "ibuf.h"

#include <algorithm>

namespace uxs {

template<typename CharT>
basic_ibuf<CharT>::basic_ibuf(basic_ibuf&& other) noexcept
    : iostate(other), first_(other.first_), curr_(other.curr_), last_(other.last_) {
    other.first_ = other.curr_ = other.last_ = nullptr;
}

template<typename CharT>
basic_ibuf<CharT>& basic_ibuf<CharT>::operator=(basic_ibuf&& other) noexcept {
    if (&other == this) { return *this; }
    iostate::operator=(other);
    first_ = other.first_, curr_ = other.curr_, last_ = other.last_;
    other.first_ = other.curr_ = other.last_ = nullptr;
    return *this;
}

template<typename CharT>
typename basic_ibuf<CharT>::size_type basic_ibuf<CharT>::read(uxs::span<char_type> s) {
    auto p = s.begin();
    size_type count = s.end() - p;
    if (!count) { return 0; }
    for (size_type n_avail = avail(); count > n_avail; n_avail = avail()) {
        if (n_avail) { p = std::copy_n(curr_, n_avail, p), curr_ = last_, count -= n_avail; }
        if (!this->good() || underflow() < 0) {
            this->setstate(iostate_bits::eof | iostate_bits::fail);
            return p - s.begin();
        }
    }
    std::copy_n(curr_, count, p), curr_ += count;
    return s.size();
}

template<typename CharT>
typename basic_ibuf<CharT>::size_type basic_ibuf<CharT>::skip(size_type count) {
    if (!count) { return 0; }
    const size_type n0 = count;
    for (size_type n_avail = avail(); count > n_avail; n_avail = avail()) {
        curr_ = last_, count -= n_avail;
        if (!this->good() || underflow() < 0) {
            this->setstate(iostate_bits::eof | iostate_bits::fail);
            return n0 - count;
        }
    }
    curr_ += count;
    return n0;
}

template<typename CharT>
typename basic_ibuf<CharT>::pos_type basic_ibuf<CharT>::seek(off_type off, seekdir dir) {
    this->setstate(this->rdstate() & ~iostate_bits::eof);
    if (this->fail()) { return traits_type::npos(); }
    if (!!(this->mode() & iomode::out) && sync() < 0) {
        this->setstate(iostate_bits::fail);
        return traits_type::npos();
    }
    pos_type pos = seekimpl(off, dir);
    if (pos == traits_type::npos()) { this->setstate(iostate_bits::fail); }
    return pos;
}

template<typename CharT>
int basic_ibuf<CharT>::underflow() {
    return -1;
}

template<typename CharT>
int basic_ibuf<CharT>::ungetfail() {
    return -1;
}

template<typename CharT>
typename basic_ibuf<CharT>::pos_type basic_ibuf<CharT>::seekimpl(off_type off, seekdir dir) {
    return traits_type::npos();
}

template<typename CharT>
int basic_ibuf<CharT>::sync() {
    return -1;
}

}  // namespace uxs
