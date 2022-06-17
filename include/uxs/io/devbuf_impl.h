#pragma once

#include "uxs/io/devbuf.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>

#if defined(UXS_USE_ZLIB)
#    define ZLIB_CONST
#    include "zlib.h"
#endif  // defined(UXS_USE_ZLIB)

namespace uxs {

template<typename CharT>
struct basic_devbuf<CharT>::flexbuffer {
    size_t alloc_sz;
    size_t sz;
#if defined(UXS_USE_ZLIB)
    Bytef* z_first;
    Bytef* z_last;
    z_stream zstr;
    bool z_in_finish;
#endif  // defined(UXS_USE_ZLIB)
    bool pending_cr;
    char_type data[1];
    static size_t get_alloc_sz(size_t sz) {
        return (offsetof(flexbuffer, data[sz]) + sizeof(flexbuffer) - 1) / sizeof(flexbuffer);
    }
};

template<typename CharT>
basic_devbuf<CharT>::basic_devbuf(basic_devbuf&& other) NOEXCEPT : basic_iobuf<CharT>(std::move(other)),
                                                                   dev_(other.dev_),
                                                                   buf_(other.buf_),
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
    dev_ = other.dev_, buf_ = other.buf_, tie_buf_ = other.tie_buf_;
    return *this;
}

template<typename CharT>
void basic_devbuf<CharT>::initbuf(iomode mode, size_type bufsz) {
    assert(dev_);
    freebuf();
    if (!(mode & iomode::kIn) && !(mode & iomode::kOut)) { return; }
    if (!!(mode & iomode::kOut)) { mode &= ~iomode::kIn; }
    bufsz = std::min<size_type>(std::max<size_type>(bufsz, kMinBufSize), kMaxBufSize);
    this->setmode(mode);
    size_t alloc_sz = flexbuffer::get_alloc_sz(bufsz);
    buf_ = std::allocator<flexbuffer>().allocate(alloc_sz);
    std::memset(buf_, 0, sizeof(flexbuffer));
    buf_->alloc_sz = alloc_sz;
    buf_->sz = (alloc_sz * sizeof(flexbuffer) - offsetof(flexbuffer, data[0])) / sizeof(char_type);
    assert(buf_->sz >= bufsz);
#if defined(UXS_USE_ZLIB)
    if (!!(mode & iomode::kZCompr)) {
        size_t tot_sz = buf_->sz;
        buf_->sz /= 2;
        buf_->z_first = reinterpret_cast<Bytef*>(&buf_->data[buf_->sz]);
        buf_->z_last = reinterpret_cast<Bytef*>(&buf_->data[tot_sz]);
        if (!!(mode & iomode::kOut)) {
            deflateInit(&buf_->zstr, Z_DEFAULT_COMPRESSION);
            buf_->zstr.next_out = buf_->z_first;
            buf_->zstr.avail_out = static_cast<uLong>(buf_->z_last - buf_->z_first);
        } else {
            inflateInit(&buf_->zstr);
        }
    }
#endif  // defined(UXS_USE_ZLIB)
    buf_->pending_cr = false;
    if (!!(mode & iomode::kOut)) {
        // Reserve additional space for Lf->CrLf expansion
        size_t cr_reserve_sz = !!(mode & iomode::kCrLf) ? buf_->sz / kCrReserveRatio : 0;
        this->setview(&buf_->data[cr_reserve_sz], &buf_->data[cr_reserve_sz], &buf_->data[buf_->sz]);
    } else {
        this->setview(buf_->data, buf_->data, buf_->data);
    }
    int64_t abs_off = dev_->seek(0, seekdir::kCurr);
    if (abs_off >= 0) { pos_ = abs_off / sizeof(char_type); }
    this->clear();
}

template<typename CharT>
void basic_devbuf<CharT>::freebuf() {
    if (!buf_) { return; }
    if (!!(this->mode() & iomode::kOut)) { this->flush(); }
#if defined(UXS_USE_ZLIB)
    if (!!(this->mode() & iomode::kZCompr)) {
        if (!!(this->mode() & iomode::kOut)) {
            finish_compressed();
            deflateEnd(&buf_->zstr);
        } else {
            inflateEnd(&buf_->zstr);
        }
    }
#endif  // defined(UXS_USE_ZLIB)
    std::allocator<flexbuffer>().deallocate(buf_, buf_->alloc_sz);
    buf_ = nullptr;
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
static int write_all(iodevice* dev, const void* data, size_t sz) {
    int ret = 0;
    sz *= sizeof(CharT);
    do {
        size_t chunk_sz = 0;
        if ((ret = dev->write(data, sz, chunk_sz)) < 0) { return ret; }
        if (sz && !chunk_sz) { return -1; }
        data = reinterpret_cast<const uint8_t*>(data) + chunk_sz, sz -= chunk_sz;
    } while (sz);
    return ret;
}

template<typename CharT>
static int read_at_least_one(iodevice* dev, void* data, size_t sz, size_t& n_read) {
    int ret = 0;
    sz *= sizeof(CharT);
    n_read = 0;
    do {
        size_t chunk_sz = 0;
        if ((ret = dev->read(data, sz, chunk_sz)) < 0) { return ret; }
        if (!chunk_sz) { break; }
        data = reinterpret_cast<uint8_t*>(data) + chunk_sz, n_read += chunk_sz;
        sz = n_read & (sizeof(CharT) - 1);
    } while (sz);
    n_read /= sizeof(CharT);
    return n_read ? ret : -1;
}

template<typename CharT>
int basic_devbuf<CharT>::write_buf(const void* data, size_t sz) {
    int ret = !!(this->mode() & iomode::kZCompr) ? write_compressed(data, sz * sizeof(char_type)) :
                                                   write_all<char_type>(dev_, data, sz);
    if (ret < 0) { return ret; }
    pos_ += sz;
    return ret;
}

template<typename CharT>
int basic_devbuf<CharT>::read_buf(void* data, size_t sz, size_t& n_read) {
    int ret = 0;
    if (!!(this->mode() & iomode::kZCompr)) {
        ret = read_compressed(data, sz * sizeof(char_type), n_read);
        n_read /= sizeof(char_type);
    } else {
        ret = read_at_least_one<char_type>(dev_, data, sz, n_read);
    }
    if (ret < 0) { return ret; }
    pos_ += n_read;
    return ret;
}

template<typename CharT>
int basic_devbuf<CharT>::write_compressed(const void* data, size_t sz) {
    assert(buf_);
#if defined(UXS_USE_ZLIB)
    buf_->zstr.next_in = reinterpret_cast<const Bytef*>(data);
    buf_->zstr.avail_in = static_cast<uLong>(sz);
    do {
        if (deflate(&buf_->zstr, Z_NO_FLUSH) != Z_OK) { return -1; }
        if (!buf_->zstr.avail_out) {
            int ret = write_all<Bytef>(dev_, buf_->z_first, buf_->zstr.next_out - buf_->z_first);
            if (ret < 0) { return ret; }
            buf_->zstr.next_out = buf_->z_first;
            buf_->zstr.avail_out = static_cast<uLong>(buf_->z_last - buf_->z_first);
        }
    } while (buf_->zstr.avail_in);
    return 0;
#else   // defined(UXS_USE_ZLIB)
    return -1;
#endif  // defined(UXS_USE_ZLIB)
}

template<typename CharT>
void basic_devbuf<CharT>::finish_compressed() {
    assert(buf_);
#if defined(UXS_USE_ZLIB)
    while (true) {
        int z_ret = deflate(&buf_->zstr, Z_FINISH);
        if (z_ret != Z_OK && z_ret != Z_STREAM_END) { return; }
        int ret = write_all<Bytef>(dev_, buf_->z_first, buf_->zstr.next_out - buf_->z_first);
        if (ret < 0 || z_ret == Z_STREAM_END) { return; }
        buf_->zstr.next_out = buf_->z_first;
        buf_->zstr.avail_out = static_cast<uLong>(buf_->z_last - buf_->z_first);
    }
#endif  // defined(UXS_USE_ZLIB)
}

template<typename CharT>
int basic_devbuf<CharT>::read_compressed(void* data, size_t sz, size_t& n_read) {
    assert(buf_);
#if defined(UXS_USE_ZLIB)
    buf_->zstr.next_out = reinterpret_cast<Bytef*>(data);
    buf_->zstr.avail_out = static_cast<uLong>(sz);
    do {
        if (!buf_->zstr.avail_in) {
            size_t n_raw_read = 0;
            int ret = read_at_least_one<Bytef>(dev_, buf_->z_first, buf_->z_last - buf_->z_first, n_raw_read);
            buf_->z_in_finish = ret < 0;
            if (!buf_->z_in_finish) {
                buf_->zstr.next_in = buf_->z_first;
                buf_->zstr.avail_in = static_cast<uLong>(n_raw_read);
            }
        }
        int z_ret = inflate(&buf_->zstr, buf_->z_in_finish ? Z_FINISH : Z_NO_FLUSH);
        if (z_ret == Z_STREAM_END) { break; }
        if (z_ret != Z_OK) { return -1; }
    } while (buf_->zstr.avail_out);
    n_read = buf_->zstr.next_out - reinterpret_cast<const Bytef*>(data);
    return n_read ? 0 : -1;
#else   // defined(UXS_USE_ZLIB)
    return -1;
#endif  // defined(UXS_USE_ZLIB)
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
        if ((ret = write_buf(from, mid - from)) < 0) { return ret; }
        this->setcurr(this->first());
        return ret;
    }
    do {
        char_type *to0 = buf_->data, *to = to0;
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
                    if ((ret = write_buf(to0, to - to0)) < 0) { return ret; }
                    parse_ctrlesc(from + 1, end_of_esc);
                    to = to0;
                }
                from = end_of_esc;
                if (from == top) { break; }
            }
            *to++ = *from++;
        }
        if ((ret = write_buf(to0, to - to0)) < 0) { return ret; }
    } while (from != top);
    this->setcurr(std::copy(mid, top, this->first()));
    return ret;
}

template<typename CharT>
size_t basic_devbuf<CharT>::remove_crlf(char_type* dst, size_t count) {
    if (!count) { return 0; }
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
    return count - static_cast<size_t>(dst_end - p);
}

template<typename CharT>
int basic_devbuf<CharT>::underflow() {
    assert(dev_ && buf_);
    if (!(this->mode() & iomode::kIn)) { return -1; }
    if (tie_buf_) { tie_buf_->flush(); }
    int ret = 0;
    size_t n_read = 0;
    if (!!(this->mode() & iomode::kCrLf)) {
        size_t sz = buf_->sz;
        char_type* p = this->first();
        if (buf_->pending_cr) { *p++ = '\r', --sz; }
        if ((ret = read_buf(p, sz, n_read)) < 0) { return ret; }
        n_read = remove_crlf(this->first(), (p + n_read) - this->first());
        buf_->pending_cr = n_read && this->first()[n_read - 1] == '\r';
        if (buf_->pending_cr) { --n_read; }
    } else if ((ret = read_buf(this->first(), buf_->sz, n_read)) < 0) {
        return ret;
    }
    assert(n_read);
    this->setview(this->first(), this->first(), this->first() + n_read);
    return ret;
}

template<typename CharT>
int basic_devbuf<CharT>::overflow() {
    assert(dev_ && buf_);
    if (!(this->mode() & iomode::kOut)) { return -1; }
    if (tie_buf_) { tie_buf_->flush(); }
    return flush_buffer();
}

template<typename CharT>
typename basic_devbuf<CharT>::pos_type basic_devbuf<CharT>::seekimpl(off_type off, seekdir dir) {
    assert(dev_ && buf_);
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
    if (!!(this->mode() & (iomode::kAppend | iomode::kZCompr))) { return pos_; }
    int64_t abs_off = dev_->seek(off * sizeof(char_type), dir);
    if (abs_off < 0) { return -1; }
    pos_ = abs_off / sizeof(char_type);
    if (!!(this->mode() & iomode::kIn)) { this->setview(this->first(), this->first(), this->first()); }
    return pos_;
}

template<typename CharT>
int basic_devbuf<CharT>::sync() {
    assert(dev_ && buf_);
    if (!(this->mode() & iomode::kOut)) { return -1; }
    if (tie_buf_) { tie_buf_->flush(); }
    int ret = flush_buffer();
    if (ret < 0) { return ret; }
    return dev_->flush();
}

}  // namespace uxs
