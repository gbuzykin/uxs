#pragma once

#include "string_view.h"
#include "utility.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace util {

template<unsigned base>
CONSTEXPR int dig(char ch) {
    return static_cast<int>(ch - '0');
}

template<>
CONSTEXPR int dig<16>(char ch) {
    if (ch >= 'a' && ch <= 'f') { return static_cast<int>(ch - 'a') + 10; }
    if (ch >= 'A' && ch <= 'F') { return static_cast<int>(ch - 'A') + 10; }
    return static_cast<int>(ch - '0');
}

template<typename InputIt, typename InputFn = nofunc>
unsigned from_hex(InputIt in, int digs, InputFn fn = InputFn{}, bool* ok = nullptr) {
    unsigned val = 0;
    while (digs > 0) {
        char ch = fn(*in++);
        val <<= 4;
        --digs;
        if (std::isxdigit(static_cast<unsigned char>(ch))) {
            val |= dig<16>(ch);
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

inline fmt_flags operator&(fmt_flags lhs, fmt_flags rhs) {
    return static_cast<fmt_flags>(static_cast<unsigned>(lhs) & static_cast<unsigned>(rhs));
}
inline fmt_flags operator|(fmt_flags lhs, fmt_flags rhs) {
    return static_cast<fmt_flags>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs));
}
inline fmt_flags operator~(fmt_flags flags) { return static_cast<fmt_flags>(~static_cast<unsigned>(flags)); }
inline bool operator!(fmt_flags flags) { return static_cast<unsigned>(flags) == 0; }
inline fmt_flags& operator&=(fmt_flags& lhs, fmt_flags rhs) { return lhs = lhs & rhs; }
inline fmt_flags& operator|=(fmt_flags& lhs, fmt_flags rhs) { return lhs = lhs | rhs; }

struct fmt_state {
    CONSTEXPR fmt_state() = default;
    CONSTEXPR fmt_state(fmt_flags fl) : flags(fl) {}
    CONSTEXPR fmt_state(fmt_flags fl, int p) : flags(fl), prec(p) {}
    CONSTEXPR fmt_state(fmt_flags fl, int p, unsigned w, char ch) : flags(fl), prec(p), width(w), fill(ch) {}
    fmt_flags flags = fmt_flags::kDec;
    int prec = -1;
    unsigned width = 0;
    char fill = ' ';
};

class char_buf_appender {
 public:
    explicit char_buf_appender(char* dst) : dst_(dst) {}
    char* get_ptr() const { return dst_; }
    template<typename InputIt,
             typename = std::enable_if_t<std::is_base_of<
                 std::input_iterator_tag, typename std::iterator_traits<InputIt>::iterator_category>::value>>
    char_buf_appender& append(InputIt first, InputIt last) {
        dst_ = std::copy(first, last, dst_);
        return *this;
    }
    char_buf_appender& append(size_t count, char ch) {
        dst_ = std::fill_n(dst_, count, ch);
        return *this;
    }
    char_buf_appender& operator+=(char ch) {
        *dst_++ = ch;
        return *this;
    }
    char_buf_appender& operator+=(const char* cstr) {
        dst_ = std::copy_n(cstr, std::strlen(cstr), dst_);
        return *this;
    }
    char_buf_appender& operator+=(std::string_view s) {
        dst_ = std::copy_n(s.data(), s.size(), dst_);
        return *this;
    }

 private:
    char* dst_;
};

class char_n_buf_appender {
 public:
    char_n_buf_appender(char* dst, size_t n) : dst_(dst), last_(dst + n) {}
    char* get_ptr() const { return dst_; }
    template<typename InputIt,
             typename = std::enable_if_t<std::is_base_of<
                 std::random_access_iterator_tag, typename std::iterator_traits<InputIt>::iterator_category>::value>>
    char_n_buf_appender& append(InputIt first, InputIt last) {
        dst_ = std::copy_n(first, std::min<size_t>(last - first, last_ - dst_), dst_);
        return *this;
    }
    char_n_buf_appender& append(size_t count, char ch) {
        dst_ = std::fill_n(dst_, std::min<size_t>(count, last_ - dst_), ch);
        return *this;
    }
    char_n_buf_appender& operator+=(char ch) {
        if (dst_ != last_) { *dst_++ = ch; }
        return *this;
    }
    char_n_buf_appender& operator+=(const char* cstr) {
        dst_ = std::copy_n(cstr, std::min<size_t>(std::strlen(cstr), last_ - dst_), dst_);
        return *this;
    }
    char_n_buf_appender& operator+=(std::string_view s) {
        dst_ = std::copy_n(s.data(), std::min<size_t>(s.size(), last_ - dst_), dst_);
        return *this;
    }

 private:
    char* dst_;
    char* last_;
};

namespace detail {
template<typename InputIt, typename StrTy,
         typename = std::enable_if_t<std::is_base_of<std::random_access_iterator_tag,
                                                     typename std::iterator_traits<InputIt>::iterator_category>::value>>
StrTy& fmt_adjusted(InputIt first, InputIt last, StrTy& s, const fmt_state& fmt) {
    unsigned len = static_cast<unsigned>(last - first);
    switch (fmt.flags & fmt_flags::kAdjustField) {
        case fmt_flags::kLeft: {
            s.append(first, last).append(fmt.width - len, fmt.fill);
        } break;
        case fmt_flags::kInternal: {
            unsigned right = fmt.width - len, left = right >> 1;
            right -= left;
            s.append(left, fmt.fill).append(first, last).append(right, fmt.fill);
        } break;
        default: {
            s.append(fmt.width - len, fmt.fill).append(first, last);
        } break;
    }
    return s;
}
}  // namespace detail

template<typename Ty>
struct string_converter;

template<typename Ty>
struct string_converter_base {
    using is_string_converter = int;
    static Ty default_value() { return {}; }
};

#define SCVT_DECLARE_STANDARD_STRING_CONVERTERS(ty) \
    template<> \
    struct UTIL_EXPORT string_converter<ty> : string_converter_base<ty> { \
        static const char* from_string(const char* first, const char* last, ty& val); \
        static std::string& to_string(ty val, std::string& s, const fmt_state& fmt); \
        static char_buf_appender& to_string(ty val, char_buf_appender& s, const fmt_state& fmt); \
        static char_n_buf_appender& to_string(ty val, char_n_buf_appender& s, const fmt_state& fmt); \
    };

SCVT_DECLARE_STANDARD_STRING_CONVERTERS(int8_t)
SCVT_DECLARE_STANDARD_STRING_CONVERTERS(int16_t)
SCVT_DECLARE_STANDARD_STRING_CONVERTERS(int32_t)
SCVT_DECLARE_STANDARD_STRING_CONVERTERS(int64_t)
SCVT_DECLARE_STANDARD_STRING_CONVERTERS(uint8_t)
SCVT_DECLARE_STANDARD_STRING_CONVERTERS(uint16_t)
SCVT_DECLARE_STANDARD_STRING_CONVERTERS(uint32_t)
SCVT_DECLARE_STANDARD_STRING_CONVERTERS(uint64_t)
SCVT_DECLARE_STANDARD_STRING_CONVERTERS(float)
SCVT_DECLARE_STANDARD_STRING_CONVERTERS(double)
#undef SCVT_DECLARE_STANDARD_STRING_CONVERTERS

template<>
struct UTIL_EXPORT string_converter<char> : string_converter_base<char> {
    static const char* from_string(const char* first, const char* last, char& val);
    template<typename StrTy>
    static StrTy& to_string(char val, StrTy& s, const fmt_state& fmt) {
        if (fmt.width > 1) { return detail::fmt_adjusted(&val, &val + 1, s, fmt); }
        s += val;
        return s;
    }
};

template<>
struct UTIL_EXPORT string_converter<bool> : string_converter_base<bool> {
    static const char* from_string(const char* first, const char* last, bool& val);
    template<typename StrTy>
    static StrTy& to_string(bool val, StrTy& s, const fmt_state& fmt) {
        std::string_view sval = !(fmt.flags & fmt_flags::kUpperCase) ?
                                    (val ? std::string_view("true", 4) : std::string_view("false", 5)) :
                                    (val ? std::string_view("TRUE", 4) : std::string_view("FALSE", 5));
        if (sval.size() < fmt.width) { return detail::fmt_adjusted(sval.begin(), sval.end(), s, fmt); }
        return s.append(sval.begin(), sval.end());
    }
};

template<typename Ty, typename Def>
Ty from_string(std::string_view s, Def&& def) {
    Ty result(std::forward<Def>(def));
    string_converter<Ty>::from_string(s.data(), s.data() + s.size(), result);
    return result;
}

template<typename Ty>
Ty from_string(std::string_view s) {
    Ty result(string_converter<Ty>::default_value());
    string_converter<Ty>::from_string(s.data(), s.data() + s.size(), result);
    return result;
}

template<typename Ty, typename StrTy>
StrTy& to_string_append(const Ty& val, StrTy& s, const fmt_state& fmt) {
    return string_converter<Ty>::to_string(val, s, fmt);
}

template<typename Ty, typename... Args>
std::string to_string(const Ty& val, Args&&... args) {
    std::string result;
    to_string_append(val, result, fmt_state(std::forward<Args>(args)...));
    return result;
}

template<typename Ty, typename... Args>
char* to_string_to(char* buf, const Ty& val, Args&&... args) {
    char_buf_appender appender(buf);
    return to_string_append(val, appender, fmt_state(std::forward<Args>(args)...)).get_ptr();
}

template<typename Ty, typename... Args>
char* to_string_to_n(char* buf, size_t n, const Ty& val, Args&&... args) {
    char_n_buf_appender appender(buf, n);
    return to_string_append(val, appender, fmt_state(std::forward<Args>(args)...)).get_ptr();
}

}  // namespace util
