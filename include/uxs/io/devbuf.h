#pragma once

#include "iobuf.h"
#include "iodevice.h"

#include <memory>

namespace uxs {

template<typename CharT, typename Alloc = std::allocator<CharT>>
class basic_devbuf : protected std::allocator_traits<Alloc>::template rebind_alloc<CharT>, public basic_iobuf<CharT> {
 protected:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<CharT>;

 public:
    using char_type = typename basic_iobuf<CharT>::char_type;
    using traits_type = typename basic_iobuf<CharT>::traits_type;
    using size_type = typename basic_iobuf<CharT>::size_type;
    using int_type = typename basic_iobuf<CharT>::int_type;
    using pos_type = typename basic_iobuf<CharT>::pos_type;
    using off_type = typename basic_iobuf<CharT>::off_type;
    using allocator_type = Alloc;

    explicit basic_devbuf(iodevice& dev)
        : alloc_type(), basic_iobuf<CharT>(iomode::none, iostate_bits::fail), dev_(&dev) {}
    basic_devbuf(iodevice& dev, const Alloc& al)
        : alloc_type(al), basic_iobuf<CharT>(iomode::none, iostate_bits::fail), dev_(&dev) {}
    basic_devbuf(iodevice& dev, iomode mode, size_type bufsz = 0) : alloc_type(), dev_(&dev) { initbuf(mode, bufsz); }
    basic_devbuf(iodevice& dev, iomode mode, size_type bufsz, const Alloc& al) : alloc_type(al), dev_(&dev) {
        initbuf(mode, bufsz);
    }
    UXS_EXPORT ~basic_devbuf() override;
    UXS_EXPORT basic_devbuf(basic_devbuf&& other) noexcept;
    UXS_EXPORT basic_devbuf& operator=(basic_devbuf&& other) noexcept;

    iodevice* dev() const { return dev_; }
    basic_iobuf<CharT>* tie() const { return tie_buf_; }
    void settie(basic_iobuf<CharT>* tie) { tie_buf_ = tie; }

    UXS_EXPORT void initbuf(iomode mode, size_type bufsz = 0);
    UXS_EXPORT void freebuf();
    allocator_type get_allocator() const noexcept { return allocator_type(*this); }

 protected:
    UXS_EXPORT int underflow() override;
    UXS_EXPORT int overflow() override;
    UXS_EXPORT pos_type seekimpl(off_type off, seekdir dir) override;
    UXS_EXPORT int sync() override;

    void setdev(iodevice* dev) { dev_ = dev; }

 private:
    enum : unsigned {
#if defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
        min_buf_size = 16384 / sizeof(char_type),
        cr_reserve_ratio = 16,
#else   // defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
        min_buf_size = 15,
        cr_reserve_ratio = 7,
#endif  // defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
        max_buf_size = 1024 * 1024 * 1024 / sizeof(char_type),
    };

    struct flexbuf_t;
    iodevice* dev_ = nullptr;
    flexbuf_t* buf_ = nullptr;
    basic_iobuf<char_type>* tie_buf_ = nullptr;

    UXS_EXPORT const char_type* find_end_of_ctrlesc(const char_type* first, const char_type* last);
    UXS_EXPORT int write_buf(const void* data, std::size_t sz);
    UXS_EXPORT int read_buf(void* data, std::size_t sz, std::size_t& n_read);
    UXS_EXPORT int flush_compressed_buf();
    UXS_EXPORT int write_compressed(const void* data, std::size_t sz);
    UXS_EXPORT void finish_compressed();
    UXS_EXPORT int read_compressed(void* data, std::size_t sz, std::size_t& n_read);
    UXS_EXPORT void parse_ctrlesc(const char_type* first, const char_type* last);
    UXS_EXPORT int flush_buffer();
    UXS_EXPORT std::size_t remove_crlf(char_type* dst, std::size_t count);
};

using devbuf = basic_devbuf<char>;
using wdevbuf = basic_devbuf<wchar_t>;
using u8devbuf = basic_devbuf<std::uint8_t>;

}  // namespace uxs
