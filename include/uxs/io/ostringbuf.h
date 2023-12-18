#pragma once

#include "iobuf.h"

#include <memory>
#include <string>

namespace uxs {

template<typename CharT, typename Alloc = std::allocator<CharT>>
class basic_ostringbuf : protected std::allocator_traits<Alloc>::template rebind_alloc<CharT>,
                         public basic_iobuf<CharT> {
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

    basic_ostringbuf() : alloc_type(), basic_iobuf<CharT>(iomode::out) {}
    explicit basic_ostringbuf(const Alloc& al) : alloc_type(al), basic_iobuf<CharT>(iomode::out) {}
    UXS_EXPORT ~basic_ostringbuf() override;
    UXS_EXPORT basic_ostringbuf(basic_ostringbuf&& other) NOEXCEPT;
    UXS_EXPORT basic_ostringbuf& operator=(basic_ostringbuf&& other) NOEXCEPT;

    size_type size() const { return (top_ > this->curr() ? top_ : this->curr()) - this->first(); }
    std::basic_string_view<char_type> view() const { return std::basic_string_view<char_type>(this->first(), size()); }
    std::basic_string<char_type> str() const { return std::basic_string<char_type>(this->first(), size()); }
    allocator_type get_allocator() const NOEXCEPT { return allocator_type(*this); }

    void truncate(size_type sz) {
        if (sz > size()) { sz = size(); }
        top_ = this->first() + sz;
        this->setcurr(this->curr() > top_ ? top_ : this->curr());
    }

 protected:
    UXS_EXPORT int overflow() override;
    UXS_EXPORT int sync() override;
    UXS_EXPORT pos_type seekimpl(off_type off, seekdir dir) override;

 private:
    enum : unsigned {
#if defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
        min_buf_size = 512 / sizeof(char_type)
#else   // defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
        min_buf_size = 7
#endif  // defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
    };
    char_type* top_ = nullptr;

    UXS_EXPORT void grow(size_type extra);
};

using ostringbuf = basic_ostringbuf<char>;
using wostringbuf = basic_ostringbuf<wchar_t>;

}  // namespace uxs
