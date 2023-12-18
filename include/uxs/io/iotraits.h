#pragma once

#include "uxs/utility.h"

namespace uxs {

template<typename CharT>
struct iotraits {
    using char_type = CharT;
    using int_type = int;
    using pos_type = uint64_t;
    using off_type = int64_t;
    static CONSTEXPR int_type eof() { return -1; }
    static CONSTEXPR int_type to_int_type(char_type ch) {
        return static_cast<int_type>(static_cast<typename std::make_unsigned<char_type>::type>(ch));
    }
    static CONSTEXPR char_type to_char_type(int_type ch) { return static_cast<char_type>(ch); }
};

}  // namespace uxs
