#pragma once

#include "iostate.h"
#include "iotraits.h"

#include "uxs/iterator.h"
#include "uxs/span.h"
#include "uxs/string_view.h"

#include <algorithm>

namespace uxs {

template<typename CharT>
class basic_iobuf : public iostate {
    static_assert(std::is_integral<CharT>::value, "uxs::basic_iobuf must have integral character type");

 public:
    using char_type = CharT;
    using traits_type = iotraits<CharT>;
    using size_type = size_t;
    using int_type = typename traits_type::int_type;
    using pos_type = typename traits_type::pos_type;
    using off_type = typename traits_type::off_type;

    basic_iobuf() = default;
    explicit basic_iobuf(iomode mode) : iostate(mode) {}
    basic_iobuf(iomode mode, iostate_bits state) : iostate(mode, state) {}
    virtual ~basic_iobuf() = default;
    basic_iobuf(const basic_iobuf&) = delete;
    basic_iobuf& operator=(const basic_iobuf&) = delete;
    UXS_EXPORT basic_iobuf(basic_iobuf&& other) NOEXCEPT;
    UXS_EXPORT basic_iobuf& operator=(basic_iobuf&& other) NOEXCEPT;

    size_type avail() const { return last_ - curr_; }
    char_type* first_avail() const { return curr_; }
    char_type* last_avail() const { return last_; }
    uxs::span<char_type> view_avail() const { return uxs::as_span(curr_, avail()); }

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

    basic_iobuf& unget() {
        this->setstate(this->rdstate() & ~iostate_bits::eof);
        if (curr_ != first_) {
            --curr_;
            return *this;
        }
        this->setstate(iostate_bits::fail);
        return *this;
    }

    basic_iobuf& unget(char_type ch) {
        this->setstate(this->rdstate() & ~iostate_bits::eof);
        if (curr_ != first_ || (this->good() && ungetfail() >= 0)) {
            *--curr_ = ch;
            return *this;
        }
        this->setstate(iostate_bits::fail);
        return *this;
    }

    basic_iobuf& reserve() {
        if (curr_ != last_ || (this->good() && overflow() >= 0)) { return *this; }
        this->setstate(iostate_bits::bad);
        return *this;
    }

    basic_iobuf& put(char_type ch) {
        if (curr_ != last_ || (this->good() && overflow() >= 0)) {
            *curr_++ = ch;
            return *this;
        }
        this->setstate(iostate_bits::bad);
        return *this;
    }

    basic_iobuf& advance(size_type n) {
        assert(n <= static_cast<size_type>(last_ - curr_));
        curr_ += n;
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_random_access_iterator<InputIt>::value>>
    basic_iobuf& write(InputIt first, InputIt last) {
        size_type count = last - first;
        if (!count) { return *this; }
        for (size_type n_avail = avail(); count > n_avail; n_avail = avail()) {
            if (n_avail) { curr_ = std::copy_n(first, n_avail, curr_), first += n_avail, count -= n_avail; }
            if (!this->good() || overflow() < 0) {
                this->setstate(iostate_bits::bad);
                return *this;
            }
        }
        curr_ = std::copy(first, last, curr_);
        return *this;
    }

    UXS_EXPORT size_type read(uxs::span<char_type> s);
    UXS_EXPORT size_type skip(size_type count);
    UXS_EXPORT basic_iobuf& write(uxs::span<const char_type> s);
    basic_iobuf& write(const char_type* cstr) { return write(std::basic_string_view<char_type>(cstr)); }
    UXS_EXPORT basic_iobuf& fill_n(size_type count, char_type ch);
    UXS_EXPORT basic_iobuf& flush();
    basic_iobuf& endl() { return put('\n').flush(); }

    UXS_EXPORT pos_type seek(off_type off, seekdir dir = seekdir::beg);
    pos_type tell() {
        if (this->fail()) { return traits_type::npos(); }
        return seekimpl(0, seekdir::curr);
    }

 protected:
    UXS_EXPORT virtual int underflow();
    UXS_EXPORT virtual int ungetfail();
    UXS_EXPORT virtual int overflow();
    UXS_EXPORT virtual pos_type seekimpl(off_type off, seekdir dir);
    UXS_EXPORT virtual int sync();

    char_type* first() const { return first_; }
    char_type* curr() const { return curr_; }
    char_type* last() const { return last_; }

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

using iobuf = basic_iobuf<char>;
using wiobuf = basic_iobuf<wchar_t>;
using u8iobuf = basic_iobuf<uint8_t>;

namespace stdbuf {
extern UXS_EXPORT iobuf& out;
extern UXS_EXPORT iobuf& in;
extern UXS_EXPORT iobuf& log;
extern UXS_EXPORT iobuf& err;
}  // namespace stdbuf

}  // namespace uxs
