#pragma once

#include "uxs/utility.h"

namespace uxs {

template<typename CharT>
struct iotraits {
    using char_type = CharT;
    using int_type = int;
    using pos_type = std::uint64_t;
    using off_type = std::int64_t;
    static UXS_CONSTEXPR pos_type npos() noexcept { return ~pos_type(0); }
    static UXS_CONSTEXPR int_type eof() noexcept { return -1; }
    static UXS_CONSTEXPR int_type to_int_type(char_type ch) noexcept {
        return static_cast<int_type>(static_cast<typename std::make_unsigned<char_type>::type>(ch));
    }
    static UXS_CONSTEXPR char_type to_char_type(int_type ch) noexcept { return static_cast<char_type>(ch); }
};

}  // namespace uxs
