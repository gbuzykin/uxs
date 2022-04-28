#pragma once

#include "util/io/devbuf.h"

#include <algorithm>
#include <array>
#include <memory>

namespace util {

template<typename CharT>
basic_devbuf<CharT>::basic_devbuf(basic_devbuf&& other) NOEXCEPT : basic_iobuf<CharT>(std::move(other)),
                                                                   dev_(other.dev_),
                                                                   bufsz_(other.bufsz_),
                                                                   tie_buf_(other.tie_buf_) {}

template<typename CharT>
basic_devbuf<CharT>::~basic_devbuf() {
    freebuf();
}

template<typename CharT>
basic_devbuf<CharT>& basic_devbuf<CharT>::operator=(basic_devbuf&& other) {
    if (&other == this) { return *this; }
    freebuf();
    static_cast<basic_iobuf<CharT>&>(*this) = std::move(other);
    dev_ = other.dev_, bufsz_ = other.bufsz_, tie_buf_ = other.tie_buf_;
    return *this;
}

template<typename CharT>
void basic_devbuf<CharT>::initbuf(iomode mode, size_type bufsz) {
    assert(dev_);
    freebuf();
    if (!(mode & iomode::kIn) && !(mode & iomode::kOut)) { return; }
    if (!!(mode & iomode::kOut)) { mode &= ~iomode::kIn; }
    this->setmode(mode);
    bufsz = std::max<size_type>(bufsz, kMinBufSize) / sizeof(char_type);
    char_type* buf = std::allocator<char_type>().allocate(bufsz);
    bufsz_ = bufsz;
    if (!!(mode & iomode::kOut)) {
        size_type cr_reserve_sz = !!(mode & iomode::kCrLf) ? bufsz / kCrReserveRatio : 0;
        this->setview(buf + cr_reserve_sz, buf + cr_reserve_sz, buf + bufsz);
    } else {
        *buf = '\0';
        this->setview(buf, buf, buf);
    }
    int64_t abs_off = dev_->seek(0, seekdir::kCurr);
    if (abs_off >= 0) { pos_ = abs_off / sizeof(char_type); }
    this->clear();
}

template<typename CharT>
void basic_devbuf<CharT>::freebuf() {
    if (this->first() == nullptr) { return; }
    if (!!(this->mode() & iomode::kOut)) { this->flush(); }
    std::allocator<char_type>().deallocate(!!(this->mode() & iomode::kOut) ?  //
                                               this->last() - bufsz_ :
                                               this->first(),
                                           bufsz_);
    this->setview(nullptr, nullptr, nullptr);
    this->setmode(iomode::kNone);
    this->setstate(iostate_bits::kFail);
}

template<typename CharT>
const typename basic_devbuf<CharT>::char_type* basic_devbuf<CharT>::find_end_of_ctrlesc(const char_type* first,
                                                                                        const char_type* last) {
    if (first == last) { return first; }
    const char_type* p = first;
    if (*p == '[') {
        while (++p != last) {
            if (*p >= '@' && *p <= '~') { return p + 1; }
        }
        return first;
    }
    return p + 1;
}

template<typename CharT>
int basic_devbuf<CharT>::write_all(const void* data, size_type sz) {
    int ret = 0;
    size_type n_written = sz;
    sz *= sizeof(char_type);
    do {
        size_type chunk_sz = 0;
        if ((ret = dev_->write(data, sz, chunk_sz)) < 0) { return ret; }
        data = reinterpret_cast<const uint8_t*>(data) + chunk_sz, sz -= chunk_sz;
    } while (sz != 0);
    pos_ += n_written;
    return ret;
}

template<typename CharT>
int basic_devbuf<CharT>::read_at_least_one(void* data, size_type sz, size_type& n_read) {
    int ret = 0;
    sz *= sizeof(char_type);
    n_read = 0;
    do {
        size_type chunk_sz = 0;
        if ((ret = dev_->read(data, sz, chunk_sz)) < 0) { return ret; }
        if (chunk_sz == 0) { break; }
        data = reinterpret_cast<uint8_t*>(data) + chunk_sz, n_read += chunk_sz;
        sz = n_read & (sizeof(char_type) - 1);
    } while (sz != 0);
    n_read /= sizeof(char_type);
    pos_ += n_read;
    return n_read != 0 ? ret : -1;
}

template<typename CharT>
void basic_devbuf<CharT>::parse_ctrlesc(const char_type* first, const char_type* last) {
    if (first == last || *first != '[' || *(last - 1) != 'm') { return; }  // not a color ANSI code
    std::array<uint8_t, 16> v;
    unsigned n = 0;
    v[0] = 0;
    while (++first != last) {
        if (*first == ';') {
            if (++n >= v.size()) { break; }
            v[n] = 0;
        } else if (*first >= '0' && *first <= '9') {
            v[n] = static_cast<uint8_t>(10 * v[n] + static_cast<int>(*first - '0'));
        } else {
            break;
        }
    }
    dev_->ctrlesc_color(as_span(v.data(), n + 1));
}

template<typename CharT>
int basic_devbuf<CharT>::flush_buffer() {
    int ret = 0;
    const char_type *from = this->first(), *top = this->curr(), *mid = top;
    if (!(this->mode() & (iomode::kCrLf | iomode::kCtrlEsc))) {
        if ((ret = write_all(from, mid - from)) < 0) { return ret; }
        this->setcurr(this->first());
        return ret;
    }
    do {
        char_type *to0 = this->last() - bufsz_, *to = to0;
        while (from != top) {
            if (*from == '\n' && !!(this->mode() & iomode::kCrLf)) {
                if (to == from) { break; }
                *to++ = '\r';
            } else if (*from == '\033' && !!(this->mode() & iomode::kCtrlEsc)) {
                const char_type* end_of_esc = find_end_of_ctrlesc(from + 1, top);
                if (end_of_esc == from + 1) {
                    mid = from;
                    from = top;
                    break;
                }
                if ((this->mode() & iomode::kSkipCtrlEsc) != iomode::kSkipCtrlEsc) {
                    if ((ret = write_all(to0, to - to0)) < 0) { return ret; }
                    parse_ctrlesc(from + 1, end_of_esc);
                    to = to0;
                }
                from = end_of_esc;
                if (from == top) { break; }
            }
            *to++ = *from++;
        }
        if ((ret = write_all(to0, to - to0)) < 0) { return ret; }
    } while (from != top);
    this->setcurr(std::copy(mid, top, this->first()));
    return ret;
}

template<typename CharT>
typename basic_devbuf<CharT>::size_type basic_devbuf<CharT>::remove_crlf(char_type* dst, size_type count) {
    if (count == 0) { return 0; }
    char_type* dst_end = dst + count;
    do {
        if ((dst = std::find(dst + 1, dst_end, '\n')) == dst_end) { return count; }
    } while (*(dst - 1) != '\r');
    char_type* p = dst - 1;
    *p = *dst;
    for (; dst != dst_end; ++dst) {
        if (*(dst - 1) != '\r' || *dst != '\n') { ++p; }
        *(p - 1) = *dst;
    }
    return count - static_cast<size_type>(dst_end - p);
}

template<typename CharT>
int basic_devbuf<CharT>::underflow() {
    assert(dev_ && this->first() && this->curr() && this->last());
    if (!(this->mode() & iomode::kIn)) { return -1; }
    if (tie_buf_) { tie_buf_->flush(); }
    int ret = 0;
    size_type n_read = 0;
    if (!!(this->mode() & iomode::kCrLf)) {
        size_type sz = bufsz_ - 1;
        char_type* p = this->first();
        if (*this->last() == '\r') { *p++ = '\r', --sz; }
        if ((ret = read_at_least_one(p, sz, n_read)) < 0) { return ret; }
        n_read = remove_crlf(this->first(), (p + n_read) - this->first());
        if (n_read > 1 && this->first()[n_read - 1] == '\r') {
            --n_read;
        } else {
            this->first()[n_read] = '\0';
        }
    } else if ((ret = read_at_least_one(this->first(), bufsz_, n_read)) < 0) {
        return ret;
    }
    this->setview(this->first(), this->first(), this->first() + n_read);
    return ret;
}

template<typename CharT>
int basic_devbuf<CharT>::overflow(char_type ch) {
    assert(dev_ && this->first() && this->curr() && this->last());
    if (!(this->mode() & iomode::kOut)) { return -1; }
    if (tie_buf_) { tie_buf_->flush(); }
    int ret = flush_buffer();
    if (ret < 0) { return ret; }
    *this->curr() = ch;
    this->bump(1);
    return ret;
}

template<typename CharT>
typename basic_devbuf<CharT>::pos_type basic_devbuf<CharT>::seekimpl(off_type off, seekdir dir) {
    assert(dev_ && this->first() && this->curr() && this->last());
    if (dir != seekdir::kEnd) {
        std::ptrdiff_t delta = !!(this->mode() & iomode::kOut) ? this->curr() - this->first() :
                                                                 this->curr() - this->last();
        pos_type pos = pos_ + delta;
        if (dir == seekdir::kCurr) {
            if (off == 0) { return pos; }
            off += delta;
        } else if (pos == off) {
            return pos;
        }
    }
    int64_t abs_off = dev_->seek(off * sizeof(char_type), dir);
    if (abs_off < 0) { return -1; }
    pos_ = abs_off / sizeof(char_type);
    if (!!(this->mode() & iomode::kIn)) { this->setview(this->first(), this->first(), this->first()); }
    return pos_;
}

template<typename CharT>
int basic_devbuf<CharT>::sync() {
    assert(dev_ && this->first() && this->curr() && this->last());
    if (!(this->mode() & iomode::kOut)) { return -1; }
    if (tie_buf_) { tie_buf_->flush(); }
    int ret = flush_buffer();
    if (ret < 0) { return ret; }
    return dev_->flush();
}

}  // namespace util
