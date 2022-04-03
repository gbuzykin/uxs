#pragma once

#include "chars.h"
#include "string_view.h"
#include "utility.h"

#include <algorithm>

namespace util {

template<typename InputIt, typename InputFn = nofunc>
unsigned from_hex(InputIt in, int digs, InputFn fn = InputFn{}, bool* ok = nullptr) {
    unsigned val = 0;
    while (digs > 0) {
        int ch = fn(*in++);
        val <<= 4;
        --digs;
        int dig_v = xdigit_v(ch);
        if (dig_v >= 0) {
            val |= dig_v;
        } else {
            if (ok) { *ok = false; }
            return val;
        }
    }
    if (ok) { *ok = true; }
    return val;
}

template<typename OutputIt, typename OutputFn = nofunc>
void to_hex(unsigned val, OutputIt out, int digs, OutputFn fn = OutputFn{}) {
    int shift = digs << 2;
    while (shift > 0) {
        shift -= 4;
        *out++ = fn("0123456789ABCDEF"[(val >> shift) & 0xf]);
    }
}

enum class fmt_flags : unsigned {
    kDefault = 0,
    kDec = kDefault,
    kBin = 1,
    kOct = 2,
    kHex = 3,
    kBaseField = 3,
    kFixed = 4,
    kScientific = 8,
    kFloatField = 12,
    kRight = kDefault,
    kLeft = 0x10,
    kInternal = 0x20,
    kAdjustField = 0x30,
    kLeadingZeroes = 0x40,
    kUpperCase = 0x80,
    kShowBase = 0x100,
    kShowPoint = 0x200,
    kSignNeg = kDefault,
    kSignPos = 0x400,
    kSignAlign = 0x800,
    kSignField = 0xc00,
};
UTIL_IMPLEMENT_BITWISE_OPS_FOR_ENUM(fmt_flags, unsigned);

struct fmt_state {
    CONSTEXPR fmt_state() = default;
    CONSTEXPR fmt_state(fmt_flags fl) : flags(fl) {}
    CONSTEXPR fmt_state(fmt_flags fl, int p) : flags(fl), prec(p) {}
    CONSTEXPR fmt_state(fmt_flags fl, int p, unsigned w, int ch) : flags(fl), prec(p), width(w), fill(ch) {}
    fmt_flags flags = fmt_flags::kDec;
    int prec = -1;
    unsigned width = 0;
    int fill = ' ';
};

template<typename CharT>
class basic_char_buf_appender {
 public:
    using value_type = CharT;

    explicit basic_char_buf_appender(CharT* dst) : dst_(dst) {}
    CharT* get_ptr() const { return dst_; }
    basic_char_buf_appender& append(const CharT* first, const CharT* last) {
        dst_ = std::copy(first, last, dst_);
        return *this;
    }
    basic_char_buf_appender& append(size_t count, CharT ch) {
        dst_ = std::fill_n(dst_, count, ch);
        return *this;
    }
    void push_back(CharT ch) { *dst_++ = ch; }

 private:
    CharT* dst_;
};

template<typename CharT>
class basic_char_n_buf_appender {
 public:
    using value_type = CharT;

    basic_char_n_buf_appender(CharT* dst, size_t n) : dst_(dst), last_(dst + n) {}
    CharT* get_ptr() const { return dst_; }
    basic_char_n_buf_appender& append(const CharT* first, const CharT* last) {
        dst_ = std::copy_n(first, std::min<size_t>(last - first, last_ - dst_), dst_);
        return *this;
    }
    basic_char_n_buf_appender& append(size_t count, CharT ch) {
        dst_ = std::fill_n(dst_, std::min<size_t>(count, last_ - dst_), ch);
        return *this;
    }
    void push_back(CharT ch) {
        if (dst_ != last_) { *dst_++ = ch; }
    }

 private:
    CharT* dst_;
    CharT* last_;
};

template<typename CharT, typename Ty>
struct basic_string_converter;

using char_buf_appender = basic_char_buf_appender<char>;
using wchar_buf_appender = basic_char_buf_appender<wchar_t>;
using char_n_buf_appender = basic_char_n_buf_appender<char>;
using wchar_n_buf_appender = basic_char_n_buf_appender<wchar_t>;
template<typename Ty>
using string_converter = basic_string_converter<char, Ty>;
template<typename Ty>
using wstring_converter = basic_string_converter<wchar_t, Ty>;

template<typename Ty>
struct string_converter_base {
    using is_string_converter = int;
    static Ty default_value() { return {}; }
};

#define SCVT_DECLARE_STANDARD_STRING_CONVERTER(ty) \
    template<typename CharT> \
    struct UTIL_EXPORT basic_string_converter<CharT, ty> : string_converter_base<ty> { \
        static const CharT* from_string(const CharT* first, const CharT* last, ty& val); \
        template<typename StrTy> \
        static StrTy& to_string(ty val, StrTy& s, const fmt_state& fmt); \
    };
SCVT_DECLARE_STANDARD_STRING_CONVERTER(int8_t)
SCVT_DECLARE_STANDARD_STRING_CONVERTER(int16_t)
SCVT_DECLARE_STANDARD_STRING_CONVERTER(int32_t)
SCVT_DECLARE_STANDARD_STRING_CONVERTER(int64_t)
SCVT_DECLARE_STANDARD_STRING_CONVERTER(uint8_t)
SCVT_DECLARE_STANDARD_STRING_CONVERTER(uint16_t)
SCVT_DECLARE_STANDARD_STRING_CONVERTER(uint32_t)
SCVT_DECLARE_STANDARD_STRING_CONVERTER(uint64_t)
SCVT_DECLARE_STANDARD_STRING_CONVERTER(float)
SCVT_DECLARE_STANDARD_STRING_CONVERTER(double)
SCVT_DECLARE_STANDARD_STRING_CONVERTER(CharT)
SCVT_DECLARE_STANDARD_STRING_CONVERTER(bool)
#undef SCVT_DECLARE_STANDARD_STRING_CONVERTER

template<typename Ty, typename Def>
Ty from_string(std::string_view s, Def&& def) {
    Ty result(std::forward<Def>(def));
    string_converter<Ty>::from_string(s.data(), s.data() + s.size(), result);
    return result;
}

template<typename Ty, typename Def>
Ty from_wstring(std::wstring_view s, Def&& def) {
    Ty result(std::forward<Def>(def));
    wstring_converter<Ty>::from_string(s.data(), s.data() + s.size(), result);
    return result;
}

template<typename Ty>
Ty from_string(std::string_view s) {
    Ty result(string_converter<Ty>::default_value());
    string_converter<Ty>::from_string(s.data(), s.data() + s.size(), result);
    return result;
}

template<typename Ty>
Ty from_wstring(std::wstring_view s) {
    Ty result(wstring_converter<Ty>::default_value());
    wstring_converter<Ty>::from_string(s.data(), s.data() + s.size(), result);
    return result;
}

template<typename Ty, typename StrTy>
StrTy& to_string_append(const Ty& val, StrTy& s, const fmt_state& fmt) {
    return basic_string_converter<typename StrTy::value_type, Ty>::to_string(val, s, fmt);
}

template<typename Ty, typename... Args>
std::string to_string(const Ty& val, Args&&... args) {
    std::string result;
    to_string_append(val, result, fmt_state(std::forward<Args>(args)...));
    return result;
}

template<typename Ty, typename... Args>
std::wstring to_wstring(const Ty& val, Args&&... args) {
    std::wstring result;
    to_string_append(val, result, fmt_state(std::forward<Args>(args)...));
    return result;
}

template<typename Ty, typename... Args>
char* to_string_to(char* buf, const Ty& val, Args&&... args) {
    char_buf_appender appender(buf);
    return to_string_append(val, appender, fmt_state(std::forward<Args>(args)...)).get_ptr();
}

template<typename Ty, typename... Args>
wchar_t* to_wstring_to(wchar_t* buf, const Ty& val, Args&&... args) {
    wchar_buf_appender appender(buf);
    return to_string_append(val, appender, fmt_state(std::forward<Args>(args)...)).get_ptr();
}

template<typename Ty, typename... Args>
char* to_string_to_n(char* buf, size_t n, const Ty& val, Args&&... args) {
    char_n_buf_appender appender(buf, n);
    return to_string_append(val, appender, fmt_state(std::forward<Args>(args)...)).get_ptr();
}

template<typename Ty, typename... Args>
wchar_t* to_wstring_to_n(wchar_t* buf, size_t n, const Ty& val, Args&&... args) {
    wchar_n_buf_appender appender(buf, n);
    return to_string_append(val, appender, fmt_state(std::forward<Args>(args)...)).get_ptr();
}

}  // namespace util
