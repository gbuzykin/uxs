#pragma once

#include "uxs/io/ibuf.h"

namespace uxs {

template<typename CharT>
basic_ibuf<CharT>::basic_ibuf(basic_ibuf&& other) noexcept
    : iostate(other), pbase_(other.pbase_), pos_(other.pos_), capacity_(other.capacity_) {
    other.pbase_ = nullptr;
}

template<typename CharT>
basic_ibuf<CharT>& basic_ibuf<CharT>::operator=(basic_ibuf&& other) noexcept {
    if (&other == this) { return *this; }
    iostate::operator=(other);
    pbase_ = other.pbase_, pos_ = other.pos_, capacity_ = other.capacity_;
    other.pbase_ = nullptr;
    return *this;
}

template<typename CharT>
auto basic_ibuf<CharT>::read(est::span<char_type> s) -> size_type {
    return read(s.begin(), s.end());
}

template<typename CharT>
auto basic_ibuf<CharT>::read_with_endian(est::span<char_type> s, size_type element_sz) -> size_type {
    if (!(this->mode() & iomode::invert_endian) || element_sz <= 1) { return read(s.begin(), s.end()); }
    size_type count = 0;
    auto p = s.begin();
    while (element_sz < static_cast<size_type>(s.end() - p)) {
        count += read(std::make_reverse_iterator(p + element_sz), std::make_reverse_iterator(p));
        p += element_sz;
    }
    return count + read(std::make_reverse_iterator(s.end()), std::make_reverse_iterator(p));
}

template<typename CharT>
auto basic_ibuf<CharT>::skip(size_type count) -> size_type {
    if (!count) { return 0; }
    const size_type n0 = count;
    for (size_type n_avail = avail(); count > n_avail; n_avail = avail()) {
        pos_ = capacity_, count -= n_avail;
        if (!this->good() || underflow() < 0) {
            this->setstate(iostate_bits::eof | iostate_bits::fail);
            return n0 - count;
        }
    }
    pos_ += count;
    return n0;
}

template<typename CharT>
auto basic_ibuf<CharT>::seek(off_type off, seekdir dir) -> pos_type {
    this->setstate(this->rdstate() & ~iostate_bits::eof);
    if (this->fail()) { return traits_type::npos(); }
    if (!!(this->mode() & iomode::out) && sync() < 0) {
        this->setstate(iostate_bits::fail);
        return traits_type::npos();
    }
    const pos_type pos = seek_impl(off, dir);
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
auto basic_ibuf<CharT>::seek_impl(off_type /*off*/, seekdir /*dir*/) -> pos_type {
    return traits_type::npos();
}

template<typename CharT>
int basic_ibuf<CharT>::sync() {
    return -1;
}

}  // namespace uxs
