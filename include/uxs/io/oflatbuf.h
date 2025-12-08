#pragma once

#include "iobuf.h"

namespace uxs {

template<typename CharT, typename Alloc = std::allocator<CharT>>
class basic_oflatbuf : protected std::allocator_traits<Alloc>::template rebind_alloc<CharT>, public basic_iobuf<CharT> {
 protected:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<CharT>;

 public:
    using char_type = typename basic_iobuf<CharT>::char_type;
    using traits_type = typename basic_iobuf<CharT>::traits_type;
    using size_type = typename basic_iobuf<CharT>::size_type;
    using pos_type = typename basic_iobuf<CharT>::pos_type;
    using off_type = typename basic_iobuf<CharT>::off_type;
    using allocator_type = Alloc;

    basic_oflatbuf() : alloc_type(), basic_iobuf<CharT>(iomode::out) {}
    explicit basic_oflatbuf(const Alloc& al) : alloc_type(al), basic_iobuf<CharT>(iomode::out) {}
    UXS_EXPORT ~basic_oflatbuf() override;
    UXS_EXPORT basic_oflatbuf(basic_oflatbuf&& other) noexcept;
    UXS_EXPORT basic_oflatbuf& operator=(basic_oflatbuf&& other) noexcept;

    const char_type* data() const noexcept { return this->first(); }
    size_type size() const noexcept { return std::max(top_, this->pos()); }
    est::span<const char_type> view() const noexcept { return est::as_span(this->first(), size()); }
    allocator_type get_allocator() const noexcept { return allocator_type(*this); }

 protected:
    UXS_EXPORT int overflow() override;
    UXS_EXPORT int sync() override;
    UXS_EXPORT int truncate_impl() override;
    UXS_EXPORT pos_type seek_impl(off_type off, seekdir dir) override;

 private:
    enum : unsigned {
#if !defined(NDEBUG) && UXS_DEBUG_REDUCED_BUFFERS != 0
        min_buf_size = 7
#else   // !defined(NDEBUG) && UXS_DEBUG_REDUCED_BUFFERS != 0
        min_buf_size = 512 / sizeof(char_type)
#endif  // !defined(NDEBUG) && UXS_DEBUG_REDUCED_BUFFERS != 0
    };

    size_type top_ = 0;

    UXS_EXPORT void grow(size_type extra);
};

using oflatbuf = basic_oflatbuf<char>;
using woflatbuf = basic_oflatbuf<wchar_t>;
using boflatbuf = basic_oflatbuf<std::uint8_t>;

}  // namespace uxs
