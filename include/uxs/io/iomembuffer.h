#pragma once

#include "iobuf.h"

#include "uxs/membuffer.h"

namespace uxs {

template<typename CharT>
class basic_iomembuffer : public basic_membuffer<CharT> {
 public:
    using size_type = typename basic_membuffer<CharT>::size_type;
    using pointer = typename basic_membuffer<CharT>::pointer;

    explicit basic_iomembuffer(basic_iobuf<CharT>& out) noexcept
        : basic_membuffer<CharT>(out.first(), out.pos(), out.capacity(), try_grow_impl), out_(out) {}
    ~basic_iomembuffer() { flush(); }
    void flush() noexcept { out_.setpos(this->size()); }

 private:
    basic_iobuf<CharT>& out_;

    void reset(pointer data, size_type size, size_type capacity) noexcept {
        basic_membuffer<CharT>::reset(data, size, capacity);
    }

    static size_type try_grow_impl(basic_membuffer<CharT>& buf, size_type /*extra*/, bool /*track_size*/) {
        auto& self = static_cast<basic_iomembuffer&>(buf);
        self.flush();
        if (!self.out_.reserve().good()) { return 0; }
        self.reset(self.out_.first(), self.out_.pos(), self.out_.capacity());
        return buf.avail();
    }
};

#define UXS_DECLARE_TYPE_ALIASES(type, prefix) using prefix##iomembuffer = basic_iomembuffer<type>
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

}  // namespace uxs
