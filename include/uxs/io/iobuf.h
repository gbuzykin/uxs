#pragma once

#include "ibuf.h"

namespace uxs {

template<typename CharT>
class basic_iobuf : public basic_ibuf<CharT> {
 public:
    using char_type = typename basic_ibuf<CharT>::char_type;
    using size_type = typename basic_ibuf<CharT>::size_type;

    basic_iobuf() noexcept = default;
    explicit basic_iobuf(iomode mode) noexcept : basic_ibuf<CharT>(mode) {}
    basic_iobuf(iomode mode, iostate_bits state) noexcept : basic_ibuf<CharT>(mode, state) {}

    char_type* first() const noexcept { return this->pbase(); }
    char_type* curr() const noexcept { return this->pbase() + this->pos(); }
    char_type* last() const noexcept { return this->pbase() + this->capacity(); }
    est::span<char_type> avail_view() const noexcept { return est::as_span(curr(), this->avail()); }

    basic_iobuf& reserve() {
        if (this->avail() || (this->good() && overflow() >= 0)) { return *this; }
        this->setstate(iostate_bits::bad);
        return *this;
    }

    basic_iobuf& put(char_type ch) {
        if (this->avail() || (this->good() && overflow() >= 0)) {
            this->next() = ch;
            return *this;
        }
        this->setstate(iostate_bits::bad);
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<est::is_random_access_iterator<InputIt>::value>>
    basic_iobuf& write(InputIt first, InputIt last) {
        assert(first <= last);
        size_type count = static_cast<size_type>(last - first);
        if (!count) { return *this; }
        for (size_type n_avail = this->avail(); count > n_avail; n_avail = this->avail()) {
            std::copy_n(first, n_avail, curr());
            this->setpos(this->capacity());
            first += n_avail;
            count -= n_avail;
            if (!this->good() || overflow() < 0) {
                this->setstate(iostate_bits::bad);
                return *this;
            }
        }
        std::copy(first, last, curr());
        this->advance(count);
        return *this;
    }

    UXS_EXPORT basic_iobuf& write(est::span<const char_type> s);
    UXS_EXPORT basic_iobuf& write_with_endian(est::span<const char_type> s, size_type element_sz);
    template<typename Ty, std::size_t N>
    basic_iobuf& write(Ty (&)[N]) = delete;
    UXS_EXPORT basic_iobuf& fill_n(size_type count, char_type ch);
    UXS_EXPORT basic_iobuf& flush();
    basic_iobuf& endl() { return put('\n').flush(); }

    UXS_EXPORT void truncate();

 protected:
    UXS_EXPORT virtual int overflow();
    UXS_EXPORT virtual int truncate_impl();
};

#define UXS_DECLARE_TYPE_ALIASES(type, prefix) using prefix##iobuf = basic_iobuf<type>
UXS_DECLARE_TYPE_ALIASES(char, );
#if UXS_USE_WCHAR_T != 0
UXS_DECLARE_TYPE_ALIASES(wchar_t, w);
#endif  // UXS_USE_WCHAR_T != 0
#if UXS_USE_CHAR8_T != 0
UXS_DECLARE_TYPE_ALIASES(char8_t, u8);
#endif  // UXS_USE_CHAR8_T != 0
UXS_DECLARE_TYPE_ALIASES(char16_t, u16);
UXS_DECLARE_TYPE_ALIASES(char32_t, u32);
UXS_DECLARE_TYPE_ALIASES(std::uint8_t, b);
#undef UXS_DECLARE_TYPE_ALIASES

namespace stdbuf {
UXS_EXPORT extern iobuf& out();
UXS_EXPORT extern iobuf& log();
UXS_EXPORT extern iobuf& err();
}  // namespace stdbuf

}  // namespace uxs
