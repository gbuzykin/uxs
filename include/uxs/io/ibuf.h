#pragma once

#include "iostate.h"
#include "iotraits.h"

#include "uxs/iterator.h"
#include "uxs/span.h"

#include <algorithm>

namespace uxs {

template<typename CharT>
class basic_ibuf : public iostate {
    static_assert(std::is_integral<CharT>::value, "uxs::basic_ibuf must have integral character type");

 public:
    using char_type = CharT;
    using traits_type = iotraits<CharT>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using int_type = typename traits_type::int_type;
    using pos_type = typename traits_type::pos_type;
    using off_type = typename traits_type::off_type;

    basic_ibuf() noexcept = default;
    explicit basic_ibuf(iomode mode) noexcept : iostate(mode) {}
    basic_ibuf(iomode mode, iostate_bits state) noexcept : iostate(mode, state) {}
    virtual ~basic_ibuf() = default;
    basic_ibuf(const basic_ibuf&) = delete;
    basic_ibuf& operator=(const basic_ibuf&) = delete;
    UXS_EXPORT basic_ibuf(basic_ibuf&& other) noexcept;
    UXS_EXPORT basic_ibuf& operator=(basic_ibuf&& other) noexcept;

    size_type pos() const noexcept { return pos_; }
    size_type capacity() const noexcept { return capacity_; }
    size_type avail() const noexcept { return capacity_ - pos_; }
    const char_type* first() const noexcept { return pbase_; }
    const char_type* curr() const noexcept { return pbase_ + pos_; }
    const char_type* last() const noexcept { return pbase_ + capacity_; }
    est::span<const char_type> avail_view() const noexcept { return est::as_span(curr(), avail()); }

    void setpos(size_type pos) noexcept {
        assert(pos <= capacity_);
        pos_ = pos;
    }

    int_type peek() {
        if (avail() || (this->good() && underflow() >= 0)) { return traits_type::to_int_type(*curr()); }
        this->setstate(iostate_bits::eof | iostate_bits::fail);
        return traits_type::eof();
    }

    int_type get() {
        if (avail() || (this->good() && underflow() >= 0)) { return traits_type::to_int_type(next()); }
        this->setstate(iostate_bits::eof | iostate_bits::fail);
        return traits_type::eof();
    }

    basic_ibuf& unget() {
        this->setstate(this->rdstate() & ~iostate_bits::eof);
        if (pos_ || (this->good() && ungetfail() >= 0)) {
            --pos_;
            return *this;
        }
        this->setstate(iostate_bits::fail);
        return *this;
    }

    void advance(difference_type n) noexcept {
        assert(n >= 0 ? static_cast<size_type>(n) <= avail() : static_cast<size_type>(-n) <= pos_);
        pos_ += n;
    }

    template<typename OutputIt, typename = std::enable_if_t<is_random_access_iterator<OutputIt>::value &&
                                                            is_output_iterator<OutputIt, CharT>::value>>
    size_type read(OutputIt first, OutputIt last) {
        assert(first <= last);
        const size_type sz = static_cast<size_type>(last - first);
        size_type count = sz;
        if (!count) { return 0; }
        for (size_type n_avail = avail(); count > n_avail; n_avail = avail()) {
            if (n_avail) { first = std::copy_n(curr(), n_avail, first), pos_ = capacity_, count -= n_avail; }
            if (!this->good() || underflow() < 0) {
                this->setstate(iostate_bits::eof | iostate_bits::fail);
                return sz - count;
            }
        }
        std::copy_n(curr(), count, first), pos_ += count;
        return sz;
    }

    UXS_EXPORT size_type read(est::span<char_type> s);
    UXS_EXPORT size_type read_with_endian(est::span<char_type> s, size_type element_sz);
    template<typename Ty, std::size_t N>
    size_type read(Ty (&)[N]) = delete;
    UXS_EXPORT size_type skip(size_type count);
    UXS_EXPORT pos_type seek(off_type off, seekdir dir = seekdir::beg);
    pos_type tell() {
        if (this->fail()) { return traits_type::npos(); }
        return seek_impl(0, seekdir::curr);
    }

 protected:
    UXS_EXPORT virtual int underflow();
    UXS_EXPORT virtual int ungetfail();
    UXS_EXPORT virtual pos_type seek_impl(off_type off, seekdir dir);
    UXS_EXPORT virtual int sync();

    char_type* pbase() const noexcept { return pbase_; }

    char_type& next() noexcept {
        assert(avail());
        return pbase_[pos_++];
    }

    void reset(char_type* pbase, size_type pos, size_type capacity) noexcept {
        pbase_ = pbase, pos_ = pos, capacity_ = capacity;
    }

 private:
    char_type* pbase_ = nullptr;
    size_type pos_ = 0;
    size_type capacity_ = 0;
};

using ibuf = basic_ibuf<char>;
using wibuf = basic_ibuf<wchar_t>;
using bibuf = basic_ibuf<std::uint8_t>;

namespace stdbuf {
extern UXS_EXPORT ibuf& in();
}  // namespace stdbuf

}  // namespace uxs
