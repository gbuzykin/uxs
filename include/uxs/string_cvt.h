#pragma once

#include "chars.h"
#include "string_util.h"

#include <algorithm>
#include <cstring>
#include <locale>
#include <stdexcept>

namespace uxs {

// --------------------------

template<typename InputIt, typename InputFn = nofunc>
unsigned from_hex(InputIt in, unsigned n_digs, InputFn fn = InputFn{}, unsigned* n_valid = nullptr) noexcept {
    unsigned val = 0;
    if (n_valid) { *n_valid = n_digs; }
    while (n_digs) {
        const unsigned dig = dig_v(fn(*in));
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
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using reference = value_type&;
    using const_reference = value_type;
    using const_iterator = array_iterator<basic_membuffer, const_pointer, true>;
    using iterator = const_iterator;

    basic_membuffer() noexcept = default;
    explicit basic_membuffer(Ty* data) noexcept : data_(data), capacity_(std::numeric_limits<size_type>::max()) {}
    basic_membuffer(Ty* data, std::size_t capacity) noexcept : data_(data), capacity_(capacity) {}
    basic_membuffer(Ty* data, std::size_t size, std::size_t capacity) noexcept
        : data_(data), size_(size), capacity_(capacity) {}
    virtual ~basic_membuffer() = default;
    basic_membuffer(const basic_membuffer&) = delete;
    basic_membuffer& operator=(const basic_membuffer&) = delete;

    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    size_type capacity() const noexcept { return capacity_; }
    size_type avail() const noexcept { return capacity_ - size_; }
    const_pointer data() const noexcept { return data_; }
    pointer data() noexcept { return data_; }
    const_pointer endp() const noexcept { return data_ + size_; }
    pointer endp() noexcept { return data_ + size_; }
    const_reference back() const noexcept { return data_[size_ - 1]; }
    reference back() noexcept { return data_[size_ - 1]; }
    const_iterator begin() const noexcept { return const_iterator(data_, data_, endp()); }
    const_iterator end() const noexcept { return const_iterator(endp(), data_, endp()); }
    void clear() noexcept { size_ = 0; }

    const_reference operator[](std::size_t n) const noexcept {
        assert(n < size_);
        return data_[n];
    }
    reference operator[](std::size_t n) noexcept {
        assert(n < size_);
        return data_[n];
    }

    void setsize(std::size_t size) noexcept {
        assert(size <= capacity_);
        size_ = size;
    }

    void advance(difference_type n) noexcept {
        assert(n >= 0 ? static_cast<std::size_t>(n) <= avail() : static_cast<std::size_t>(-n) <= size_);
        size_ += n;
    }

    template<typename InputIt, typename = std::enable_if_t<is_random_access_iterator<InputIt>::value &&
                                                           std::is_same<iterator_value_t<InputIt>, Ty>::value>>
    basic_membuffer& append(InputIt first, InputIt last) {
        assert(first <= last);
        size_type count = static_cast<size_type>(last - first);
        size_type n_avail = avail();
        while (count > n_avail) {
            std::copy_n(first, n_avail, endp());
            first += n_avail, count -= n_avail, size_ += n_avail;
            if (!(n_avail = try_grow(count))) { return *this; }
        }
        std::copy(first, last, endp());
        size_ += count;
        return *this;
    }

    basic_membuffer& append(size_type count, value_type val) {
        size_type n_avail = avail();
        while (count > n_avail) {
            std::fill_n(endp(), n_avail, val);
            count -= n_avail, size_ += n_avail;
            if (!(n_avail = try_grow(count))) { return *this; }
        }
        std::fill_n(endp(), count, val);
        size_ += count;
        return *this;
    }

    template<typename... Args>
    void emplace_back(Args&&... args) {
        if (size_ != capacity_ || try_grow(1)) { new (&data_[size_++]) value_type(std::forward<Args>(args)...); }
    }
    void push_back(value_type val) { emplace_back(val); }
    void pop_back() noexcept { --size_; }

    template<typename CharT = value_type>
    std::enable_if_t<is_character<CharT>::value, basic_membuffer&> append(const value_type* s, size_type count) {
        return append(s, s + count);
    }
    template<typename CharT = value_type>
    std::enable_if_t<is_character<CharT>::value, basic_membuffer&> operator+=(std::basic_string_view<value_type> s) {
        return append(s.data(), s.size());
    }
    template<typename CharT = value_type>
    std::enable_if_t<is_character<CharT>::value, basic_membuffer&> operator+=(const value_type* s) {
        return *this += std::basic_string_view<value_type>(s);
    }
    template<typename CharT = value_type>
    std::enable_if_t<is_character<CharT>::value, basic_membuffer&> operator+=(value_type ch) {
        push_back(ch);
        return *this;
    }

 protected:
    void reset(Ty* data, std::size_t size, std::size_t capacity) noexcept {
        data_ = data, size_ = size, capacity_ = capacity;
    }
    virtual size_type try_grow(size_type /*extra*/) { return 0; }

 private:
    Ty* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};

using membuffer = basic_membuffer<char>;
using wmembuffer = basic_membuffer<wchar_t>;

template<typename Ty, typename Alloc>
class basic_dynbuffer : protected std::allocator_traits<Alloc>::template rebind_alloc<Ty>, public basic_membuffer<Ty> {
 private:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<Ty>;

 public:
    using value_type = typename basic_membuffer<Ty>::value_type;
    using size_type = typename basic_membuffer<Ty>::size_type;
    using difference_type = typename basic_membuffer<Ty>::difference_type;
    using pointer = typename basic_membuffer<Ty>::pointer;
    using const_pointer = typename basic_membuffer<Ty>::const_pointer;
    using reference = typename basic_membuffer<Ty>::reference;
    using const_reference = typename basic_membuffer<Ty>::const_reference;

    basic_dynbuffer() noexcept = default;

    ~basic_dynbuffer() override {
        if (is_allocated_) { this->deallocate(this->data(), this->capacity()); }
    }

    void reserve(size_type extra) {
        if (extra > this->avail()) { try_grow(extra); }
    }

 protected:
    basic_dynbuffer(Ty* data, std::size_t capacity) noexcept : basic_membuffer<Ty>(data, capacity) {}

    size_type try_grow(size_type extra) override {
        size_type sz = this->size();
        size_type delta_sz = std::max(extra, sz >> 1);
        const size_type max_avail = std::allocator_traits<alloc_type>::max_size(*this) - sz;
        if (delta_sz > max_avail) {
            if (extra > max_avail) { throw std::length_error("too much to reserve"); }
            delta_sz = std::max(extra, max_avail >> 1);
        }
        sz += delta_sz;
        Ty* data = this->allocate(sz);
        std::copy(this->data(), this->endp(), data);
        if (is_allocated_) { this->deallocate(this->data(), this->capacity()); }
        this->reset(data, this->size(), sz);
        is_allocated_ = true;
        return this->avail();
    }

 private:
    bool is_allocated_ = false;
};

template<typename Ty, std::size_t InlineBufSize = 0, typename Alloc = std::allocator<Ty>>
class inline_basic_dynbuffer final : public basic_dynbuffer<Ty, Alloc> {
 public:
    inline_basic_dynbuffer() noexcept : basic_dynbuffer<Ty, Alloc>(reinterpret_cast<Ty*>(buf_), inline_buf_size) {}

 private:
    enum : unsigned {
#if defined(NDEBUG) || !defined(UXS_DEBUG_REDUCED_BUFFERS)
        inline_buf_size = InlineBufSize != 0 ? InlineBufSize : 256 / sizeof(Ty)
#else   // defined(NDEBUG) || !defined(UXS_DEBUG_REDUCED_BUFFERS)
        inline_buf_size = 7
#endif  // defined(NDEBUG) || !defined(UXS_DEBUG_REDUCED_BUFFERS)
    };
    alignas(std::alignment_of<Ty>::value) std::uint8_t buf_[inline_buf_size * sizeof(Ty)]{};
};

using inline_dynbuffer = inline_basic_dynbuffer<char>;
using inline_wdynbuffer = inline_basic_dynbuffer<wchar_t>;

template<typename Ty>
class basic_membuffer_with_size_tracker final : public basic_membuffer<Ty> {
 public:
    basic_membuffer_with_size_tracker(Ty* data, std::size_t capacity) noexcept
        : basic_membuffer<Ty>(data, capacity), tracked_size_(capacity) {}
    std::size_t tracked_size() const { return this->avail() ? this->size() : tracked_size_; }

 private:
    std::size_t tracked_size_;

    std::size_t try_grow(std::size_t extra) override {
        tracked_size_ += extra;
        return 0;
    }
};

using membuffer_with_size_tracker = basic_membuffer_with_size_tracker<char>;
using wmembuffer_with_size_tracker = basic_membuffer_with_size_tracker<wchar_t>;

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
    debug_format = 0x4000,
};
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(fmt_flags);

struct fmt_opts {
    UXS_CONSTEXPR fmt_opts() noexcept = default;
    UXS_CONSTEXPR explicit fmt_opts(fmt_flags fl, int p = -1, unsigned w = 0, int f = ' ') noexcept
        : flags(fl), prec(p), width(w), fill(f) {}
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

template<typename StrTy, typename InputIt>
std::size_t append_escaped_text(StrTy& s, InputIt first, InputIt last, bool single_quoted,
                                std::size_t max_width = std::numeric_limits<std::size_t>::max()) {
    using char_type = typename StrTy::value_type;
    if (max_width == 0) { return 0; }
    s += single_quoted ? '\'' : '\"';
    std::size_t width = 1;
    std::uint32_t code = 0;
    unsigned count = 0;
    auto first0 = first;
    for (auto next = first; (count = utf_decoder<char_type>{}.decode(first, last, next, code)) != 0; first = next) {
        char esc = '\0';
        bool is_wellformed = true;
        switch (code) {
            case '\t': esc = 't'; break;
            case '\n': esc = 'n'; break;
            case '\r': esc = 'r'; break;
            case '\\': esc = '\\'; break;
            case '\"': {
                if (single_quoted) {
                    if (width == max_width) { goto finish; }
                    ++width;
                    continue;
                }
                esc = '\"';
            } break;
            case '\'': {
                if (!single_quoted) {
                    if (width == max_width) { goto finish; }
                    ++width;
                    continue;
                }
                esc = '\'';
            } break;
            default: {
                if ((is_wellformed = count > 1 || utf_decoder<char_type>{}.is_wellformed(*first))) {
                    if (is_utf_code_printable(code)) {
                        const unsigned w = get_utf_code_width(code);
                        if (max_width - width < w) { goto finish; }
                        width += w;
                        continue;
                    }
                }
            } break;
        }
        s.append(first0, first);
        if (esc) {
            if (max_width - width < 2) { goto finish; }
            width += 2;
            s += '\\';
            s += esc;
        } else {
            std::array<char_type, 8> digs;
            char_type* p = digs.data();
            do { *p++ = "0123456789abcdef"[code & 0xf]; } while ((code >>= 4));
            const unsigned count = 1 + static_cast<unsigned>(p - digs.data());
            const unsigned w = 4 + count;
            if (max_width - width < w) { goto finish; }
            width += w;
            s += is_wellformed ? string_literal<char_type, '\\', 'u', '{'>{}() :
                                 string_literal<char_type, '\\', 'x', '{'>{}();
            do { s += *--p; } while (p != digs.data());
            s += '}';
        }
        first0 = next;
    }
finish:
    s.append(first0, first);
    if (width == max_width) { return width; }
    s += single_quoted ? '\'' : '\"';
    return width + 1;
}

template<typename CharT, typename InputIt>
std::size_t estimate_string_width(InputIt first, InputIt last) {
    std::size_t width = 0;
    for (std::uint32_t code = 0; utf_decoder<CharT>{}.decode(first, last, first, code) != 0;
         width += get_utf_code_width(code)) {}
    return width;
}

template<typename StrTy, typename Func>
void append_adjusted(StrTy& s, Func fn, unsigned len, fmt_opts fmt, bool prefer_right = false) {
    unsigned left = fmt.width - len;
    unsigned right = left;
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
    enum : std::uint64_t { mantissa_mask = (1ULL << bits_per_mantissa) - 1 };
    enum : int { exp_max = (1 << (total_bits - bits_per_mantissa - 1)) - 1 };
    static std::uint64_t to_u64(float f) noexcept { return bit_cast<std::uint32_t>(f); }
    static float from_u64(std::uint64_t u64) noexcept { return bit_cast<float>(static_cast<std::uint32_t>(u64)); }
};

template<>
struct fp_traits<double> {
    static_assert(sizeof(double) == sizeof(std::uint64_t), "type size mismatch");
    enum : unsigned { total_bits = 64, bits_per_mantissa = 52 };
    enum : std::uint64_t { mantissa_mask = (1ULL << bits_per_mantissa) - 1 };
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

// digit pairs
UXS_FORCE_INLINE const char* get_digits(unsigned n) noexcept {
    static const UXS_CONSTEXPR char digs[][2] = {
        {'0', '0'}, {'0', '1'}, {'0', '2'}, {'0', '3'}, {'0', '4'}, {'0', '5'}, {'0', '6'}, {'0', '7'}, {'0', '8'},
        {'0', '9'}, {'1', '0'}, {'1', '1'}, {'1', '2'}, {'1', '3'}, {'1', '4'}, {'1', '5'}, {'1', '6'}, {'1', '7'},
        {'1', '8'}, {'1', '9'}, {'2', '0'}, {'2', '1'}, {'2', '2'}, {'2', '3'}, {'2', '4'}, {'2', '5'}, {'2', '6'},
        {'2', '7'}, {'2', '8'}, {'2', '9'}, {'3', '0'}, {'3', '1'}, {'3', '2'}, {'3', '3'}, {'3', '4'}, {'3', '5'},
        {'3', '6'}, {'3', '7'}, {'3', '8'}, {'3', '9'}, {'4', '0'}, {'4', '1'}, {'4', '2'}, {'4', '3'}, {'4', '4'},
        {'4', '5'}, {'4', '6'}, {'4', '7'}, {'4', '8'}, {'4', '9'}, {'5', '0'}, {'5', '1'}, {'5', '2'}, {'5', '3'},
        {'5', '4'}, {'5', '5'}, {'5', '6'}, {'5', '7'}, {'5', '8'}, {'5', '9'}, {'6', '0'}, {'6', '1'}, {'6', '2'},
        {'6', '3'}, {'6', '4'}, {'6', '5'}, {'6', '6'}, {'6', '7'}, {'6', '8'}, {'6', '9'}, {'7', '0'}, {'7', '1'},
        {'7', '2'}, {'7', '3'}, {'7', '4'}, {'7', '5'}, {'7', '6'}, {'7', '7'}, {'7', '8'}, {'7', '9'}, {'8', '0'},
        {'8', '1'}, {'8', '2'}, {'8', '3'}, {'8', '4'}, {'8', '5'}, {'8', '6'}, {'8', '7'}, {'8', '8'}, {'8', '9'},
        {'9', '0'}, {'9', '1'}, {'9', '2'}, {'9', '3'}, {'9', '4'}, {'9', '5'}, {'9', '6'}, {'9', '7'}, {'9', '8'},
        {'9', '9'}};
    assert(n < 100);
    return digs[n];
}

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
    s.append(buf.data(), buf.size());
}

template<typename CharT, typename Ty>
UXS_EXPORT void fmt_integer_common(basic_membuffer<CharT>& s, Ty val, bool is_signed, fmt_opts fmt, locale_ref loc);

template<typename StrTy, typename Ty,
         typename = std::enable_if_t<!std::is_convertible<StrTy&, basic_membuffer<typename StrTy::value_type>&>::value>>
void fmt_integer_common(StrTy& s, Ty val, bool is_signed, fmt_opts fmt, locale_ref loc) {
    inline_basic_dynbuffer<typename StrTy::value_type> buf;
    fmt_integer_common(buf, val, is_signed, fmt, loc);
    s.append(buf.data(), buf.size());
}

template<typename CharT>
UXS_EXPORT void fmt_float_common(basic_membuffer<CharT>& s, std::uint64_t u64, fmt_opts fmt, unsigned bpm, int exp_max,
                                 locale_ref loc);

template<typename StrTy,
         typename = std::enable_if_t<!std::is_convertible<StrTy&, basic_membuffer<typename StrTy::value_type>&>::value>>
void fmt_float_common(StrTy& s, std::uint64_t u64, fmt_opts fmt, unsigned bpm, int exp_max, locale_ref loc) {
    inline_basic_dynbuffer<typename StrTy::value_type> buf;
    fmt_float_common(buf, u64, fmt, bpm, exp_max, loc);
    s.append(buf.data(), buf.size());
}

template<typename StrTy, typename Ty>
void fmt_integer(StrTy& s, Ty val, fmt_opts fmt = {}, locale_ref loc = {}) {
    using UTy = typename std::make_unsigned<Ty>::type;
    using ReducedTy = std::conditional_t<(sizeof(UTy) <= sizeof(std::uint32_t)), std::uint32_t, std::uint64_t>;
    const bool is_signed = std::is_signed<Ty>::value;
    fmt_integer_common(s, static_cast<ReducedTy>(val), is_signed, fmt, loc);
}

template<typename StrTy, typename Ty>
void fmt_float(StrTy& s, Ty val, fmt_opts fmt = {}, locale_ref loc = {}) {
    fmt_float_common(s, fp_traits<Ty>::to_u64(val), fmt, fp_traits<Ty>::bits_per_mantissa, fp_traits<Ty>::exp_max, loc);
}

template<typename CharT>
UXS_EXPORT void fmt_character(basic_membuffer<CharT>& s, CharT val, fmt_opts fmt, locale_ref loc);

template<typename CharT>
UXS_EXPORT void fmt_string(basic_membuffer<CharT>& s, std::basic_string_view<CharT> val, fmt_opts fmt, locale_ref loc);

}  // namespace scvt

// --------------------------

template<typename Ty, typename CharT = char, typename = void>
struct formatter;

// --------------------------

template<typename Ty, typename CharT = char, typename = void>
struct from_string_impl;

template<typename Ty, typename CharT = char, typename = void>
struct to_string_impl;

template<typename Ty, typename CharT = char, typename = void>
struct convertible_from_string : std::false_type {};
template<typename Ty, typename CharT>
struct convertible_from_string<
    Ty, CharT,
    std::enable_if_t<std::is_same<decltype(from_string_impl<Ty, CharT>{}(nullptr, nullptr, std::declval<Ty&>())),
                                  const CharT*>::value>> : std::true_type {};

template<typename Ty, typename StrTy = membuffer, typename = void>
struct convertible_to_string : std::false_type {};
template<typename Ty, typename StrTy>
struct convertible_to_string<Ty, StrTy,
                             std::void_t<decltype(to_string_impl<Ty, typename StrTy::value_type>{}(
                                 std::declval<StrTy&>(), std::declval<const Ty&>(), {}))>> : std::true_type {};

// --------------------------

#define UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(ty, from_func, fmt_func) \
    template<typename CharT> \
    struct from_string_impl<ty, CharT> { \
        const CharT* operator()(const CharT* first, const CharT* last, ty& val) const noexcept { \
            auto t = from_func(first, last, last); \
            if (last != first) { val = t; } \
            return last; \
        } \
    }; \
    template<typename CharT> \
    struct to_string_impl<ty, CharT> { \
        template<typename StrTy> \
        void operator()(StrTy& s, ty val, fmt_opts fmt, locale_ref loc = {}) const { \
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
#undef UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER

template<typename CharT, typename Ty>
const CharT* from_basic_chars(const CharT* first, const CharT* last, Ty& val) {
    return from_string_impl<Ty, CharT>{}(first, last, val);
}

// ---- from_chars

template<typename Ty>
const char* from_chars(const char* first, const char* last, Ty& val) {
    return from_basic_chars(first, last, val);
}

template<typename Ty>
const wchar_t* from_wchars(const wchar_t* first, const wchar_t* last, Ty& val) {
    return from_basic_chars(first, last, val);
}

// ---- from_string

template<typename Ty, typename CharT, typename Traits>
std::size_t from_basic_string(std::basic_string_view<CharT, Traits> s, Ty& val) {
    return from_basic_chars(s.data(), s.data() + s.size(), val) - s.data();
}

template<typename Ty>
std::size_t from_string(std::string_view s, Ty& val) {
    return from_basic_string(s, val);
}

template<typename Ty>
std::size_t from_wstring(std::wstring_view s, Ty& val) {
    return from_basic_string(s, val);
}

template<typename Ty, typename CharT, typename Traits>
Ty from_basic_string(std::basic_string_view<CharT, Traits> s) {
    Ty result{};
    from_basic_string(s, result);
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

template<typename Ty, typename CharT, typename Traits, typename U>
Ty from_basic_string_or(std::basic_string_view<CharT, Traits> s, U&& default_value) {
    Ty result(std::forward<U>(default_value));
    from_basic_string(s, result);
    return result;
}

template<typename Ty, typename U>
Ty from_string_or(std::string_view s, U&& default_value) {
    return from_basic_string_or<Ty>(s, std::forward<U>(default_value));
}

template<typename Ty, typename U>
Ty from_wstring_or(std::wstring_view s, U&& default_value) {
    return from_basic_string_or<Ty>(s, std::forward<U>(default_value));
}

// ---- to_string

template<typename StrTy, typename Ty>
StrTy& to_basic_string(StrTy& s, const Ty& val, fmt_opts fmt = {}) {
    to_string_impl<Ty, typename StrTy::value_type>{}(s, val, fmt);
    return s;
}

template<typename StrTy, typename Ty>
StrTy& to_basic_string(StrTy& s, const std::locale& loc, const Ty& val, fmt_opts fmt) {
    to_string_impl<Ty, typename StrTy::value_type>{}(s, val, fmt, locale_ref{loc});
    return s;
}

template<typename Ty>
std::string to_string(const Ty& val, fmt_opts fmt = {}) {
    inline_dynbuffer buf;
    to_basic_string(buf, val, fmt);
    return std::string(buf.data(), buf.size());
}

template<typename Ty>
std::string to_string(const std::locale& loc, const Ty& val, fmt_opts fmt = {}) {
    inline_dynbuffer buf;
    to_basic_string(buf, loc, val, fmt);
    return std::string(buf.data(), buf.size());
}

template<typename Ty>
std::wstring to_wstring(const Ty& val, fmt_opts fmt = {}) {
    inline_wdynbuffer buf;
    to_basic_string(buf, val, fmt);
    return std::wstring(buf.data(), buf.size());
}

template<typename Ty>
std::wstring to_wstring(const std::locale& loc, const Ty& val, fmt_opts fmt = {}) {
    inline_wdynbuffer buf;
    to_basic_string(buf, loc, val, fmt);
    return std::wstring(buf.data(), buf.size());
}

// ---- to_chars

template<typename Ty>
char* to_chars(char* p, const Ty& val, fmt_opts fmt = {}) {
    membuffer buf(p);
    return to_basic_string(buf, val, fmt).endp();
}

template<typename Ty>
char* to_chars(char* p, const std::locale& loc, const Ty& val, fmt_opts fmt = {}) {
    membuffer buf(p);
    return to_basic_string(buf, loc, val, fmt).endp();
}

template<typename Ty>
wchar_t* to_wchars(wchar_t* p, const Ty& val, fmt_opts fmt = {}) {
    wmembuffer buf(p);
    return to_basic_string(buf, val, fmt).endp();
}

template<typename Ty>
wchar_t* to_wchars(wchar_t* p, const std::locale& loc, const Ty& val, fmt_opts fmt = {}) {
    wmembuffer buf(p);
    return to_basic_string(buf, loc, val, fmt).endp();
}

// ---- to_chars_n

template<typename CharT>
struct chars_to_n_result {
#if __cplusplus < 201703L
    chars_to_n_result(CharT* out, std::size_t size) : out(out), size(size) {}
#endif  // __cplusplus < 201703L
    CharT* out;
    std::size_t size;
};

template<typename Ty>
chars_to_n_result<char> to_chars_n(char* p, std::size_t n, const Ty& val, fmt_opts fmt = {}) {
    membuffer_with_size_tracker buf(p, n);
    return {to_basic_string(buf, val, fmt).endp(), buf.tracked_size()};
}

template<typename Ty>
chars_to_n_result<char> to_chars_n(char* p, std::size_t n, const std::locale& loc, const Ty& val, fmt_opts fmt = {}) {
    membuffer_with_size_tracker buf(p, n);
    return {to_basic_string(buf, loc, val, fmt).endp(), buf.tracked_size()};
}

template<typename Ty>
chars_to_n_result<wchar_t> to_wchars_n(wchar_t* p, std::size_t n, const Ty& val, fmt_opts fmt = {}) {
    wmembuffer_with_size_tracker buf(p, n);
    return {to_basic_string(buf, val, fmt).endp(), buf.tracked_size()};
}

template<typename Ty>
chars_to_n_result<wchar_t> to_wchars_n(wchar_t* p, std::size_t n, const std::locale& loc, const Ty& val,
                                       fmt_opts fmt = {}) {
    wmembuffer_with_size_tracker buf(p, n);
    return {to_basic_string(buf, loc, val, fmt).endp(), buf.tracked_size()};
}

}  // namespace uxs
