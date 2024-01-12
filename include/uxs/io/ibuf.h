#pragma once

#include "iostate.h"
#include "iotraits.h"

#include "uxs/span.h"

#include <cassert>

namespace uxs {

template<typename CharT>
class basic_ibuf : public iostate {
    static_assert(std::is_integral<CharT>::value, "uxs::basic_ibuf must have integral character type");

 public:
    using char_type = CharT;
    using traits_type = iotraits<CharT>;
    using size_type = size_t;
    using int_type = typename traits_type::int_type;
    using pos_type = typename traits_type::pos_type;
    using off_type = typename traits_type::off_type;

    basic_ibuf() = default;
    explicit basic_ibuf(iomode mode) : iostate(mode) {}
    basic_ibuf(iomode mode, iostate_bits state) : iostate(mode, state) {}
    virtual ~basic_ibuf() = default;
    basic_ibuf(const basic_ibuf&) = delete;
    basic_ibuf& operator=(const basic_ibuf&) = delete;
    UXS_EXPORT basic_ibuf(basic_ibuf&& other) noexcept;
    UXS_EXPORT basic_ibuf& operator=(basic_ibuf&& other) noexcept;

    size_type avail() const { return last_ - curr_; }
    const char_type* first_avail() const { return curr_; }
    const char_type* last_avail() const { return last_; }
    uxs::span<const char_type> view_avail() const { return uxs::as_span(curr_, avail()); }

    int_type peek() {
        if (curr_ != last_ || (this->good() && underflow() >= 0)) { return traits_type::to_int_type(*curr_); }
        this->setstate(iostate_bits::eof | iostate_bits::fail);
        return traits_type::eof();
    }

    int_type get() {
        if (curr_ != last_ || (this->good() && underflow() >= 0)) { return traits_type::to_int_type(*curr_++); }
        this->setstate(iostate_bits::eof | iostate_bits::fail);
        return traits_type::eof();
    }

    basic_ibuf& unget() {
        this->setstate(this->rdstate() & ~iostate_bits::eof);
        if (curr_ != first_ || (this->good() && ungetfail() >= 0)) {
            --curr_;
            return *this;
        }
        this->setstate(iostate_bits::fail);
        return *this;
    }

    basic_ibuf& advance(size_type n) {
        assert(n <= static_cast<size_type>(last_ - curr_));
        curr_ += n;
        return *this;
    }

    UXS_EXPORT size_type read(uxs::span<char_type> s);
    UXS_EXPORT size_type skip(size_type count);
    UXS_EXPORT pos_type seek(off_type off, seekdir dir = seekdir::beg);
    pos_type tell() {
        if (this->fail()) { return traits_type::npos(); }
        return seekimpl(0, seekdir::curr);
    }

 protected:
    UXS_EXPORT virtual int underflow();
    UXS_EXPORT virtual int ungetfail();
    UXS_EXPORT virtual pos_type seekimpl(off_type off, seekdir dir);
    UXS_EXPORT virtual int sync();

    char_type* first() const { return first_; }
    char_type* curr() const { return curr_; }
    char_type* last() const { return last_; }

    void putcurr(char_type ch) { *curr_++ = ch; }
    void setcurr(char_type* curr) { curr_ = curr; }
    void setview(char_type* first, char_type* curr, char_type* last) {
        first_ = first;
        curr_ = curr;
        last_ = last;
    }

 private:
    char_type* first_ = nullptr;
    char_type* curr_ = nullptr;
    char_type* last_ = nullptr;
};

using ibuf = basic_ibuf<char>;
using wibuf = basic_ibuf<wchar_t>;
using u8ibuf = basic_ibuf<uint8_t>;

namespace stdbuf {
extern UXS_EXPORT ibuf& in;
}  // namespace stdbuf

}  // namespace uxs
