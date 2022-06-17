#pragma once

#include "iobuf.h"
#include "iodevice.h"

namespace uxs {

template<typename CharT>
class UXS_EXPORT basic_devbuf : public basic_iobuf<CharT> {
 public:
    using char_type = CharT;
    using size_type = typename basic_iobuf<CharT>::size_type;
    using int_type = typename basic_iobuf<CharT>::int_type;
    using pos_type = typename basic_iobuf<CharT>::pos_type;
    using off_type = typename basic_iobuf<CharT>::off_type;

    explicit basic_devbuf(iodevice& dev) : basic_iobuf<CharT>(iomode::kNone, iostate_bits::kFail), dev_(&dev) {}
    basic_devbuf(iodevice& dev, iomode mode, size_type bufsz = 0) : dev_(&dev) { initbuf(mode, bufsz); }
    ~basic_devbuf() override;
    basic_devbuf(basic_devbuf&& other) NOEXCEPT;
    basic_devbuf& operator=(basic_devbuf&& other);

    iodevice* dev() const { return dev_; }
    basic_iobuf<CharT>* tie() const { return tie_buf_; }
    void settie(basic_iobuf<CharT>* tie) { tie_buf_ = tie; }

    void initbuf(iomode mode, size_type bufsz = 0);
    void freebuf();

 protected:
    int underflow() override;
    int overflow() override;
    pos_type seekimpl(off_type off, seekdir dir) override;
    int sync() override;

    void setdev(iodevice* dev) { dev_ = dev; }

 private:
    enum : unsigned {
#if defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
        kMinBufSize = 16384 / sizeof(char_type),
        kCrReserveRatio = 16,
#else   // defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
        kMinBufSize = 15,
        kCrReserveRatio = 7,
#endif  // defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
        kMaxBufSize = 1024 * 1024 * 1024 / sizeof(char_type),
    };

    struct flexbuffer;
    iodevice* dev_ = nullptr;
    flexbuffer* buf_ = nullptr;
    pos_type pos_ = 0;
    basic_iobuf<char_type>* tie_buf_ = nullptr;

    const char_type* find_end_of_ctrlesc(const char_type* first, const char_type* last);
    int write_buf(const void* data, size_t sz);
    int read_buf(void* data, size_t sz, size_t& n_read);
    int flush_compressed_buf();
    int write_compressed(const void* data, size_t sz);
    void finish_compressed();
    int read_compressed(void* data, size_t sz, size_t& n_read);
    void parse_ctrlesc(const char_type* first, const char_type* last);
    int flush_buffer();
    size_t remove_crlf(char_type* dst, size_t count);
};

using devbuf = basic_devbuf<char>;
using wdevbuf = basic_devbuf<wchar_t>;
using u8devbuf = basic_devbuf<uint8_t>;

}  // namespace uxs
