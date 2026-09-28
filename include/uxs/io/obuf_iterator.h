#pragma once

#include "iobuf.h"

namespace uxs {

template<typename CharT>
class basic_obuf_iterator {
 public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using reference = void;
    using pointer = void;
    using char_type = CharT;
    using obuf_type = basic_iobuf<CharT>;
    using traits_type = typename obuf_type::traits_type;
    using int_type = typename traits_type::int_type;

    explicit basic_obuf_iterator(obuf_type& buf) noexcept : buf_(&buf) {}

    basic_obuf_iterator& operator=(char_type ch) {
        assert(buf_);
        buf_->put(ch);
        return *this;
    }

    bool failed() const noexcept { return buf_->eof(); }

    basic_obuf_iterator& operator*() { return *this; }
    basic_obuf_iterator& operator++() { return *this; }
    basic_obuf_iterator operator++(int) { return *this; }

 private:
    obuf_type* buf_;
};

#define UXS_DECLARE_TYPE_ALIASES(type, prefix) using prefix##obuf_iterator = basic_obuf_iterator<type>
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
