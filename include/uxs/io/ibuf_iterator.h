#pragma once

#include "ibuf.h"

namespace uxs {

template<typename CharT>
class basic_ibuf_iterator
    : public est::input_iterator_facade<basic_ibuf_iterator<CharT>, CharT, std::input_iterator_tag, CharT, CharT,
                                        typename basic_ibuf<CharT>::traits_type::off_type> {
 public:
    using char_type = CharT;
    using ibuf_type = basic_ibuf<CharT>;
    using traits_type = typename ibuf_type::traits_type;
    using int_type = typename traits_type::int_type;

    basic_ibuf_iterator() noexcept = default;
    explicit basic_ibuf_iterator(ibuf_type& buf) : buf_(&buf) {
        if ((val_ = buf_->peek()) == traits_type::eof()) { buf_ = nullptr; }
    }

    void increment() {
        assert(buf_);
        buf_->advance(1);
        if ((val_ = buf_->peek()) == traits_type::eof()) { buf_ = nullptr; }
    }

    char_type dereference() const noexcept {
        assert(buf_);
        return traits_type::to_char_type(val_);
    }
    bool is_equal_to(const basic_ibuf_iterator& it) const noexcept { return buf_ == it.buf_; }

 private:
    ibuf_type* buf_ = nullptr;
    int_type val_ = traits_type::eof();
};

#define UXS_DECLARE_TYPE_ALIASES(type, prefix) using prefix##ibuf_iterator = basic_ibuf_iterator<type>
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
