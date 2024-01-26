#pragma once

#include "ibuf.h"

namespace uxs {

template<typename CharT>
class basic_iobuf : public basic_ibuf<CharT> {
 public:
    using char_type = typename basic_ibuf<CharT>::char_type;
    using traits_type = typename basic_ibuf<CharT>::traits_type;
    using size_type = typename basic_ibuf<CharT>::size_type;
    using int_type = typename basic_ibuf<CharT>::int_type;
    using pos_type = typename basic_ibuf<CharT>::pos_type;
    using off_type = typename basic_ibuf<CharT>::off_type;

    basic_iobuf() = default;
    explicit basic_iobuf(iomode mode) : basic_ibuf<CharT>(mode) {}
    basic_iobuf(iomode mode, iostate_bits state) : basic_ibuf<CharT>(mode, state) {}

    char_type* first_avail() const { return this->curr(); }
    char_type* last_avail() const { return this->last(); }
    uxs::span<char_type> view_avail() const { return uxs::as_span(this->curr(), this->avail()); }

    basic_iobuf& reserve() {
        if (this->curr() != this->last() || (this->good() && overflow() >= 0)) { return *this; }
        this->setstate(iostate_bits::bad);
        return *this;
    }

    basic_iobuf& put(char_type ch) {
        if (this->curr() != this->last() || (this->good() && overflow() >= 0)) {
            this->putcurr(ch);
            return *this;
        }
        this->setstate(iostate_bits::bad);
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_random_access_iterator<InputIt>::value>>
    basic_iobuf& write(InputIt first, InputIt last) {
        assert(first <= last);
        size_type count = static_cast<size_type>(last - first);
        if (!count) { return *this; }
        for (size_type n_avail = this->avail(); count > n_avail; n_avail = this->avail()) {
            if (n_avail) {
                this->setcurr(std::copy_n(first, n_avail, this->curr()));
                first += n_avail, count -= n_avail;
            }
            if (!this->good() || overflow() < 0) {
                this->setstate(iostate_bits::bad);
                return *this;
            }
        }
        this->setcurr(std::copy(first, last, this->curr()));
        return *this;
    }

    UXS_EXPORT basic_iobuf& write(uxs::span<const char_type> s);
    UXS_EXPORT basic_iobuf& write_with_endian(uxs::span<const char_type> s, size_type element_sz);
    template<typename Ty, size_t N>
    basic_iobuf& write(Ty (&)[N]) = delete;
    UXS_EXPORT basic_iobuf& fill_n(size_type count, char_type ch);
    UXS_EXPORT basic_iobuf& flush();
    basic_iobuf& endl() { return put('\n').flush(); }

 protected:
    UXS_EXPORT virtual int overflow();
};

using iobuf = basic_iobuf<char>;
using wiobuf = basic_iobuf<wchar_t>;
using u8iobuf = basic_iobuf<uint8_t>;

namespace stdbuf {
extern UXS_EXPORT iobuf& out;
extern UXS_EXPORT iobuf& log;
extern UXS_EXPORT iobuf& err;
}  // namespace stdbuf

}  // namespace uxs
