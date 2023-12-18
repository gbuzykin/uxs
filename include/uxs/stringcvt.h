#pragma once

#include "chars.h"
#include "iterator.h"
#include "string_view.h"

#include <algorithm>
#include <locale>
#include <memory>
#include <stdexcept>
#include <string>

namespace uxs {

// --------------------------

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

// --------------------------

enum class fmt_flags : unsigned {
    none = 0,
    dec = none,
    bin = 1,
    oct = 2,
    hex = 3,
    base_field = 3,
    fixed = 4,
    scientific = 8,
    general = 0xc,
    float_field = 0xc,
    left = 0x10,
    right = 0x20,
    internal = 0x30,
    adjust_field = 0x30,
    leading_zeroes = 0x40,
    uppercase = 0x80,
    alternate = 0x100,
    json_compat = 0x200,
    sign_neg = none,
    sign_pos = 0x400,
    sign_align = 0x800,
    sign_field = 0xc00,
};
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(fmt_flags, unsigned);

struct fmt_opts {
    CONSTEXPR fmt_opts() = default;
    CONSTEXPR fmt_opts(fmt_flags fl) : flags(fl) {}
    CONSTEXPR fmt_opts(fmt_flags fl, int p) : flags(fl), prec(p) {}
    CONSTEXPR fmt_opts(fmt_flags fl, int p, const std::locale* l) : flags(fl), prec(p), loc(l) {}
    CONSTEXPR fmt_opts(fmt_flags fl, int p, const std::locale* l, unsigned w, int ch)
        : flags(fl), prec(p), loc(l), width(w), fill(ch) {}
    fmt_flags flags = fmt_flags::dec;
    int prec = -1;
    const std::locale* loc = nullptr;
    unsigned width = 0;
    int fill = ' ';
};

// --------------------------

template<typename Ty>
class basic_membuffer {
 private:
    static_assert(std::is_trivially_copyable<Ty>::value && std::is_trivially_destructible<Ty>::value,
                  "uxs::basic_membuffer<> must have trivially copyable and destructible value type");

 public:
    using value_type = Ty;

    explicit basic_membuffer(Ty* first, Ty* last = reinterpret_cast<Ty*>(std::numeric_limits<uintptr_t>::max())) NOEXCEPT
        : curr_(first),
          last_(last) {}
    virtual ~basic_membuffer() = default;
    basic_membuffer(const basic_membuffer&) = delete;
    basic_membuffer& operator=(const basic_membuffer&) = delete;

    size_t avail() const NOEXCEPT { return last_ - curr_; }
    const Ty* curr() const NOEXCEPT { return curr_; }
    Ty* curr() NOEXCEPT { return curr_; }
    Ty** p_curr() NOEXCEPT { return &curr_; }
    const Ty* last() const NOEXCEPT { return last_; }
    Ty* last() NOEXCEPT { return last_; }
    Ty& back() NOEXCEPT { return *(curr_ - 1); }

    basic_membuffer& advance(size_t n) NOEXCEPT {
        assert(n <= avail());
        curr_ += n;
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_random_access_iterator<InputIt>::value>>
    basic_membuffer& append_by_chunks(InputIt first, InputIt last) {
        size_t count = last - first, n_avail = avail();
        while (count > n_avail) {
            curr_ = std::copy_n(first, n_avail, curr_);
            if (!try_grow()) { return *this; }
            first += n_avail, count -= n_avail;
            n_avail = avail();
        }
        curr_ = std::copy(first, last, curr_);
        return *this;
    }

    basic_membuffer& append_by_chunks(size_t count, Ty val) {
        size_t n_avail = avail();
        while (count > n_avail) {
            curr_ = std::fill_n(curr_, n_avail, val);
            if (!try_grow()) { return *this; }
            count -= n_avail;
            n_avail = avail();
        }
        curr_ = std::fill_n(curr_, count, val);
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_random_access_iterator<InputIt>::value>>
    basic_membuffer& append(InputIt first, InputIt last) {
        const size_t count = last - first;
        if (avail() >= count || try_grow(count)) {
            curr_ = std::copy(first, last, curr_);
            return *this;
        }
        return append_by_chunks(first, last);
    }

    basic_membuffer& append(size_t count, Ty val) {
        if (avail() >= count || try_grow(count)) {
            curr_ = std::fill_n(curr_, count, val);
            return *this;
        }
        return append_by_chunks(count, val);
    }

    template<typename... Args>
    void emplace_back(Args&&... args) {
        if (curr_ != last_ || try_grow()) { new (curr_++) Ty(std::forward<Args>(args)...); }
    }
    void push_back(Ty val) {
        if (curr_ != last_ || try_grow()) { *curr_++ = val; }
    }
    void pop_back() { --curr_; }

    template<typename Range, typename = std::void_t<decltype(std::declval<Range>().end())>>
    basic_membuffer& operator+=(const Range& r) {
        return append(r.begin(), r.end());
    }
    basic_membuffer& operator+=(const value_type* s) { return *this += std::basic_string_view<value_type>(s); }
    basic_membuffer& operator+=(value_type ch) {
        push_back(ch);
        return *this;
    }

    virtual bool try_grow(size_t extra = 1) { return false; }

 protected:
    void set(Ty* curr) NOEXCEPT { curr_ = curr; }
    void set(Ty* curr, Ty* last) NOEXCEPT { curr_ = curr, last_ = last; }

 private:
    Ty* curr_;
    Ty* last_;
};

using membuffer = basic_membuffer<char>;
using wmembuffer = basic_membuffer<wchar_t>;

template<typename Ty, typename Alloc>
class basic_dynbuffer : protected std::allocator_traits<Alloc>::template rebind_alloc<Ty>, public basic_membuffer<Ty> {
 private:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<Ty>;

 public:
    using value_type = typename basic_membuffer<Ty>::value_type;

    ~basic_dynbuffer() override {
        if (is_allocated_) { this->deallocate(first_, capacity()); }
    }

    bool empty() const NOEXCEPT { return first_ == this->curr(); }
    size_t size() const NOEXCEPT { return this->curr() - first_; }
    size_t capacity() const NOEXCEPT { return this->last() - first_; }
    const Ty* data() const NOEXCEPT { return first_; }
    Ty* data() NOEXCEPT { return first_; }
    void clear() NOEXCEPT { this->set(first_); }

    void reserve(size_t extra = 1) {
        if (extra > this->avail()) { try_grow(extra); }
    }

    bool try_grow(size_t extra) override {
        size_t sz = size(), cap = capacity(), delta_sz = std::max(extra, sz >> 1);
        const size_t max_avail = std::allocator_traits<alloc_type>::max_size(*this) - sz;
        if (delta_sz > max_avail) {
            if (extra > max_avail) { throw std::length_error("too much to reserve"); }
            delta_sz = std::max(extra, max_avail >> 1);
        }
        sz += delta_sz;
        Ty* first = this->allocate(sz);
        this->set(std::copy(first_, this->curr(), first), first + sz);
        if (is_allocated_) { this->deallocate(first_, cap); }
        first_ = first, is_allocated_ = true;
        return true;
    }

 protected:
    basic_dynbuffer(Ty* first, Ty* last) NOEXCEPT : basic_membuffer<Ty>(first, last),
                                                    first_(first),
                                                    is_allocated_(false) {}

 private:
    Ty* first_;
    bool is_allocated_;
};

template<typename Ty, size_t InlineBufSize = 0, typename Alloc = std::allocator<Ty>>
class inline_basic_dynbuffer final : public basic_dynbuffer<Ty, Alloc> {
 public:
    inline_basic_dynbuffer()
        : basic_dynbuffer<Ty, Alloc>(reinterpret_cast<Ty*>(buf_), reinterpret_cast<Ty*>(&buf_[inline_buf_size])) {}

 private:
    enum : unsigned {
#if defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
        inline_buf_size = InlineBufSize != 0 ? InlineBufSize : 256 / sizeof(Ty)
#else   // defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
        inline_buf_size = 7
#endif  // defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
    };
    typename std::aligned_storage<sizeof(Ty), std::alignment_of<Ty>::value>::type buf_[inline_buf_size];
};

using inline_dynbuffer = inline_basic_dynbuffer<char>;
using inline_wdynbuffer = inline_basic_dynbuffer<wchar_t>;

// --------------------------

template<typename StrTy, typename Func>
void append_adjusted(StrTy& s, Func fn, unsigned len, const fmt_opts& fmt) {
    unsigned left = fmt.width - len, right = left;
    if (!(fmt.flags & fmt_flags::leading_zeroes)) {
        switch (fmt.flags & fmt_flags::adjust_field) {
            case fmt_flags::right: right = 0; break;
            case fmt_flags::internal: left >>= 1, right -= left; break;
            case fmt_flags::left:
            default: left = 0; break;
        }
    } else {
        right = 0;
    }
    s.append(left, fmt.fill);
    fn(s);
    s.append(right, fmt.fill);
}

// --------------------------

namespace scvt {

template<typename Ty>
struct fp_traits;

template<>
struct fp_traits<double> {
    static_assert(sizeof(double) == sizeof(uint64_t), "type size mismatch");
    enum : unsigned { total_bits = 64, bits_per_mantissa = 52 };
    enum : uint64_t { mantissa_mask = (1ull << bits_per_mantissa) - 1 };
    enum : int { exp_max = (1 << (total_bits - bits_per_mantissa - 1)) - 1 };
    static uint64_t to_u64(const double& f) { return *reinterpret_cast<const uint64_t*>(&f); }
    static double from_u64(const uint64_t& u64) { return *reinterpret_cast<const double*>(&u64); }
};

template<>
struct fp_traits<float> {
    static_assert(sizeof(float) == sizeof(uint32_t), "type size mismatch");
    enum : unsigned { total_bits = 32, bits_per_mantissa = 23 };
    enum : uint64_t { mantissa_mask = (1ull << bits_per_mantissa) - 1 };
    enum : int { exp_max = (1 << (total_bits - bits_per_mantissa - 1)) - 1 };
    static uint64_t to_u64(const float& f) { return *reinterpret_cast<const uint32_t*>(&f); }
    static float from_u64(const uint64_t& u64) { return *reinterpret_cast<const float*>(&u64); }
};

extern UXS_EXPORT const fmt_opts g_default_opts;

// --------------------------

template<typename Ty, typename = void>
struct reduce_type {
    using type = std::remove_cv_t<Ty>;
};
template<typename Ty>
struct reduce_type<Ty, std::enable_if_t<std::is_integral<Ty>::value && std::is_unsigned<Ty>::value &&
                                        !is_boolean<Ty>::value && !is_character<Ty>::value>> {
    using type = std::conditional_t<(sizeof(Ty) <= sizeof(uint32_t)), uint32_t, uint64_t>;
};
template<typename Ty>
struct reduce_type<Ty, std::enable_if_t<std::is_integral<Ty>::value && std::is_signed<Ty>::value &&
                                        !is_boolean<Ty>::value && !is_character<Ty>::value>> {
    using type = std::conditional_t<(sizeof(Ty) <= sizeof(int32_t)), int32_t, int64_t>;
};
template<typename Ty>
struct reduce_type<Ty, std::enable_if_t<std::is_array<Ty>::value>> {
    using type = typename std::add_pointer<std::remove_cv_t<typename std::remove_extent<Ty>::type>>::type;
};

template<typename Ty>
struct reduce_type<Ty*, void> {
    using type = typename std::add_pointer<std::remove_cv_t<Ty>>::type;
};

template<>
struct reduce_type<std::nullptr_t, void> {
    using type = void*;
};

template<typename Ty>
using reduce_type_t = typename reduce_type<Ty>::type;

// --------------------------

template<typename Ty, typename CharT>
UXS_EXPORT Ty to_boolean(const CharT* p, const CharT* end, const CharT*& last) NOEXCEPT;

template<typename Ty, typename CharT>
Ty to_character(const CharT* p, const CharT* end, const CharT*& last) NOEXCEPT {
    last = p;
    if (p == end) { return '\0'; }
    ++last;
    return *p;
}

template<typename Ty, typename CharT>
UXS_EXPORT Ty to_integral_common(const CharT* p, const CharT* end, const CharT*& last, Ty pos_limit) NOEXCEPT;

template<typename CharT>
UXS_EXPORT uint64_t to_float_common(const CharT* p, const CharT* end, const CharT*& last, const unsigned bpm,
                                    const int exp_max) NOEXCEPT;

template<typename Ty, typename CharT>
Ty to_integer(const CharT* p, const CharT* end, const CharT*& last) NOEXCEPT {
    using UTy = typename std::make_unsigned<Ty>::type;
    return static_cast<Ty>(to_integral_common<reduce_type_t<UTy>>(p, end, last, std::numeric_limits<UTy>::max()));
}

template<typename Ty, typename CharT>
Ty to_float(const CharT* p, const CharT* end, const CharT*& last) NOEXCEPT {
    using FpTy = std::conditional_t<(sizeof(Ty) <= sizeof(double)), Ty, double>;
    return static_cast<Ty>(fp_traits<FpTy>::from_u64(
        to_float_common(p, end, last, fp_traits<FpTy>::bits_per_mantissa, fp_traits<FpTy>::exp_max)));
}

// --------------------------

template<typename CharT, typename Ty>
UXS_EXPORT void fmt_boolean(basic_membuffer<CharT>& s, Ty val, const fmt_opts& fmt);

template<typename CharT, typename Ty>
UXS_EXPORT void fmt_character(basic_membuffer<CharT>& s, Ty val, const fmt_opts& fmt);

template<typename CharT, typename Ty>
UXS_EXPORT void fmt_integral_common(basic_membuffer<CharT>& s, Ty val, const fmt_opts& fmt);

template<typename CharT>
UXS_EXPORT void fmt_float_common(basic_membuffer<CharT>& s, uint64_t u64, const fmt_opts& fmt, const unsigned bpm,
                                 const int exp_max);

template<typename CharT, typename Ty>
void fmt_integer(basic_membuffer<CharT>& s, Ty val, const fmt_opts& fmt) {
    fmt_integral_common(s, static_cast<reduce_type_t<Ty>>(val), fmt);
}

template<typename CharT, typename Ty>
void fmt_float(basic_membuffer<CharT>& s, Ty val, const fmt_opts& fmt) {
    using FpTy = std::conditional_t<(sizeof(Ty) <= sizeof(double)), Ty, double>;
    fmt_float_common(s, fp_traits<Ty>::to_u64(static_cast<FpTy>(val)), fmt, fp_traits<Ty>::bits_per_mantissa,
                     fp_traits<Ty>::exp_max);
}

}  // namespace scvt

// --------------------------

template<typename Ty, typename CharT>
struct string_parser;

template<typename Ty, typename CharT>
struct formatter;

namespace detail {
template<typename Ty, typename CharT>
struct has_string_parser {
    template<typename U>
    static auto test(const U* first, const U* last, Ty& v)
        -> std::is_same<decltype(string_parser<Ty, U>().from_chars(first, last, v)), const U*>;
    template<typename U>
    static auto test(...) -> std::false_type;
    using type = decltype(test<CharT>(nullptr, nullptr, std::declval<Ty&>()));
};
template<typename Ty, typename StrTy>
struct has_formatter {
    template<typename U>
    static auto test(U& s, const Ty& v)
        -> always_true<decltype(formatter<Ty, typename U::value_type>().format(s, v, fmt_opts{}))>;
    template<typename U>
    static auto test(...) -> std::false_type;
    using type = decltype(test<StrTy>(std::declval<StrTy&>(), std::declval<Ty>()));
};
}  // namespace detail

template<typename Ty, typename CharT = char>
struct has_string_parser : detail::has_string_parser<Ty, CharT>::type {};

template<typename Ty, typename StrTy = membuffer>
struct has_formatter : detail::has_formatter<Ty, StrTy>::type {};

#define UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(ty, from_chars_func, fmt_func) \
    template<typename CharT> \
    struct string_parser<ty, CharT> { \
        const CharT* from_chars(const CharT* first, const CharT* last, ty& val) const NOEXCEPT { \
            auto t = from_chars_func<ty>(first, last, last); \
            if (last != first) { val = t; } \
            return last; \
        } \
    }; \
    template<typename CharT> \
    struct formatter<ty, CharT> { \
        template<typename StrTy, typename = std::enable_if_t<!std::is_base_of<basic_membuffer<CharT>, StrTy>::value>> \
        void format(StrTy& s, ty val, const fmt_opts& fmt) const { \
            inline_basic_dynbuffer<CharT> buf; \
            format(buf, val, fmt); \
            s.append(buf.data(), buf.data() + buf.size()); \
        } \
        void format(basic_membuffer<CharT>& s, ty val, const fmt_opts& fmt) const { \
            fmt_func<CharT, ty>(s, val, fmt); \
        } \
    };

UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(bool, scvt::to_boolean, scvt::fmt_boolean)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(CharT, scvt::to_character, scvt::fmt_character)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(signed char, scvt::to_integer, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(signed short, scvt::to_integer, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(signed, scvt::to_integer, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(signed long, scvt::to_integer, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(signed long long, scvt::to_integer, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(unsigned char, scvt::to_integer, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(unsigned short, scvt::to_integer, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(unsigned, scvt::to_integer, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(unsigned long, scvt::to_integer, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(unsigned long long, scvt::to_integer, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(float, scvt::to_float, scvt::fmt_float)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(double, scvt::to_float, scvt::fmt_float)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(long double, scvt::to_float, scvt::fmt_float)
#undef SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER

template<typename Ty>
const char* from_chars(const char* first, const char* last, Ty& v) {
    return string_parser<Ty, char>().from_chars(first, last, v);
}

template<typename Ty>
const wchar_t* from_wchars(const wchar_t* first, const wchar_t* last, Ty& v) {
    return string_parser<Ty, wchar_t>().from_chars(first, last, v);
}

template<typename Ty, typename CharT, typename Traits = std::char_traits<CharT>>
size_t basic_stoval(std::basic_string_view<CharT, Traits> s, Ty& v) {
    return string_parser<Ty, CharT>().from_chars(s.data(), s.data() + s.size(), v) - s.data();
}

template<typename Ty>
size_t stoval(std::string_view s, Ty& v) {
    return basic_stoval(s, v);
}

template<typename Ty>
size_t wstoval(std::wstring_view s, Ty& v) {
    return basic_stoval(s, v);
}

template<typename Ty, typename CharT, typename Traits = std::char_traits<CharT>>
NODISCARD Ty from_basic_string(std::basic_string_view<CharT, Traits> s) {
    Ty result{};
    basic_stoval(s, result);
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

template<typename StrTy, typename Ty>
StrTy& to_basic_string(StrTy& s, const Ty& val, const fmt_opts& fmt = scvt::g_default_opts) {
    formatter<Ty, typename StrTy::value_type>().format(s, val, fmt);
    return s;
}

template<typename Ty>
NODISCARD std::string to_string(const Ty& val) {
    inline_dynbuffer buf;
    to_basic_string(buf, val);
    return std::string(buf.data(), buf.size());
}

template<typename Ty, typename... Opts>
NODISCARD std::string to_string(const Ty& val, const Opts&... opts) {
    inline_dynbuffer buf;
    to_basic_string(buf, val, fmt_opts(opts...));
    return std::string(buf.data(), buf.size());
}

template<typename Ty>
NODISCARD std::wstring to_wstring(const Ty& val) {
    inline_wdynbuffer buf;
    to_basic_string(buf, val);
    return std::wstring(buf.data(), buf.size());
}

template<typename Ty, typename... Opts>
NODISCARD std::wstring to_wstring(const Ty& val, const Opts&... opts) {
    inline_wdynbuffer buf;
    to_basic_string(buf, val, fmt_opts(opts...));
    return std::wstring(buf.data(), buf.size());
}

template<typename Ty>
char* to_chars(char* p, const Ty& val) {
    membuffer buf(p);
    return to_basic_string(buf, val).curr();
}

template<typename Ty, typename... Opts>
char* to_chars(char* p, const Ty& val, const Opts&... opts) {
    membuffer buf(p);
    return to_basic_string(buf, val, fmt_opts(opts...)).curr();
}

template<typename Ty>
wchar_t* to_wchars(wchar_t* p, const Ty& val) {
    wmembuffer buf(p);
    return to_basic_string(buf, val).curr();
}

template<typename Ty, typename... Opts>
wchar_t* to_wchars(wchar_t* p, const Ty& val, const Opts&... opts) {
    wmembuffer buf(p);
    return to_basic_string(buf, val, fmt_opts(opts...)).curr();
}

template<typename Ty>
char* to_chars_n(char* p, size_t n, const Ty& val) {
    membuffer buf(p, p + n);
    return to_basic_string(buf, val).curr();
}

template<typename Ty, typename... Opts>
char* to_chars_n(char* p, size_t n, const Ty& val, const Opts&... opts) {
    membuffer buf(p, p + n);
    return to_basic_string(buf, val, fmt_opts(opts...)).curr();
}

template<typename Ty>
wchar_t* to_wchars_n(wchar_t* p, size_t n, const Ty& val) {
    wmembuffer buf(p, p + n);
    return to_basic_string(buf, val).curr();
}

template<typename Ty, typename... Opts>
wchar_t* to_wchars_n(wchar_t* p, size_t n, const Ty& val, const Opts&... opts) {
    wmembuffer buf(p, p + n);
    return to_basic_string(buf, val, fmt_opts(opts...)).curr();
}

}  // namespace uxs
