#pragma once

#include "iobuf.h"

namespace uxs {

template<typename CharT>
basic_iobuf<CharT>::basic_iobuf(basic_iobuf&& other) NOEXCEPT : iostate(other),
                                                                first_(other.first_),
                                                                curr_(other.curr_),
                                                                last_(other.last_) {
    other.first_ = other.curr_ = other.last_ = nullptr;
}

template<typename CharT>
basic_iobuf<CharT>& basic_iobuf<CharT>::operator=(basic_iobuf&& other) NOEXCEPT {
    if (&other == this) { return *this; }
    iostate::operator=(other);
    first_ = other.first_, curr_ = other.curr_, last_ = other.last_;
    other.first_ = other.curr_ = other.last_ = nullptr;
    return *this;
}

template<typename CharT>
typename basic_iobuf<CharT>::size_type basic_iobuf<CharT>::read(uxs::span<char_type> s) {
    auto p = s.begin();
    size_type count = s.end() - p;
    if (!count) { return 0; }
    for (size_type n_avail = avail(); count > n_avail; n_avail = avail()) {
        if (n_avail) { p = std::copy_n(curr_, n_avail, p), curr_ = last_, count -= n_avail; }
        if (!this->good() || underflow() < 0) {
            this->setstate(iostate_bits::kEof | iostate_bits::kFail);
            return p - s.begin();
        }
    }
    std::copy_n(curr_, count, p), curr_ += count;
    return s.size();
}

template<typename CharT>
typename basic_iobuf<CharT>::size_type basic_iobuf<CharT>::skip(size_type count) {
    if (!count) { return 0; }
    const size_type n0 = count;
    for (size_type n_avail = avail(); count > n_avail; n_avail = avail()) {
        curr_ = last_, count -= n_avail;
        if (!this->good() || underflow() < 0) {
            this->setstate(iostate_bits::kEof | iostate_bits::kFail);
            return n0 - count;
        }
    }
    curr_ += count;
    return n0;
}

template<typename CharT>
basic_iobuf<CharT>& basic_iobuf<CharT>::write(uxs::span<const char_type> s) {
    return write(s.begin(), s.end());
}

template<typename CharT>
basic_iobuf<CharT>& basic_iobuf<CharT>::fill_n(size_type count, char_type ch) {
    if (!count) { return *this; }
    for (size_type n_avail = avail(); count > n_avail; n_avail = avail()) {
        if (n_avail) { curr_ = std::fill_n(curr_, n_avail, ch), count -= n_avail; }
        if (!this->good() || overflow() < 0) {
            this->setstate(iostate_bits::kBad);
            return *this;
        }
    }
    curr_ = std::fill_n(curr_, count, ch);
    return *this;
}

template<typename CharT>
basic_iobuf<CharT>& basic_iobuf<CharT>::flush() {
    if (!this->good() || sync() < 0) { this->setstate(iostate_bits::kBad); }
    return *this;
}

template<typename CharT>
typename basic_iobuf<CharT>::pos_type basic_iobuf<CharT>::seek(off_type off, seekdir dir) {
    this->setstate(this->rdstate() & ~iostate_bits::kEof);
    if (this->fail()) { return -1; }
    if (!!(this->mode() & iomode::kOut) && sync() < 0) {
        this->setstate(iostate_bits::kFail);
        return -1;
    }
    pos_type pos = seekimpl(off, dir);
    if (pos < 0) { this->setstate(iostate_bits::kFail); }
    return pos;
}

template<typename CharT>
int basic_iobuf<CharT>::underflow() {
    return -1;
}

template<typename CharT>
int basic_iobuf<CharT>::ungetfail() {
    return -1;
}

template<typename CharT>
int basic_iobuf<CharT>::overflow() {
    return -1;
}

template<typename CharT>
typename basic_iobuf<CharT>::pos_type basic_iobuf<CharT>::seekimpl(off_type off, seekdir dir) {
    return -1;
}

template<typename CharT>
int basic_iobuf<CharT>::sync() {
    return -1;
}

}  // namespace uxs
