#pragma once

#include "chars.h"
#include "string_view.h"

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

template<typename CharT, typename Appender>
class basic_appender_mixin {
 public:
    using value_type = CharT;
    Appender& operator+=(std::basic_string_view<value_type> s) {
        return static_cast<Appender&>(*this).append(s.data(), s.data() + s.size());
    }
    Appender& operator+=(value_type ch) {
        static_cast<Appender&>(*this).push_back(ch);
        return static_cast<Appender&>(*this);
    }
};

template<typename CharT>
class basic_unlimbuf_appender : public basic_appender_mixin<CharT, basic_unlimbuf_appender<CharT>> {
 public:
    explicit basic_unlimbuf_appender(CharT* dst) : dst_(dst) {}
    CharT* curr() const { return dst_; }
    basic_unlimbuf_appender& append(const CharT* first, const CharT* last) {
        dst_ = std::copy(first, last, dst_);
        return *this;
    }
    basic_unlimbuf_appender& append(size_t count, CharT ch) {
        dst_ = std::fill_n(dst_, count, ch);
        return *this;
    }
    void push_back(CharT ch) { *dst_++ = ch; }

 private:
    CharT* dst_;
};

using unlimbuf_appender = basic_unlimbuf_appender<char>;
using wunlimbuf_appender = basic_unlimbuf_appender<wchar_t>;

template<typename CharT>
class basic_limbuf_appender : public basic_appender_mixin<CharT, basic_limbuf_appender<CharT>> {
 public:
    basic_limbuf_appender(CharT* dst, size_t n) : dst_(dst), last_(dst + n) {}
    CharT* curr() const { return dst_; }
    CharT* last() const { return last_; }
    basic_limbuf_appender& append(const CharT* first, const CharT* last) {
        dst_ = std::copy_n(first, std::min<size_t>(last - first, last_ - dst_), dst_);
        return *this;
    }
    basic_limbuf_appender& append(size_t count, CharT ch) {
        dst_ = std::fill_n(dst_, std::min<size_t>(count, last_ - dst_), ch);
        return *this;
    }
    void push_back(CharT ch) {
        if (dst_ != last_) { *dst_++ = ch; }
    }
    basic_limbuf_appender& setcurr(CharT* curr) {
        assert(dst_ <= curr && curr <= last_);
        dst_ = curr;
        return *this;
    }

 private:
    CharT *dst_, *last_;
};

using limbuf_appender = basic_limbuf_appender<char>;
using wlimbuf_appender = basic_limbuf_appender<wchar_t>;

template<typename CharT>
class basic_dynbuf_appender : public basic_appender_mixin<CharT, basic_dynbuf_appender<CharT>> {
 public:
    basic_dynbuf_appender() : first_(buf_), dst_(buf_), last_(&buf_[kInlineBufSize]) {}
    ~basic_dynbuf_appender();
    basic_dynbuf_appender(const basic_dynbuf_appender&) = delete;
    basic_dynbuf_appender& operator=(const basic_dynbuf_appender&) = delete;
    size_t size() const { return dst_ - first_; }
    const CharT* data() const { return first_; }
    const CharT* curr() const { return dst_; }
    basic_dynbuf_appender& append(const CharT* first, const CharT* last) {
        if (last_ - dst_ < last - first) { grow(last - first); }
        dst_ = std::copy(first, last, dst_);
        return *this;
    }
    basic_dynbuf_appender& append(size_t count, CharT ch) {
        if (static_cast<size_t>(last_ - dst_) < count) { grow(count); }
        dst_ = std::fill_n(dst_, count, ch);
        return *this;
    }
    void push_back(CharT ch) {
        if (last_ == dst_) { grow(1); }
        *dst_++ = ch;
    }
    CharT* reserve(size_t count) {
        if (static_cast<size_t>(last_ - dst_) < count) { grow(count); }
        return dst_;
    }
    basic_dynbuf_appender& setcurr(CharT* curr) {
        assert(dst_ <= curr && curr <= last_);
        dst_ = curr;
        return *this;
    }

 private:
    enum : unsigned {
#if defined(NDEBUG)
        kInlineBufSize = 256 / sizeof(CharT)
#else   // defined(NDEBUG)
        kInlineBufSize = 7 / sizeof(CharT)
#endif  // defined(NDEBUG)
    };
    CharT *first_, *dst_, *last_;
    CharT buf_[kInlineBufSize];

    void grow(size_t extra);
};

using dynbuf_appender = basic_dynbuf_appender<char>;
using wdynbuf_appender = basic_dynbuf_appender<wchar_t>;

template<typename Ty>
struct string_converter;

template<typename Ty>
struct string_converter_base {
    using is_string_converter = int;
    static Ty default_value() { return {}; }
};

#define SCVT_DECLARE_STANDARD_STRING_CONVERTER(ty) \
    template<> \
    struct UTIL_EXPORT string_converter<ty> : string_converter_base<ty> { \
        template<typename CharT> \
        static const CharT* from_string(const CharT* first, const CharT* last, ty& val); \
        template<typename StrTy> \
        static StrTy& to_string(StrTy& s, ty val, const fmt_state& fmt); \
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
SCVT_DECLARE_STANDARD_STRING_CONVERTER(char)
SCVT_DECLARE_STANDARD_STRING_CONVERTER(wchar_t)
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
    string_converter<Ty>::from_string(s.data(), s.data() + s.size(), result);
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
    Ty result(string_converter<Ty>::default_value());
    string_converter<Ty>::from_string(s.data(), s.data() + s.size(), result);
    return result;
}

template<typename Ty, typename StrTy>
StrTy& to_string_append(StrTy& s, const Ty& val, const fmt_state& fmt) {
    return string_converter<Ty>::to_string(s, val, fmt);
}

template<typename Ty, typename... Args>
std::string to_string(const Ty& val, Args&&... args) {
    util::dynbuf_appender appender;
    to_string_append(appender, val, fmt_state(std::forward<Args>(args)...));
    return std::string(appender.data(), appender.size());
}

template<typename Ty, typename... Args>
std::wstring to_wstring(const Ty& val, Args&&... args) {
    util::wdynbuf_appender appender;
    to_string_append(appender, val, fmt_state(std::forward<Args>(args)...));
    return std::wstring(appender.data(), appender.size());
}

template<typename Ty, typename... Args>
char* to_string_to(char* buf, const Ty& val, Args&&... args) {
    unlimbuf_appender appender(buf);
    return to_string_append(appender, val, fmt_state(std::forward<Args>(args)...)).curr();
}

template<typename Ty, typename... Args>
wchar_t* to_wstring_to(wchar_t* buf, const Ty& val, Args&&... args) {
    wunlimbuf_appender appender(buf);
    return to_string_append(appender, val, fmt_state(std::forward<Args>(args)...)).curr();
}

template<typename Ty, typename... Args>
char* to_string_to_n(char* buf, size_t n, const Ty& val, Args&&... args) {
    limbuf_appender appender(buf, n);
    return to_string_append(appender, val, fmt_state(std::forward<Args>(args)...)).curr();
}

template<typename Ty, typename... Args>
wchar_t* to_wstring_to_n(wchar_t* buf, size_t n, const Ty& val, Args&&... args) {
    wlimbuf_appender appender(buf, n);
    return to_string_append(appender, val, fmt_state(std::forward<Args>(args)...)).curr();
}

}  // namespace util
