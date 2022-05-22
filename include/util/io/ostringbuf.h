#pragma once

#include "iobuf.h"

namespace util {

template<typename CharT>
class UTIL_EXPORT basic_ostringbuf : public basic_iobuf<CharT> {
 public:
    using char_type = typename basic_iobuf<CharT>::char_type;
    using size_type = typename basic_iobuf<CharT>::size_type;
    using int_type = typename basic_iobuf<CharT>::int_type;
    using pos_type = typename basic_iobuf<CharT>::pos_type;
    using off_type = typename basic_iobuf<CharT>::off_type;

    basic_ostringbuf() : basic_iobuf<CharT>(iomode::kOut) {}
    ~basic_ostringbuf() override;
    basic_ostringbuf(basic_ostringbuf&& other) NOEXCEPT;
    basic_ostringbuf& operator=(basic_ostringbuf&& other) NOEXCEPT;

    size_type size() const { return (top_ > this->curr() ? top_ : this->curr()) - this->first(); }
    std::basic_string_view<char_type> view() const { return std::basic_string_view<char_type>(this->first(), size()); }
    std::basic_string<char_type> str() const { return std::basic_string<char_type>(this->first(), size()); }

    void truncate(size_type sz) {
        if (sz > size()) { sz = size(); }
        top_ = this->first() + sz;
        this->setcurr(this->curr() > top_ ? top_ : this->curr());
    }

 protected:
    int overflow(char_type ch) override;
    int sync() override;
    pos_type seekimpl(off_type off, seekdir dir) override;

 private:
    enum : unsigned {
#if defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
        kMinBufSize = 512 / sizeof(char_type)
#else   // defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
        kMinBufSize = 7
#endif  // defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
    };
    char_type* top_ = nullptr;

    void grow(size_type extra);
};

using ostringbuf = basic_ostringbuf<char>;
using wostringbuf = basic_ostringbuf<wchar_t>;

}  // namespace util
