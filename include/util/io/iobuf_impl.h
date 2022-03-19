#pragma once

#include "util/io/iobuf.h"

#include <algorithm>

using namespace util;

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
    static_cast<iostate&>(*this) = other;
    first_ = other.first_, curr_ = other.curr_, last_ = other.last_;
    other.first_ = other.curr_ = other.last_ = nullptr;
    return *this;
}

template<typename CharT>
typename basic_iobuf<CharT>::size_type basic_iobuf<CharT>::read(span<char_type> s) {
    if (s.empty()) { return 0; }
    auto p = s.begin();
    for (std::ptrdiff_t n_avail = last_ - curr_; s.end() - p > n_avail; n_avail = last_ - curr_) {
        if (n_avail != 0) { p = std::copy_n(curr_, n_avail, p), curr_ = last_; }
        if (!this->good() || underflow() < 0) {
            this->setstate(iostate_bits::kEof | iostate_bits::kFail);
            return p - s.begin();
        }
    }
    std::copy_n(curr_, s.end() - p, p);
    curr_ += s.end() - p;
    return s.size();
}

template<typename CharT>
basic_iobuf<CharT>& basic_iobuf<CharT>::write(span<const char_type> s) {
    if (s.empty()) { return *this; }
    auto p = s.begin();
    for (std::ptrdiff_t n_free = last_ - curr_; s.end() - p > n_free; n_free = last_ - curr_) {
        if (n_free != 0) { curr_ = std::copy_n(p, n_free, curr_), p += n_free; }
        if (!this->good() || overflow(*p++) < 0) {
            this->setstate(iostate_bits::kBad);
            return *this;
        }
    }
    curr_ = std::copy_n(p, s.end() - p, curr_);
    return *this;
}

template<typename CharT>
basic_iobuf<CharT>& basic_iobuf<CharT>::fill_n(size_type n, char_type ch) {
    if (n == 0) { return *this; }
    for (size_type n_free = last_ - curr_; n > n_free; n_free = last_ - curr_) {
        if (n_free != 0) { curr_ = std::fill_n(curr_, n_free, ch), n -= n_free + 1; }
        if (!this->good() || overflow(ch) < 0) {
            this->setstate(iostate_bits::kBad);
            return *this;
        }
    }
    curr_ = std::fill_n(curr_, n, ch);
    return *this;
}

template<typename CharT>
basic_iobuf<CharT>& basic_iobuf<CharT>::flush() {
    if (!this->good() || sync() < 0) { this->setstate(iostate_bits::kBad); }
    return *this;
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
int basic_iobuf<CharT>::overflow(char_type ch) {
    return -1;
}

template<typename CharT>
int basic_iobuf<CharT>::sync() {
    return -1;
}
