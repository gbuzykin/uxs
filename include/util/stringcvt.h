#pragma once

#include "chars.h"
#include "string_view.h"

#include <algorithm>
#include <memory>
#include <stdexcept>

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
    kAlternate = 0x100,
    kSignNeg = kDefault,
    kSignPos = 0x200,
    kSignAlign = 0x400,
    kSignField = 0x600,
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
    explicit basic_unlimbuf_appender(CharT* dst) : curr_(dst) {}
    CharT* curr() { return curr_; }
    template<typename CharT_>
    basic_unlimbuf_appender& append(const CharT_* first, const CharT_* last) {
        curr_ = std::copy(first, last, curr_);
        return *this;
    }
    basic_unlimbuf_appender& append(size_t count, CharT ch) {
        curr_ = std::fill_n(curr_, count, ch);
        return *this;
    }
    void push_back(CharT ch) { *curr_++ = ch; }

 private:
    CharT* curr_;
};

using unlimbuf_appender = basic_unlimbuf_appender<char>;
using wunlimbuf_appender = basic_unlimbuf_appender<wchar_t>;

template<typename CharT>
class basic_limbuf_appender : public basic_appender_mixin<CharT, basic_limbuf_appender<CharT>> {
 public:
    basic_limbuf_appender(CharT* dst, size_t n) : curr_(dst), last_(dst + n) {}
    CharT* curr() { return curr_; }
    template<typename CharT_>
    basic_limbuf_appender& append(const CharT_* first, const CharT_* last) {
        curr_ = std::copy_n(first, std::min<size_t>(last - first, last_ - curr_), curr_);
        return *this;
    }
    basic_limbuf_appender& append(size_t count, CharT ch) {
        curr_ = std::fill_n(curr_, std::min<size_t>(count, last_ - curr_), ch);
        return *this;
    }
    void push_back(CharT ch) {
        if (curr_ != last_) { *curr_++ = ch; }
    }

 private:
    CharT *curr_, *last_;
};

using limbuf_appender = basic_limbuf_appender<char>;
using wlimbuf_appender = basic_limbuf_appender<wchar_t>;

template<typename Ty, size_t InlineBufSize = 0>
class basic_dynbuffer : public basic_appender_mixin<Ty, basic_dynbuffer<Ty>> {
    static_assert(std::is_trivially_copyable<Ty>::value && std::is_trivially_destructible<Ty>::value,
                  "util::basic_dynbuffer must have trivially copyable and destructible value type");

 public:
    basic_dynbuffer()
        : first_(reinterpret_cast<Ty*>(&buf_)), curr_(reinterpret_cast<Ty*>(&buf_)),
          last_(reinterpret_cast<Ty*>(&buf_[kInlineBufSize])) {}
    ~basic_dynbuffer() {
        if (first_ != reinterpret_cast<Ty*>(&buf_)) { std::allocator<Ty>().deallocate(first_, last_ - first_); }
    }
    basic_dynbuffer(const basic_dynbuffer&) = delete;
    basic_dynbuffer& operator=(const basic_dynbuffer&) = delete;
    bool empty() const { return first_ == curr_; }
    size_t size() const { return curr_ - first_; }
    size_t avail() const { return last_ - curr_; }
    const Ty* data() const { return first_; }
    Ty& back() {
        assert(first_ < curr_);
        return *(curr_ - 1);
    }
    Ty* curr() { return curr_; }
    Ty** p_curr() { return &curr_; }
    basic_dynbuffer& setcurr(Ty* curr) {
        assert(first_ <= curr && curr <= last_);
        curr_ = curr;
        return *this;
    }
    Ty* last() { return last_; }
    void clear() { curr_ = first_; }

    Ty* reserve_at_curr(size_t count) {
        if (static_cast<size_t>(last_ - curr_) < count) { grow(count); }
        return curr_;
    }

    template<typename Ty_>
    basic_dynbuffer& append(const Ty_* first, const Ty_* last) {
        if (last_ - curr_ < last - first) { grow(last - first); }
        curr_ = std::uninitialized_copy(first, last, curr_);
        return *this;
    }
    basic_dynbuffer& append(size_t count, Ty val) {
        if (static_cast<size_t>(last_ - curr_) < count) { grow(count); }
        curr_ = std::uninitialized_fill_n(curr_, count, val);
        return *this;
    }
    void push_back(Ty val) {
        if (curr_ == last_) { grow(1); }
        new (curr_) Ty(val);
        ++curr_;
    }
    template<typename... Args>
    void emplace_back(Args&&... args) {
        if (curr_ == last_) { grow(1); }
        new (curr_) Ty(std::forward<Args>(args)...);
        ++curr_;
    }
    void pop_back() {
        assert(first_ < curr_);
        (--curr_)->~Ty();
    }

 private:
    enum : unsigned {
#if defined(NDEBUG)
        kInlineBufSize = InlineBufSize != 0 ? InlineBufSize : 256 / sizeof(Ty)
#else   // defined(NDEBUG)
        kInlineBufSize = 7
#endif  // defined(NDEBUG)
    };
    Ty *first_, *curr_, *last_;
    typename std::aligned_storage<sizeof(Ty), std::alignment_of<Ty>::value>::type buf_[kInlineBufSize];

    void grow(size_t extra);
};

template<typename Ty, size_t InlineBufSize>
void basic_dynbuffer<Ty, InlineBufSize>::grow(size_t extra) {
    size_t sz = curr_ - first_, delta_sz = std::max(extra, sz >> 1);
    using alloc_traits = std::allocator_traits<std::allocator<Ty>>;
    if (delta_sz > alloc_traits::max_size(std::allocator<Ty>()) - sz) {
        if (extra > alloc_traits::max_size(std::allocator<Ty>()) - sz) {
            throw std::length_error("too much to reserve");
        }
        delta_sz = std::max(extra, (alloc_traits::max_size(std::allocator<Ty>()) - sz) >> 1);
    }
    sz += delta_sz;
    Ty* first = std::allocator<Ty>().allocate(sz);
    curr_ = std::uninitialized_copy(first_, curr_, first);
    if (first_ != reinterpret_cast<Ty*>(buf_)) { std::allocator<Ty>().deallocate(first_, last_ - first_); }
    first_ = first, last_ = first + sz;
}

using dynbuffer = basic_dynbuffer<char, 256>;
using wdynbuffer = basic_dynbuffer<wchar_t, 256 / sizeof(wchar_t)>;

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
        static size_t from_string(std::basic_string_view<CharT> s, ty& val); \
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
    string_converter<Ty>::from_string(s, result);
    return result;
}

template<typename Ty, typename Def>
Ty from_wstring(std::wstring_view s, Def&& def) {
    Ty result(std::forward<Def>(def));
    string_converter<Ty>::from_string(s, result);
    return result;
}

template<typename Ty>
Ty from_string(std::string_view s) {
    Ty result(string_converter<Ty>::default_value());
    string_converter<Ty>::from_string(s, result);
    return result;
}

template<typename Ty>
Ty from_wstring(std::wstring_view s) {
    Ty result(string_converter<Ty>::default_value());
    string_converter<Ty>::from_string(s, result);
    return result;
}

template<typename Ty>
size_t stoval(std::string_view s, Ty& v) {
    return string_converter<Ty>::from_string(s, v);
}

template<typename Ty>
size_t wstoval(std::wstring_view s, Ty& v) {
    return string_converter<Ty>::from_string(s, v);
}

template<typename StrTy, typename Ty>
StrTy& basic_to_string(StrTy& s, const Ty& val, const fmt_state& fmt) {
    return string_converter<Ty>::to_string(s, val, fmt);
}

template<typename Ty, typename... Args>
std::string to_string(const Ty& val, Args&&... args) {
    dynbuffer buf;
    basic_to_string(buf, val, fmt_state(std::forward<Args>(args)...));
    return std::string(buf.data(), buf.size());
}

template<typename Ty, typename... Args>
std::wstring to_wstring(const Ty& val, Args&&... args) {
    wdynbuffer buf;
    basic_to_string(buf, val, fmt_state(std::forward<Args>(args)...));
    return std::wstring(buf.data(), buf.size());
}

template<typename Ty, typename... Args>
char* to_chars(char* buf, const Ty& val, Args&&... args) {
    unlimbuf_appender appender(buf);
    return basic_to_string(appender, val, fmt_state(std::forward<Args>(args)...)).curr();
}

template<typename Ty, typename... Args>
wchar_t* to_wchars(wchar_t* buf, const Ty& val, Args&&... args) {
    wunlimbuf_appender appender(buf);
    return basic_to_string(appender, val, fmt_state(std::forward<Args>(args)...)).curr();
}

template<typename Ty, typename... Args>
char* to_chars_n(char* buf, size_t n, const Ty& val, Args&&... args) {
    limbuf_appender appender(buf, n);
    return basic_to_string(appender, val, fmt_state(std::forward<Args>(args)...)).curr();
}

template<typename Ty, typename... Args>
wchar_t* to_wchars_n(wchar_t* buf, size_t n, const Ty& val, Args&&... args) {
    wlimbuf_appender appender(buf, n);
    return basic_to_string(appender, val, fmt_state(std::forward<Args>(args)...)).curr();
}

}  // namespace util
