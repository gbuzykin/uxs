#pragma once

#include "iobuf.h"

namespace uxs {

template<typename CharT>
class UXS_EXPORT basic_istringbuf : public basic_iobuf<CharT> {
 public:
    using char_type = typename basic_iobuf<CharT>::char_type;
    using size_type = typename basic_iobuf<CharT>::size_type;
    using int_type = typename basic_iobuf<CharT>::int_type;
    using pos_type = typename basic_iobuf<CharT>::pos_type;
    using off_type = typename basic_iobuf<CharT>::off_type;

    explicit basic_istringbuf(std::basic_string<char_type> s) : basic_iobuf<CharT>(iomode::kIn), str_(std::move(s)) {
        redirect_ptrs();
    }
    ~basic_istringbuf() override = default;
    basic_istringbuf(basic_istringbuf&& other) NOEXCEPT;
    basic_istringbuf& operator=(basic_istringbuf&& other) NOEXCEPT;

 protected:
    pos_type seekimpl(off_type off, seekdir dir) override;

 private:
    std::basic_string<char_type> str_;

    void redirect_ptrs();
};

using istringbuf = basic_istringbuf<char>;
using wistringbuf = basic_istringbuf<wchar_t>;

}  // namespace uxs
