#pragma once

#include "ibuf.h"

namespace uxs {

template<typename CharT>
class basic_iflatbuf : public basic_ibuf<CharT> {
 public:
    using char_type = typename basic_ibuf<CharT>::char_type;
    using size_type = typename basic_ibuf<CharT>::size_type;
    using pos_type = typename basic_ibuf<CharT>::pos_type;
    using off_type = typename basic_ibuf<CharT>::off_type;

    explicit basic_iflatbuf(est::span<const char_type> s) noexcept : basic_ibuf<CharT>(iomode::in) {
        this->reset(const_cast<char_type*>(s.data()), 0, s.size());
    }

 protected:
    UXS_EXPORT pos_type seek_impl(off_type off, seekdir dir) override;
};

#define UXS_DECLARE_TYPE_ALIASES(type, prefix) using prefix##iflatbuf = basic_iflatbuf<type>
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
