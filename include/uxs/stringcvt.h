#pragma once

#include "chars.h"
#include "iterator.h"
#include "string_view.h"

#include <algorithm>
#include <memory>
#include <stdexcept>

namespace uxs {

template<typename InputIt, typename InputFn = nofunc>
unsigned from_hex(InputIt in, unsigned n_digs, InputFn fn = InputFn{}, unsigned* n_valid = nullptr) {
    unsigned val = 0;
    if (n_valid) { *n_valid = n_digs; }
    while (n_digs) {
        unsigned dig = dig_v(fn(*in));
        if (dig < 16) {
            val = (val << 4) | dig;
        } else {
            if (n_valid) { *n_valid -= n_digs; }
            return val;
        }
        ++in, --n_digs;
    }
    return val;
}

template<typename OutputIt, typename OutputFn = nofunc>
void to_hex(unsigned val, OutputIt out, unsigned n_digs, bool upper = false, OutputFn fn = OutputFn{}) {
    const char* digs = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    unsigned shift = n_digs << 2;
    while (shift) {
        shift -= 4;
        *out++ = fn(digs[(val >> shift) & 0xf]);
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
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(fmt_flags, unsigned);

struct fmt_opts {
    CONSTEXPR fmt_opts() = default;
    CONSTEXPR fmt_opts(fmt_flags fl) : flags(fl) {}
    CONSTEXPR fmt_opts(fmt_flags fl, int p) : flags(fl), prec(p) {}
    CONSTEXPR fmt_opts(fmt_flags fl, int p, unsigned w, int ch) : flags(fl), prec(p), width(w), fill(ch) {}
    fmt_flags flags = fmt_flags::kDec;
    int prec = -1;
    unsigned width = 0;
    int fill = ' ';
};

template<typename CharT, typename Appender>
class basic_appender_mixin {
 public:
    using value_type = CharT;
    template<typename Range, typename = std::void_t<decltype(std::declval<Range>().end())>>
    Appender& operator+=(const Range& r) {
        return static_cast<Appender&>(*this).append(r.begin(), r.end());
    }
    Appender& operator+=(const value_type* s) { return *this += std::basic_string_view<value_type>(s); }
    Appender& operator+=(value_type ch) {
        static_cast<Appender&>(*this).push_back(ch);
        return static_cast<Appender&>(*this);
    }
};

template<typename CharT>
class basic_unlimbuf_appender : public basic_appender_mixin<CharT, basic_unlimbuf_appender<CharT>> {
 public:
    explicit basic_unlimbuf_appender(CharT* dst) : curr_(dst) {}
    basic_unlimbuf_appender(const basic_unlimbuf_appender&) = delete;
    basic_unlimbuf_appender& operator=(const basic_unlimbuf_appender&) = delete;
    CharT& back() { return *(curr_ - 1); }
    CharT* curr() { return curr_; }
    basic_unlimbuf_appender& setcurr(CharT* curr) {
        curr_ = curr;
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_random_access_iterator<InputIt>::value>>
    basic_unlimbuf_appender& append(InputIt first, InputIt last) {
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
    basic_limbuf_appender(const basic_limbuf_appender&) = delete;
    basic_limbuf_appender& operator=(const basic_limbuf_appender&) = delete;
    CharT& back() { return *(curr_ - 1); }
    CharT* curr() { return curr_; }
    basic_limbuf_appender& setcurr(CharT* curr) {
        assert(curr <= last_);
        curr_ = curr;
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_random_access_iterator<InputIt>::value>>
    basic_limbuf_appender& append(InputIt first, InputIt last) {
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

template<typename Ty, typename Alloc = std::allocator<Ty>>
class basic_dynbuffer : protected std::allocator_traits<Alloc>::template rebind_alloc<Ty>,
                        public basic_appender_mixin<Ty, basic_dynbuffer<Ty>> {
 private:
    static_assert(std::is_trivially_copyable<Ty>::value && std::is_trivially_destructible<Ty>::value,
                  "uxs::basic_dynbuffer<> must have trivially copyable and destructible value type");

    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<Ty>;

 public:
    using value_type = Ty;

    basic_dynbuffer(Ty* first, Ty* last, bool is_inline)
        : alloc_type(), curr_(first), first_(first), last_(last), is_inline_(is_inline) {}
    basic_dynbuffer(Ty* first, Ty* last, bool is_inline, const Alloc& al)
        : alloc_type(al), curr_(first), first_(first), last_(last), is_inline_(is_inline) {}
    ~basic_dynbuffer() {
        if (!is_inline_) { this->deallocate(first_, last_ - first_); }
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
    Ty* first() { return first_; }
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

    template<typename InputIt, typename = std::enable_if_t<is_random_access_iterator<InputIt>::value>>
    basic_dynbuffer& append(InputIt first, InputIt last) {
        if (static_cast<size_t>(last_ - curr_) < static_cast<size_t>(last - first)) { grow(last - first); }
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
        --curr_;
    }

 private:
    Ty *curr_, *first_, *last_;
    bool is_inline_;

    void grow(size_t extra);
};

template<typename Ty, typename Alloc>
void basic_dynbuffer<Ty, Alloc>::grow(size_t extra) {
    size_t sz = curr_ - first_, delta_sz = std::max(extra, sz >> 1);
    const size_t max_avail = std::allocator_traits<alloc_type>::max_size(*this) - sz;
    if (delta_sz > max_avail) {
        if (extra > max_avail) { throw std::length_error("too much to reserve"); }
        delta_sz = std::max(extra, max_avail >> 1);
    }
    sz += delta_sz;
    Ty* first = this->allocate(sz);
    curr_ = std::uninitialized_copy(first_, curr_, first);
    if (!is_inline_) { this->deallocate(first_, last_ - first_); }
    first_ = first, last_ = first + sz, is_inline_ = false;
}

using dynbuffer = basic_dynbuffer<char>;
using wdynbuffer = basic_dynbuffer<wchar_t>;

template<typename Ty, size_t InlineBufSize = 0, typename Alloc = std::allocator<Ty>>
class basic_inline_dynbuffer : public basic_dynbuffer<Ty, Alloc> {
 public:
    basic_inline_dynbuffer()
        : basic_dynbuffer<Ty>(reinterpret_cast<Ty*>(&buf_), reinterpret_cast<Ty*>(&buf_[kInlineBufSize]), true) {}
    explicit basic_inline_dynbuffer(const Alloc& al)
        : basic_dynbuffer<Ty>(reinterpret_cast<Ty*>(&buf_), reinterpret_cast<Ty*>(&buf_[kInlineBufSize]), true, al) {}
    basic_dynbuffer<Ty>& base() { return *this; }

 private:
    enum : unsigned {
#if defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
        kInlineBufSize = InlineBufSize != 0 ? InlineBufSize : 256 / sizeof(Ty)
#else   // defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
        kInlineBufSize = 7
#endif  // defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
    };
    typename std::aligned_storage<sizeof(Ty), std::alignment_of<Ty>::value>::type buf_[kInlineBufSize];
};

using inline_dynbuffer = basic_inline_dynbuffer<char>;
using inline_wdynbuffer = basic_inline_dynbuffer<wchar_t>;

template<typename StrTy, typename Func>
StrTy& append_adjusted(StrTy& s, Func fn, unsigned len, const fmt_opts& fmt) {
    unsigned left = fmt.width - len, right = left;
    switch (fmt.flags & fmt_flags::kAdjustField) {
        case fmt_flags::kLeft: left = 0; break;
        case fmt_flags::kInternal: left >>= 1, right -= left; break;
        default: right = 0; break;
    }
    s.append(left, fmt.fill);
    return fn(s).append(right, fmt.fill);
}

template<typename Ty>
struct string_converter;

template<typename Ty>
struct string_converter_base {
    using is_string_converter = int;
    static Ty default_value() { return {}; }
};

#define UXS_SCVT_DECLARE_STANDARD_STRING_CONVERTER(ty) \
    template<> \
    struct UXS_EXPORT string_converter<ty> : string_converter_base<ty> { \
        template<typename CharT, typename Traits> \
        static size_t from_string(std::basic_string_view<CharT, Traits> s, ty& val); \
        template<typename StrTy> \
        static StrTy& to_string(StrTy& s, ty val, const fmt_opts& fmt); \
    };
UXS_SCVT_DECLARE_STANDARD_STRING_CONVERTER(int8_t)
UXS_SCVT_DECLARE_STANDARD_STRING_CONVERTER(int16_t)
UXS_SCVT_DECLARE_STANDARD_STRING_CONVERTER(int32_t)
UXS_SCVT_DECLARE_STANDARD_STRING_CONVERTER(int64_t)
UXS_SCVT_DECLARE_STANDARD_STRING_CONVERTER(uint8_t)
UXS_SCVT_DECLARE_STANDARD_STRING_CONVERTER(uint16_t)
UXS_SCVT_DECLARE_STANDARD_STRING_CONVERTER(uint32_t)
UXS_SCVT_DECLARE_STANDARD_STRING_CONVERTER(uint64_t)
UXS_SCVT_DECLARE_STANDARD_STRING_CONVERTER(float)
UXS_SCVT_DECLARE_STANDARD_STRING_CONVERTER(double)
UXS_SCVT_DECLARE_STANDARD_STRING_CONVERTER(char)
UXS_SCVT_DECLARE_STANDARD_STRING_CONVERTER(wchar_t)
UXS_SCVT_DECLARE_STANDARD_STRING_CONVERTER(bool)
#undef UXS_SCVT_DECLARE_STANDARD_STRING_CONVERTER

template<typename Ty, typename CharT, typename Traits>
NODISCARD Ty from_basic_string(std::basic_string_view<CharT, Traits> s) {
    Ty result(string_converter<Ty>::default_value());
    string_converter<Ty>::from_string(s, result);
    return result;
}

template<typename Ty>
NODISCARD Ty from_string(std::string_view s) {
    return from_basic_string<Ty>(s);
}

template<typename Ty>
NODISCARD Ty from_wstring(std::wstring_view s) {
    return from_basic_string<Ty>(s);
}

template<typename CharT, typename Traits, typename Ty>
size_t basic_stoval(std::basic_string_view<CharT, Traits> s, Ty& v) {
    return string_converter<Ty>::from_string(s, v);
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
StrTy& to_basic_string(StrTy& s, const Ty& val, const fmt_opts& fmt = fmt_opts{}) {
    return string_converter<Ty>::to_string(s, val, fmt);
}

template<typename Ty, typename... Args>
NODISCARD std::string to_string(const Ty& val, Args&&... args) {
    inline_dynbuffer buf;
    to_basic_string(buf.base(), val, fmt_opts{std::forward<Args>(args)...});
    return std::string(buf.data(), buf.size());
}

template<typename Ty, typename... Args>
NODISCARD std::wstring to_wstring(const Ty& val, Args&&... args) {
    inline_wdynbuffer buf;
    to_basic_string(buf.base(), val, fmt_opts{std::forward<Args>(args)...});
    return std::wstring(buf.data(), buf.size());
}

template<typename Ty, typename... Args>
char* to_chars(char* buf, const Ty& val, Args&&... args) {
    unlimbuf_appender appender(buf);
    return to_basic_string(appender, val, fmt_opts{std::forward<Args>(args)...}).curr();
}

template<typename Ty, typename... Args>
wchar_t* to_wchars(wchar_t* buf, const Ty& val, Args&&... args) {
    wunlimbuf_appender appender(buf);
    return to_basic_string(appender, val, fmt_opts{std::forward<Args>(args)...}).curr();
}

template<typename Ty, typename... Args>
char* to_chars_n(char* buf, size_t n, const Ty& val, Args&&... args) {
    limbuf_appender appender(buf, n);
    return to_basic_string(appender, val, fmt_opts{std::forward<Args>(args)...}).curr();
}

template<typename Ty, typename... Args>
wchar_t* to_wchars_n(wchar_t* buf, size_t n, const Ty& val, Args&&... args) {
    wlimbuf_appender appender(buf, n);
    return to_basic_string(appender, val, fmt_opts{std::forward<Args>(args)...}).curr();
}

}  // namespace uxs
