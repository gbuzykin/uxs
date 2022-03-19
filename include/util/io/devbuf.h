#pragma once

#include "iobuf.h"
#include "iodevice.h"

namespace util {

template<typename CharT>
class UTIL_EXPORT basic_devbuf : public basic_iobuf<CharT> {
 public:
    using char_type = CharT;
    using size_type = typename basic_iobuf<CharT>::size_type;

    explicit basic_devbuf(iodevice& dev) : basic_iobuf<CharT>(iomode::kIn, iostate_bits::kFail), dev_(&dev) {}
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
    int overflow(char_type ch) override;
    int sync() override;

    void setdev(iodevice* dev) { dev_ = dev; }

 private:
    enum : unsigned {
#if defined(NDEBUG)
        kMinBufSize = 16384,
        kCrReserveRatio = 16
#else   // defined(NDEBUG)
        kMinBufSize = 13,
        kCrReserveRatio = 7
#endif  // defined(NDEBUG)
    };
    iodevice* dev_ = nullptr;
    size_type bufsz_ = 0;
    basic_iobuf<char_type>* tie_buf_ = nullptr;

    const char_type* find_end_of_ctrlesc(const char_type* first, const char_type* last);
    int write_all(const void* data, size_type sz);
    int read_at_least_one(void* data, size_type sz, size_type& n_read);
    void parse_ctrlesc(const char_type* first, const char_type* last);
    int flush_buffer();
    size_type remove_crlf(char_type* dst, size_type count);
};

using devbuf = basic_devbuf<char>;
using wdevbuf = basic_devbuf<wchar_t>;
using u8devbuf = basic_devbuf<uint8_t>;

}  // namespace util
