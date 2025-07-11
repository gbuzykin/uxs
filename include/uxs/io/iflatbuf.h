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

using iflatbuf = basic_iflatbuf<char>;
using wiflatbuf = basic_iflatbuf<wchar_t>;
using biflatbuf = basic_iflatbuf<std::uint8_t>;

}  // namespace uxs
