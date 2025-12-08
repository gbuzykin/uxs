#pragma once

#include "uxs/io/devbuf.h"

#include <array>
#include <cstring>

#if UXS_USE_ZLIB != 0
#    define ZLIB_CONST
#    include <zlib.h>
#endif  // UXS_USE_ZLIB != 0

namespace uxs {

namespace detail {
enum class devbuf_impl_flags { none = 0, z_in_finish = 1, pending_cr = 2 };
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(devbuf_impl_flags);
}  // namespace detail

template<typename CharT, typename Alloc>
struct basic_devbuf<CharT, Alloc>::flexbuf_t {
    detail::devbuf_impl_flags flags;
    std::size_t alloc_sz;
    std::size_t sz;

#if UXS_USE_ZLIB != 0
    Bytef* z_buf;
    uLong z_buf_sz;
    z_stream zstr;
#endif  // UXS_USE_ZLIB != 0

    char_type data[1];
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<flexbuf_t>;
    static std::size_t get_alloc_sz(std::size_t sz) {
        return (offsetof(flexbuf_t, data) + sz * sizeof(char_type) + sizeof(flexbuf_t) - 1) / sizeof(flexbuf_t);
    }
    static flexbuf_t* alloc(alloc_type al, std::size_t sz) {
        const std::size_t alloc_sz = get_alloc_sz(sz);
        flexbuf_t* buf = al.allocate(alloc_sz);
        std::memset(buf, 0, sizeof(flexbuf_t));
        buf->alloc_sz = alloc_sz;
        buf->sz = (alloc_sz * sizeof(flexbuf_t) - offsetof(flexbuf_t, data)) / sizeof(char_type);
        assert(buf->sz >= sz && get_alloc_sz(buf->sz) == alloc_sz);
        return buf;
    }
};

template<typename CharT, typename Alloc>
basic_devbuf<CharT, Alloc>::~basic_devbuf() {
    freebuf();
}

template<typename CharT, typename Alloc>
basic_devbuf<CharT, Alloc>::basic_devbuf(basic_devbuf&& other) noexcept
    : alloc_type(std::move(other)), basic_iobuf<CharT>(std::move(other)), dev_(other.dev_), buf_(other.buf_),
      tie_buf_(other.tie_buf_) {
    other.dev_ = nullptr, other.buf_ = nullptr, other.tie_buf_ = nullptr;
}

template<typename CharT, typename Alloc>
basic_devbuf<CharT, Alloc>& basic_devbuf<CharT, Alloc>::operator=(basic_devbuf&& other) noexcept {
    if (&other == this) { return *this; }
    freebuf();
    static_cast<alloc_type&>(*this) = std::move(other);
    basic_iobuf<CharT>::operator=(std::move(other));
    dev_ = other.dev_, buf_ = other.buf_, tie_buf_ = other.tie_buf_;
    other.dev_ = nullptr, other.buf_ = nullptr, other.tie_buf_ = nullptr;
    return *this;
}

template<typename CharT, typename Alloc>
void basic_devbuf<CharT, Alloc>::initbuf(iomode mode, size_type bufsz) {
    assert(dev_);
    freebuf();
    if (!(mode & iomode::in) && !(mode & iomode::out)) { return; }
    bufsz = std::min<size_type>(std::max<size_type>(bufsz, min_buf_size), max_buf_size);
    const bool mappable = !!(dev_->caps() & iodevcaps::mappable);
    if (!!(mode & iomode::out)) {
        mode &= ~iomode::in;
        if (!mappable || !!(mode & (iomode::cr_lf | iomode::ctrl_esc | iomode::z_compr))) {
            buf_ = flexbuf_t::alloc(*this, bufsz);

#if UXS_USE_ZLIB != 0
            if (!!(mode & iomode::z_compr)) {
                const int level = static_cast<int>(mode & iomode::z_compr_level_mask) /
                                  static_cast<int>(iomode::z_compr_level);
                ::deflateInit(&buf_->zstr, level > 0 ? level : Z_DEFAULT_COMPRESSION);
                if (!mappable) {
                    const std::size_t tot_sz = buf_->sz;
                    buf_->sz /= 2;
                    buf_->z_buf = reinterpret_cast<Bytef*>(buf_->data + buf_->sz);
                    buf_->z_buf_sz = static_cast<uLong>(tot_sz - buf_->sz);
                    buf_->zstr.next_out = buf_->z_buf;
                    buf_->zstr.avail_out = buf_->z_buf_sz;
                }
            }
#endif  // UXS_USE_ZLIB != 0

            // reserve additional space for Lf->CrLf expansion
            const std::size_t cr_reserve_sz = !!(mode & iomode::cr_lf) ? buf_->sz / cr_reserve_ratio : 0;
            this->reset(buf_->data + cr_reserve_sz, 0, buf_->sz - cr_reserve_sz);
        }
    } else if (!mappable || !!(mode & (iomode::cr_lf | iomode::z_compr))) {
        buf_ = flexbuf_t::alloc(*this, bufsz);

#if UXS_USE_ZLIB != 0
        if (!!(mode & iomode::z_compr)) {
            ::inflateInit(&buf_->zstr);
            if (!mappable) {
                const std::size_t tot_sz = buf_->sz;
                buf_->sz /= 2;
                buf_->z_buf = reinterpret_cast<Bytef*>(buf_->data + buf_->sz);
                buf_->z_buf_sz = static_cast<uLong>(tot_sz - buf_->sz);
            }
        }
#endif  // UXS_USE_ZLIB != 0
    }
    this->setmode(mode);
    this->clear();
}

template<typename CharT, typename Alloc>
void basic_devbuf<CharT, Alloc>::freebuf() noexcept {
    if (this->mode() == iomode::none) { return; }
    if (!!(this->mode() & iomode::out)) {
        this->flush();

#if UXS_USE_ZLIB != 0
        if (!!(this->mode() & iomode::z_compr)) {
            finish_compressed();
            ::deflateEnd(&buf_->zstr);
        }
#endif  // UXS_USE_ZLIB != 0
    } else if (!!(this->mode() & iomode::z_compr)) {
#if UXS_USE_ZLIB != 0
        ::inflateEnd(&buf_->zstr);
#endif  // UXS_USE_ZLIB != 0
    }
    if (buf_) {
        typename flexbuf_t::alloc_type(*this).deallocate(buf_, buf_->alloc_sz);
        buf_ = nullptr;
    }
    this->reset(nullptr, 0, 0);
    this->setmode(iomode::none);
    this->setstate(iostate_bits::fail);
}

template<typename CharT, typename Alloc>
const typename basic_devbuf<CharT, Alloc>::char_type* basic_devbuf<CharT, Alloc>::find_end_of_ctrlesc(
    const char_type* first, const char_type* last) noexcept {
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

namespace detail {
template<typename CharT>
int write_all(iodevice* dev, const void* data, std::size_t sz) {
    sz *= sizeof(CharT);
    do {
        std::size_t chunk_sz = 0;
        const int ret = dev->write(data, sz, chunk_sz);
        if (ret < 0) { return ret; }
        if (sz && !chunk_sz) { return -1; }
        data = static_cast<const std::uint8_t*>(data) + chunk_sz, sz -= chunk_sz;
    } while (sz);
    return 0;
}
template<typename CharT>
int read_at_least_one(iodevice* dev, void* data, std::size_t sz, std::size_t& n_read) {
    sz *= sizeof(CharT);
    n_read = 0;
    do {
        std::size_t chunk_sz = 0;
        const int ret = dev->read(data, sz, chunk_sz);
        if (ret < 0) { return ret; }
        if (!chunk_sz) { break; }
        data = static_cast<std::uint8_t*>(data) + chunk_sz, n_read += chunk_sz;
        sz = n_read & (sizeof(CharT) - 1);
    } while (sz);
    n_read /= sizeof(CharT);
    return n_read ? 0 : -1;
}
}  // namespace detail

template<typename CharT, typename Alloc>
int basic_devbuf<CharT, Alloc>::write_buf(const void* data, std::size_t sz) {
    assert(buf_);
    return !!(this->mode() & iomode::z_compr) ? write_compressed(data, sz * sizeof(char_type)) :
                                                detail::write_all<char_type>(dev_, data, sz);
}

template<typename CharT, typename Alloc>
int basic_devbuf<CharT, Alloc>::read_buf(void* data, std::size_t sz, std::size_t& n_read) {
    assert(buf_);
    int ret = 0;
    if (!!(this->mode() & iomode::z_compr)) {
        ret = read_compressed(data, sz * sizeof(char_type), n_read);
        n_read /= sizeof(char_type);
    } else {
        ret = detail::read_at_least_one<char_type>(dev_, data, sz, n_read);
    }
    return ret;
}

#if UXS_USE_ZLIB != 0

template<typename CharT, typename Alloc>
int basic_devbuf<CharT, Alloc>::flush_compressed_buf() {
    if (!(dev_->caps() & iodevcaps::mappable)) {
        const int ret = detail::write_all<Bytef>(dev_, buf_->z_buf, buf_->zstr.next_out - buf_->z_buf);
        if (ret < 0) { return ret; }
        buf_->zstr.next_out = buf_->z_buf;
        buf_->zstr.avail_out = buf_->z_buf_sz;
        return 0;
    }
    std::size_t sz = 0;
    dev_->advance(buf_->zstr.next_out - buf_->z_buf);
    buf_->zstr.next_out = buf_->z_buf = static_cast<Bytef*>(dev_->map(sz, true));
    buf_->zstr.avail_out = static_cast<uLong>(sz);
    return sz ? 0 : -1;
}

template<typename CharT, typename Alloc>
int basic_devbuf<CharT, Alloc>::write_compressed(const void* data, std::size_t sz) {
    buf_->zstr.next_in = static_cast<const Bytef*>(data);
    buf_->zstr.avail_in = static_cast<uLong>(sz);
    do {
        int ret = 0;
        if (!buf_->zstr.avail_out && (ret = flush_compressed_buf()) < 0) { return ret; }
        if (::deflate(&buf_->zstr, Z_NO_FLUSH) != Z_OK) { return -1; }
    } while (buf_->zstr.avail_in);
    return 0;
}

template<typename CharT, typename Alloc>
void basic_devbuf<CharT, Alloc>::finish_compressed() {
    int z_ret = 0;
    do {
        if (!buf_->zstr.avail_out && flush_compressed_buf() < 0) { return; }
        z_ret = ::deflate(&buf_->zstr, Z_FINISH);
    } while (z_ret == Z_OK);
    if (z_ret != Z_STREAM_END) { return; }
    if (!(dev_->caps() & iodevcaps::mappable)) {
        detail::write_all<Bytef>(dev_, buf_->z_buf, buf_->zstr.next_out - buf_->z_buf);
    } else {
        dev_->advance(buf_->zstr.next_out - buf_->z_buf);
    }
}

template<typename CharT, typename Alloc>
int basic_devbuf<CharT, Alloc>::read_compressed(void* data, std::size_t sz, std::size_t& n_read) {
    buf_->zstr.next_out = static_cast<Bytef*>(data);
    buf_->zstr.avail_out = static_cast<uLong>(sz);
    do {
        if (!(buf_->flags & detail::devbuf_impl_flags::z_in_finish) && !buf_->zstr.avail_in) {
            if (!(dev_->caps() & iodevcaps::mappable)) {
                std::size_t n_raw_read = 0;
                detail::read_at_least_one<Bytef>(dev_, buf_->z_buf, buf_->z_buf_sz, n_raw_read);
                buf_->zstr.next_in = buf_->z_buf;
                buf_->zstr.avail_in = static_cast<uLong>(n_raw_read);
            } else {
                dev_->advance(buf_->zstr.next_in - buf_->z_buf);
                std::size_t sz = 0;
                buf_->zstr.next_in = buf_->z_buf = static_cast<Bytef*>(dev_->map(sz, false));
                buf_->zstr.avail_in = static_cast<uLong>(sz);
            }
            if (!buf_->zstr.avail_in) { buf_->flags |= detail::devbuf_impl_flags::z_in_finish; }
        }
        const int z_ret = ::inflate(&buf_->zstr,
                                    !!(buf_->flags & detail::devbuf_impl_flags::z_in_finish) ? Z_FINISH : Z_NO_FLUSH);
        if (z_ret == Z_STREAM_END) { break; }
        if (z_ret != Z_OK) { return -1; }
    } while (buf_->zstr.avail_out);
    n_read = buf_->zstr.next_out - static_cast<const Bytef*>(data);
    return n_read ? 0 : -1;
}

#else  // UXS_USE_ZLIB != 0

template<typename CharT, typename Alloc>
int basic_devbuf<CharT, Alloc>::flush_compressed_buf() {
    return -1;
}

template<typename CharT, typename Alloc>
int basic_devbuf<CharT, Alloc>::write_compressed(const void* /*data*/, std::size_t /*sz*/) {
    return -1;
}

template<typename CharT, typename Alloc>
void basic_devbuf<CharT, Alloc>::finish_compressed() {}

template<typename CharT, typename Alloc>
int basic_devbuf<CharT, Alloc>::read_compressed(void* /*data*/, std::size_t /*sz*/, std::size_t& /*n_read*/) {
    return -1;
}

#endif  // UXS_USE_ZLIB != 0

template<typename CharT, typename Alloc>
void basic_devbuf<CharT, Alloc>::parse_ctrlesc(const char_type* first, const char_type* last) {
    if (first == last || *first != '[' || *(last - 1) != 'm') { return; }  // not a color ANSI code
    std::array<std::uint8_t, 16> v;
    unsigned n = 0;
    v[0] = 0;
    while (++first != last) {
        if (*first == ';') {
            if (++n >= v.size()) { break; }
            v[n] = 0;
        } else if (*first >= '0' && *first <= '9') {
            v[n] = static_cast<std::uint8_t>(10 * v[n] + static_cast<int>(*first - '0'));
        } else {
            break;
        }
    }
    dev_->ctrlesc_color(est::as_span(v.data(), n + 1));
}

template<typename CharT, typename Alloc>
int basic_devbuf<CharT, Alloc>::flush_buffer() {
    int ret = 0;
    const char_type* from = this->first();
    if (!(this->mode() & (iomode::cr_lf | iomode::ctrl_esc))) {
        if ((ret = write_buf(from, this->pos())) < 0) { return ret; }
        this->setpos(0);
        return 0;
    }
    do {
        char_type* to0 = buf_->data;
        char_type* to = to0;
        while (from != this->curr()) {
            if (*from == '\n' && !!(this->mode() & iomode::cr_lf)) {
                if (to == from) { break; }
                *to++ = '\r';
            } else if (*from == '\033' && !!(this->mode() & iomode::ctrl_esc)) {
                const char_type* end_of_esc = find_end_of_ctrlesc(from + 1, this->curr());
                if (end_of_esc == from + 1) {                  // escape sequence is unfinished
                    if (from == this->first()) { return -1; }  // too long escape sequence
                    if ((ret = write_buf(to0, to - to0)) < 0) { return ret; }
                    const std::size_t pos = this->curr() - from;
                    std::copy_n(from, pos, this->first());  // move it to the beginning
                    this->setpos(pos);
                    return 0;
                }
                if ((this->mode() & iomode::skip_ctrl_esc) != iomode::skip_ctrl_esc) {
                    if ((ret = write_buf(to0, to - to0)) < 0) { return ret; }
                    parse_ctrlesc(from + 1, end_of_esc);
                    to = to0;
                }
                from = end_of_esc;
                continue;
            }
            *to++ = *from++;
        }
        if ((ret = write_buf(to0, to - to0)) < 0) { return ret; }
    } while (from != this->curr());
    this->setpos(0);
    return 0;
}

template<typename CharT, typename Alloc>
std::size_t basic_devbuf<CharT, Alloc>::remove_crlf(char_type* dst, std::size_t count) noexcept {
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
    return count - static_cast<std::size_t>(dst_end - p);
}

template<typename CharT, typename Alloc>
int basic_devbuf<CharT, Alloc>::underflow() {
    assert(dev_);
    if (!(this->mode() & iomode::in)) { return -1; }
    if (tie_buf_) { tie_buf_->flush(); }
    if (!buf_) {  // no buffer - can use mapping
        std::size_t sz = 0;
        char_type* p = static_cast<char_type*>(dev_->map(sz, false));
        sz /= sizeof(char_type);
        dev_->advance(sz * sizeof(char_type));
        this->reset(p, 0, sz);
        return sz ? 0 : -1;
    }
    int ret = 0;
    std::size_t n_read = 0;
    if (!!(this->mode() & iomode::cr_lf)) {
        std::size_t sz = buf_->sz;
        char_type* p = buf_->data;
        if (!!(buf_->flags & detail::devbuf_impl_flags::pending_cr)) {
            *p++ = '\r', --sz, buf_->flags &= ~detail::devbuf_impl_flags::pending_cr;
        }
        if ((ret = read_buf(p, sz, n_read)) < 0) { return ret; }
        n_read = remove_crlf(buf_->data, static_cast<std::size_t>(p - buf_->data) + n_read);
        if (n_read && buf_->data[n_read - 1] == '\r') {
            --n_read, buf_->flags |= detail::devbuf_impl_flags::pending_cr;
        }
    } else if ((ret = read_buf(buf_->data, buf_->sz, n_read)) < 0) {
        return ret;
    }
    assert(n_read);
    this->reset(buf_->data, 0, n_read);
    return 0;
}

template<typename CharT, typename Alloc>
int basic_devbuf<CharT, Alloc>::overflow() {
    assert(dev_);
    if (!(this->mode() & iomode::out)) { return -1; }
    if (tie_buf_) { tie_buf_->flush(); }
    if (!buf_) {  // no buffer - can use mapping
        dev_->advance(this->pos() * sizeof(char_type));
        std::size_t sz = 0;
        char_type* p = static_cast<char_type*>(dev_->map(sz, true));
        this->reset(p, 0, sz / sizeof(char_type));
        return sz ? 0 : -1;
    }
    return flush_buffer();
}

template<typename CharT, typename Alloc>
int basic_devbuf<CharT, Alloc>::sync() {
    assert(dev_);
    if (!(this->mode() & iomode::out)) { return -1; }
    if (tie_buf_) { tie_buf_->flush(); }
    if (!buf_) {  // no buffer - can use mapping
        dev_->advance(this->pos() * sizeof(char_type));
        this->reset(this->curr(), 0, this->avail());
    } else {
        const int ret = flush_buffer();
        if (ret < 0) { return ret; }
    }
    return dev_->flush();
}

template<typename CharT, typename Alloc>
int basic_devbuf<CharT, Alloc>::truncate_impl() {
    assert(dev_);
    return dev_->truncate();
}

template<typename CharT, typename Alloc>
auto basic_devbuf<CharT, Alloc>::seek_impl(off_type off, seekdir dir) -> pos_type {
    assert(dev_);
    if (!!(this->mode() & (iomode::z_compr | iomode::append))) { off = 0, dir = seekdir::curr; }
    if (dir == seekdir::curr) {
        const off_type delta = !!(this->mode() & iomode::out) ? static_cast<off_type>(this->pos()) :
                                                                -static_cast<off_type>(this->avail());
        if (off == 0) {
            const std::int64_t dev_pos = dev_->seek(0, seekdir::curr);
            if (dev_pos < 0) { return traits_type::npos(); }
            return static_cast<pos_type>(dev_pos / sizeof(char_type)) + delta;
        }
        off += delta;
    }
    const std::int64_t dev_pos = dev_->seek(static_cast<std::int64_t>(off) * sizeof(char_type), dir);
    if (dev_pos < 0) { return traits_type::npos(); }
    if (!buf_ || !!(this->mode() & iomode::in)) { this->reset(nullptr, 0, 0); }
    return static_cast<pos_type>(dev_pos / sizeof(char_type));
}

}  // namespace uxs
