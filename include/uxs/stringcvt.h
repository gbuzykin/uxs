#pragma once

#include "chars.h"
#include "iterator.h"
#include "string_view.h"

#include <algorithm>
#include <cstring>
#include <locale>
#include <memory>
#include <stdexcept>
#include <string>

namespace uxs {

template<typename CharT, CharT... C>
struct string_literal {
#if __cplusplus < 201703L
    operator std::basic_string_view<CharT>() const {
        static const std::array<CharT, sizeof...(C)> value{C...};
        return {value.data(), value.size()};
    }
#else   // __cplusplus < 201703L
    static constexpr std::array<CharT, sizeof...(C)> value{C...};
    constexpr operator std::basic_string_view<CharT>() const { return {value.data(), value.size()}; }
#endif  // __cplusplus < 201703L
    UXS_CONSTEXPR std::basic_string_view<CharT> operator()() const { return *this; }
};

// --------------------------

template<typename InputIt, typename InputFn = nofunc>
unsigned from_hex(InputIt in, unsigned n_digs, InputFn fn = InputFn{}, unsigned* n_valid = nullptr) noexcept {
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
void to_hex(unsigned val, OutputIt out, unsigned n_digs, bool upper = false, OutputFn fn = OutputFn{}) noexcept {
    const char* digs = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    unsigned shift = n_digs << 2;
    while (shift) {
        shift -= 4;
        *out++ = fn(digs[(val >> shift) & 0xf]);
    }
}

// --------------------------

template<typename Ty>
class basic_membuffer {
 private:
    static_assert(std::is_trivially_copyable<Ty>::value && std::is_trivially_destructible<Ty>::value,
                  "uxs::basic_membuffer<> must have trivially copyable and destructible value type");

 public:
    using value_type = Ty;
    using pointer = Ty*;
    using const_pointer = const Ty*;
    using reference = Ty&;
    using const_reference = const Ty&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    explicit basic_membuffer(Ty* first, Ty* last = reinterpret_cast<Ty*>(0)) noexcept : curr_(first), last_(last) {}
    virtual ~basic_membuffer() = default;
    basic_membuffer(const basic_membuffer&) = delete;
    basic_membuffer& operator=(const basic_membuffer&) = delete;

    size_type avail() const noexcept { return last_ - curr_; }
    const_pointer curr() const noexcept { return curr_; }
    pointer curr() noexcept { return curr_; }
    pointer* p_curr() noexcept { return &curr_; }
    const_pointer last() const noexcept { return last_; }
    pointer last() noexcept { return last_; }
    reference back() noexcept { return *(curr_ - 1); }

    basic_membuffer& advance(size_type n) noexcept {
        assert(n <= avail());
        curr_ += n;
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_random_access_iterator<InputIt>::value>>
    basic_membuffer& append(InputIt first, InputIt last) {
        assert(first <= last);
        size_type count = static_cast<size_type>(last - first), n_avail = avail();
        while (count > n_avail) {
            curr_ = std::copy_n(first, n_avail, curr_);
            first += n_avail, count -= n_avail;
            if (!(n_avail = try_grow(count))) { return *this; }
        }
        curr_ = std::copy(first, last, curr_);
        return *this;
    }

    basic_membuffer& append(size_type count, value_type val) {
        size_type n_avail = avail();
        while (count > n_avail) {
            curr_ = std::fill_n(curr_, n_avail, val);
            count -= n_avail;
            if (!(n_avail = try_grow(count))) { return *this; }
        }
        curr_ = std::fill_n(curr_, count, val);
        return *this;
    }

    template<typename... Args>
    void emplace_back(Args&&... args) {
        if (curr_ != last_ || try_grow(1)) { new (curr_++) value_type(std::forward<Args>(args)...); }
    }
    void push_back(value_type val) {
        if (curr_ != last_ || try_grow(1)) { *curr_++ = val; }
    }
    void pop_back() noexcept { --curr_; }

    template<typename Range, typename = std::void_t<decltype(std::declval<Range>().end())>>
    basic_membuffer& operator+=(const Range& r) {
        return append(r.begin(), r.end());
    }
    basic_membuffer& operator+=(const value_type* s) { return *this += std::basic_string_view<value_type>(s); }
    basic_membuffer& operator+=(value_type ch) {
        push_back(ch);
        return *this;
    }

 protected:
    void set(Ty* curr) noexcept { curr_ = curr; }
    void set(Ty* curr, Ty* last) noexcept { curr_ = curr, last_ = last; }
    virtual size_type try_grow(size_type extra) { return 0; }

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
    using pointer = typename basic_membuffer<Ty>::pointer;
    using const_pointer = typename basic_membuffer<Ty>::const_pointer;
    using reference = typename basic_membuffer<Ty>::reference;
    using const_reference = typename basic_membuffer<Ty>::const_reference;
    using const_iterator = uxs::array_iterator<basic_dynbuffer, true>;
    using iterator = const_iterator;
    using size_type = typename basic_membuffer<Ty>::size_type;
    using difference_type = typename basic_membuffer<Ty>::difference_type;

    ~basic_dynbuffer() override {
        if (is_allocated_) { this->deallocate(first_, capacity()); }
    }

    bool empty() const noexcept { return first_ == this->curr(); }
    size_type size() const noexcept { return this->curr() - first_; }
    size_type capacity() const noexcept { return this->last() - first_; }
    const_pointer data() const noexcept { return first_; }
    pointer data() noexcept { return first_; }
    const_iterator begin() const noexcept { return const_iterator(first_, first_, this->curr()); }
    const_iterator end() const noexcept { return const_iterator(this->curr(), first_, this->curr()); }
    void clear() noexcept { this->set(first_); }

    void reserve(size_type extra) {
        if (extra > this->avail()) { try_grow(extra); }
    }

 protected:
    basic_dynbuffer(Ty* first, Ty* last) noexcept
        : basic_membuffer<Ty>(first, last), first_(first), is_allocated_(false) {}

    size_type try_grow(size_type extra) override {
        size_type sz = size(), cap = capacity(), delta_sz = std::max(extra, sz >> 1);
        const size_type max_avail = std::allocator_traits<alloc_type>::max_size(*this) - sz;
        if (delta_sz > max_avail) {
            if (extra > max_avail) { throw std::length_error("too much to reserve"); }
            delta_sz = std::max(extra, max_avail >> 1);
        }
        sz += delta_sz;
        Ty* first = this->allocate(sz);
        this->set(std::copy(first_, this->curr(), first), first + sz);
        if (is_allocated_) { this->deallocate(first_, cap); }
        first_ = first, is_allocated_ = true;
        return this->avail();
    }

 private:
    Ty* first_;
    bool is_allocated_;
};

template<typename Ty, std::size_t InlineBufSize = 0, typename Alloc = std::allocator<Ty>>
class inline_basic_dynbuffer final : public basic_dynbuffer<Ty, Alloc> {
 public:
    inline_basic_dynbuffer() noexcept
        : basic_dynbuffer<Ty, Alloc>(reinterpret_cast<Ty*>(buf_), reinterpret_cast<Ty*>(buf_) + inline_buf_size) {}

 private:
    enum : unsigned {
#if defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
        inline_buf_size = InlineBufSize != 0 ? InlineBufSize : 256 / sizeof(Ty)
#else   // defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
        inline_buf_size = 7
#endif  // defined(NDEBUG) || !defined(_DEBUG_REDUCED_BUFFERS)
    };
    alignas(std::alignment_of<Ty>::value) std::uint8_t buf_[inline_buf_size * sizeof(Ty)];
};

using inline_dynbuffer = inline_basic_dynbuffer<char>;
using inline_wdynbuffer = inline_basic_dynbuffer<wchar_t>;

// --------------------------

enum class fmt_flags : unsigned {
    none = 0,
    dec = 1,
    bin = 2,
    oct = 3,
    hex = 4,
    character = 5,
    base_field = 7,
    uppercase = 8,
    fixed = 0x10,
    scientific = 0x20,
    general = 0x30,
    float_field = 0x30,
    sign_neg = 0x40,
    sign_pos = 0x80,
    sign_align = 0xc0,
    sign_field = 0xc0,
    left = 0x100,
    right = 0x200,
    internal = 0x300,
    adjust_field = 0x300,
    leading_zeroes = 0x400,
    alternate = 0x800,
    json_compat = 0x1000,
    localize = 0x2000,
};
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(fmt_flags, unsigned);

struct fmt_opts {
#if __cplusplus < 201703L
    explicit fmt_opts(fmt_flags fl = fmt_flags::none, int p = -1, unsigned w = 0, int f = ' ')
        : flags(fl), prec(p), width(w), fill(f) {}
#endif  // __cplusplus < 201703L
    fmt_flags flags = fmt_flags::none;
    int prec = -1;
    unsigned width = 0;
    int fill = ' ';
};

class UXS_EXPORT_ALL_STUFF_FOR_GNUC format_error : public std::runtime_error {
 public:
    UXS_EXPORT explicit format_error(const char* message);
    UXS_EXPORT explicit format_error(const std::string& message);
    UXS_EXPORT const char* what() const noexcept override;
};

class locale_ref {
 public:
    locale_ref() noexcept = default;
    explicit locale_ref(const std::locale& loc) noexcept : ref_(&loc) {}
    operator bool() const noexcept { return ref_ != nullptr; }
    std::locale operator*() const noexcept { return ref_ ? *ref_ : std::locale{}; }

 private:
    const std::locale* ref_ = nullptr;
};

// --------------------------

template<typename StrTy, typename Func>
void append_adjusted(StrTy& s, Func fn, unsigned len, fmt_opts fmt, bool prefer_right = false) {
    unsigned left = fmt.width - len, right = left;
    if ((fmt.flags & fmt_flags::adjust_field) == fmt_flags::left) {
        left = 0;
    } else if ((fmt.flags & fmt_flags::adjust_field) == fmt_flags::internal) {
        left >>= 1, right -= left;
    } else if ((fmt.flags & fmt_flags::adjust_field) == fmt_flags::right || prefer_right) {
        right = 0;
    } else {
        left = 0;
    }
    s.append(left, fmt.fill);
    fn(s);
    s.append(right, fmt.fill);
}

// --------------------------

namespace scvt {

template<typename Ty>
struct fp_traits;

template<typename TyTo, typename TyFrom>
TyTo bit_cast(const TyFrom& v) noexcept {
    static_assert(sizeof(TyTo) == sizeof(TyFrom), "bad bit cast");
    TyTo ret;
    std::memcpy(&ret, &v, sizeof(TyFrom));
    return ret;
}

template<>
struct fp_traits<float> {
    static_assert(sizeof(float) == sizeof(std::uint32_t), "type size mismatch");
    enum : unsigned { total_bits = 32, bits_per_mantissa = 23 };
    enum : std::uint64_t { mantissa_mask = (1ull << bits_per_mantissa) - 1 };
    enum : int { exp_max = (1 << (total_bits - bits_per_mantissa - 1)) - 1 };
    static std::uint64_t to_u64(float f) noexcept { return bit_cast<std::uint32_t>(f); }
    static float from_u64(std::uint64_t u64) noexcept { return bit_cast<float>(static_cast<std::uint32_t>(u64)); }
};

template<>
struct fp_traits<double> {
    static_assert(sizeof(double) == sizeof(std::uint64_t), "type size mismatch");
    enum : unsigned { total_bits = 64, bits_per_mantissa = 52 };
    enum : std::uint64_t { mantissa_mask = (1ull << bits_per_mantissa) - 1 };
    enum : int { exp_max = (1 << (total_bits - bits_per_mantissa - 1)) - 1 };
    static std::uint64_t to_u64(double f) noexcept { return bit_cast<std::uint64_t>(f); }
    static double from_u64(std::uint64_t u64) noexcept { return bit_cast<double>(u64); }
};

template<>
struct fp_traits<long double> : fp_traits<double> {
    static std::uint64_t to_u64(long double f) noexcept { return bit_cast<std::uint64_t>(static_cast<double>(f)); }
    static long double from_u64(std::uint64_t u64) noexcept { return static_cast<long double>(bit_cast<double>(u64)); }
};

// --------------------------

template<typename CharT>
UXS_EXPORT bool to_boolean(const CharT* p, const CharT* end, const CharT*& last) noexcept;

template<typename Ty, typename CharT>
UXS_EXPORT Ty to_integer_common(const CharT* p, const CharT* end, const CharT*& last, Ty pos_limit) noexcept;

template<typename CharT>
UXS_EXPORT std::uint64_t to_float_common(const CharT* p, const CharT* end, const CharT*& last, unsigned bpm,
                                         int exp_max) noexcept;

template<typename Ty, typename CharT>
Ty to_integer(const CharT* p, const CharT* end, const CharT*& last) noexcept {
    using UTy = typename std::make_unsigned<Ty>::type;
    using ReducedTy = std::conditional_t<(sizeof(UTy) <= sizeof(std::uint32_t)), std::uint32_t, std::uint64_t>;
    return static_cast<Ty>(to_integer_common<ReducedTy>(p, end, last, std::numeric_limits<UTy>::max()));
}

template<typename Ty, typename CharT>
Ty to_float(const CharT* p, const CharT* end, const CharT*& last) noexcept {
    return fp_traits<Ty>::from_u64(
        to_float_common(p, end, last, fp_traits<Ty>::bits_per_mantissa, fp_traits<Ty>::exp_max));
}

// --------------------------

template<typename CharT>
UXS_EXPORT void fmt_boolean(basic_membuffer<CharT>& s, bool val, fmt_opts fmt, locale_ref loc);

template<typename StrTy,
         typename = std::enable_if_t<!std::is_convertible<StrTy&, basic_membuffer<typename StrTy::value_type>&>::value>>
void fmt_boolean(StrTy& s, bool val, fmt_opts fmt, locale_ref loc) {
    inline_basic_dynbuffer<typename StrTy::value_type> buf;
    fmt_boolean(buf, val, fmt, loc);
    s.append(buf.begin(), buf.end());
}

template<typename CharT>
UXS_EXPORT void fmt_character(basic_membuffer<CharT>& s, CharT val, fmt_opts fmt, locale_ref loc);

template<typename StrTy,
         typename = std::enable_if_t<!std::is_convertible<StrTy&, basic_membuffer<typename StrTy::value_type>&>::value>>
void fmt_character(StrTy& s, typename StrTy::value_type val, fmt_opts fmt, locale_ref loc) {
    inline_basic_dynbuffer<typename StrTy::value_type> buf;
    fmt_character(buf, val, fmt, loc);
    s.append(buf.begin(), buf.end());
}

template<typename CharT>
UXS_EXPORT void fmt_string(basic_membuffer<CharT>& s, std::basic_string_view<CharT> val, fmt_opts fmt, locale_ref loc);

template<typename StrTy,
         typename = std::enable_if_t<!std::is_convertible<StrTy&, basic_membuffer<typename StrTy::value_type>&>::value>>
void fmt_string(StrTy& s, std::basic_string_view<typename StrTy::value_type> val, fmt_opts fmt, locale_ref loc) {
    inline_basic_dynbuffer<typename StrTy::value_type> buf;
    fmt_string(buf, val, fmt, loc);
    s.append(buf.begin(), buf.end());
}

template<typename CharT, typename Ty>
UXS_EXPORT void fmt_integer_common(basic_membuffer<CharT>& s, Ty val, bool is_signed, fmt_opts fmt, locale_ref loc);

template<typename StrTy, typename Ty,
         typename = std::enable_if_t<!std::is_convertible<StrTy&, basic_membuffer<typename StrTy::value_type>&>::value>>
void fmt_integer_common(StrTy& s, Ty val, bool is_signed, fmt_opts fmt, locale_ref loc) {
    inline_basic_dynbuffer<typename StrTy::value_type> buf;
    fmt_integer_common(buf, val, is_signed, fmt, loc);
    s.append(buf.begin(), buf.end());
}

template<typename CharT>
UXS_EXPORT void fmt_float_common(basic_membuffer<CharT>& s, std::uint64_t u64, fmt_opts fmt, unsigned bpm, int exp_max,
                                 locale_ref loc);

template<typename StrTy,
         typename = std::enable_if_t<!std::is_convertible<StrTy&, basic_membuffer<typename StrTy::value_type>&>::value>>
void fmt_float_common(StrTy& s, std::uint64_t u64, fmt_opts fmt, unsigned bpm, int exp_max, locale_ref loc) {
    inline_basic_dynbuffer<typename StrTy::value_type> buf;
    fmt_float_common(buf, u64, fmt, bpm, exp_max, loc);
    s.append(buf.begin(), buf.end());
}

template<typename StrTy, typename Ty>
void fmt_integer(StrTy& s, Ty val, fmt_opts fmt, locale_ref loc) {
    using UTy = typename std::make_unsigned<Ty>::type;
    using ReducedTy = std::conditional_t<(sizeof(UTy) <= sizeof(std::uint32_t)), std::uint32_t, std::uint64_t>;
    const bool is_signed = std::is_signed<Ty>::value;
    fmt_integer_common(s, static_cast<ReducedTy>(val), is_signed, fmt, loc);
}

template<typename StrTy, typename Ty>
void fmt_float(StrTy& s, Ty val, fmt_opts fmt, locale_ref loc) {
    using FpTy = std::conditional_t<(sizeof(Ty) <= sizeof(double)), Ty, double>;
    fmt_float_common(s, fp_traits<Ty>::to_u64(static_cast<FpTy>(val)), fmt, fp_traits<Ty>::bits_per_mantissa,
                     fp_traits<Ty>::exp_max, loc);
}

}  // namespace scvt

// --------------------------

template<typename Ty, typename CharT = char>
struct string_converter;

template<typename Ty, typename CharT = char>
struct formatter;

template<typename Ty, typename CharT = char, typename = void>
struct convertible_from_string : std::false_type {};
template<typename Ty, typename CharT>
struct convertible_from_string<
    Ty, CharT,
    std::enable_if_t<std::is_same<
        decltype(string_converter<Ty, CharT>{}.from_chars(nullptr, nullptr, std::declval<Ty&>())), const CharT*>::value>>
    : std::true_type {};

template<typename Ty, typename StrTy = membuffer, typename = void>
struct convertible_to_string : std::false_type {};
template<typename Ty, typename StrTy>
struct convertible_to_string<Ty, StrTy,
                             std::void_t<decltype(string_converter<Ty, typename StrTy::value_type>{}.to_string(
                                 std::declval<StrTy&>(), std::declval<const Ty&>(), fmt_opts{}))>> : std::true_type {};

#define UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(ty, from_func, fmt_func) \
    template<typename CharT> \
    struct string_converter<ty, CharT> { \
        const CharT* from_chars(const CharT* first, const CharT* last, ty& val) const noexcept { \
            auto t = from_func(first, last, last); \
            if (last != first) { val = t; } \
            return last; \
        } \
        template<typename StrTy> \
        void to_string(StrTy& s, ty val, fmt_opts fmt, locale_ref loc = locale_ref{}) const { \
            fmt_func(s, val, fmt, loc); \
        } \
    };
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(bool, scvt::to_boolean, scvt::fmt_boolean)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(signed char, scvt::to_integer<signed char>, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(signed short, scvt::to_integer<signed short>, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(signed, scvt::to_integer<signed>, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(signed long, scvt::to_integer<signed long>, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(signed long long, scvt::to_integer<signed long long>, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(unsigned char, scvt::to_integer<unsigned char>, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(unsigned short, scvt::to_integer<unsigned short>, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(unsigned, scvt::to_integer<unsigned>, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(unsigned long, scvt::to_integer<unsigned long>, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(unsigned long long, scvt::to_integer<unsigned long long>, scvt::fmt_integer)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(float, scvt::to_float<float>, scvt::fmt_float)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(double, scvt::to_float<double>, scvt::fmt_float)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(long double, scvt::to_float<long double>, scvt::fmt_float)
#undef SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER

template<typename CharT, typename Ty>
const CharT* from_basic_chars(const CharT* first, const CharT* last, Ty& val) {
    return string_converter<Ty, CharT>{}.from_chars(first, last, val);
}

template<typename Ty>
const char* from_chars(const char* first, const char* last, Ty& val) {
    return from_basic_chars(first, last, val);
}

template<typename Ty>
const wchar_t* from_wchars(const wchar_t* first, const wchar_t* last, Ty& val) {
    return from_basic_chars(first, last, val);
}

template<typename Ty, typename CharT, typename Traits = std::char_traits<CharT>>
std::size_t basic_stoval(std::basic_string_view<CharT, Traits> s, Ty& val) {
    return from_basic_chars(s.data(), s.data() + s.size(), val) - s.data();
}

template<typename Ty>
std::size_t stoval(std::string_view s, Ty& val) {
    return basic_stoval(s, val);
}

template<typename Ty>
std::size_t wstoval(std::wstring_view s, Ty& val) {
    return basic_stoval(s, val);
}

template<typename Ty, typename CharT, typename Traits = std::char_traits<CharT>>
Ty from_basic_string(std::basic_string_view<CharT, Traits> s) {
    Ty result{};
    basic_stoval(s, result);
    return result;
}

template<typename Ty>
Ty from_string(std::string_view s) {
    return from_basic_string<Ty>(s);
}

template<typename Ty>
Ty from_wstring(std::wstring_view s) {
    return from_basic_string<Ty>(s);
}

template<typename StrTy, typename Ty>
StrTy& to_basic_string(StrTy& s, const Ty& val, fmt_opts fmt = fmt_opts{}) {
    string_converter<Ty, typename StrTy::value_type>{}.to_string(s, val, fmt);
    return s;
}

template<typename StrTy, typename Ty>
StrTy& to_basic_string(StrTy& s, const std::locale& loc, const Ty& val, fmt_opts fmt = fmt_opts{}) {
    string_converter<Ty, typename StrTy::value_type>{}.to_string(s, val, fmt, loc);
    return s;
}

template<typename Ty>
std::string to_string(const Ty& val) {
    inline_dynbuffer buf;
    to_basic_string(buf, val);
    return std::string(buf.data(), buf.size());
}

template<typename Ty, typename... Opts>
std::string to_string(const Ty& val, const Opts&... opts) {
    inline_dynbuffer buf;
    to_basic_string(buf, val, fmt_opts{opts...});
    return std::string(buf.data(), buf.size());
}

template<typename Ty>
std::wstring to_wstring(const Ty& val) {
    inline_wdynbuffer buf;
    to_basic_string(buf, val);
    return std::wstring(buf.data(), buf.size());
}

template<typename Ty, typename... Opts>
std::wstring to_wstring(const Ty& val, const Opts&... opts) {
    inline_wdynbuffer buf;
    to_basic_string(buf, val, fmt_opts{opts...});
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
    return to_basic_string(buf, val, fmt_opts{opts...}).curr();
}

template<typename Ty>
wchar_t* to_wchars(wchar_t* p, const Ty& val) {
    wmembuffer buf(p);
    return to_basic_string(buf, val).curr();
}

template<typename Ty, typename... Opts>
wchar_t* to_wchars(wchar_t* p, const Ty& val, const Opts&... opts) {
    wmembuffer buf(p);
    return to_basic_string(buf, val, fmt_opts{opts...}).curr();
}

template<typename Ty>
char* to_chars_n(char* p, std::size_t n, const Ty& val) {
    membuffer buf(p, p + n);
    return to_basic_string(buf, val).curr();
}

template<typename Ty, typename... Opts>
char* to_chars_n(char* p, std::size_t n, const Ty& val, const Opts&... opts) {
    membuffer buf(p, p + n);
    return to_basic_string(buf, val, fmt_opts{opts...}).curr();
}

template<typename Ty>
wchar_t* to_wchars_n(wchar_t* p, std::size_t n, const Ty& val) {
    wmembuffer buf(p, p + n);
    return to_basic_string(buf, val).curr();
}

template<typename Ty, typename... Opts>
wchar_t* to_wchars_n(wchar_t* p, std::size_t n, const Ty& val, const Opts&... opts) {
    wmembuffer buf(p, p + n);
    return to_basic_string(buf, val, fmt_opts{opts...}).curr();
}

}  // namespace uxs
