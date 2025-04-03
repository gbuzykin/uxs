#pragma once

#include "string_cvt.h"

#include <functional>

namespace uxs {

struct guid {
    using data8_t = std::array<std::uint8_t, 16>;
    using data16_t = std::array<std::uint16_t, 8>;
    using data32_t = std::array<std::uint32_t, 4>;

    struct data64_t : std::array<std::uint64_t, 2> {
        bool valid() const noexcept { return (*this)[0] || (*this)[1]; }
        friend bool operator==(data64_t lhs, data64_t rhs) noexcept { return lhs[0] == rhs[0] && lhs[1] == rhs[1]; }
        friend bool operator<(data64_t lhs, data64_t rhs) noexcept {
            return lhs[0] < rhs[0] || (lhs[0] == rhs[0] && lhs[1] < rhs[1]);
        }
    };

    struct layout_t {
        layout_t() noexcept = default;
        UXS_CONSTEXPR layout_t(std::uint32_t l, std::uint16_t w1, std::uint16_t w2, std::uint8_t b1, std::uint8_t b2,
                               std::uint8_t b3, std::uint8_t b4, std::uint8_t b5, std::uint8_t b6, std::uint8_t b7,
                               std::uint8_t b8) noexcept
            : l{l}, w{w1, w2}, b{b1, b2, b3, b4, b5, b6, b7, b8} {}
        std::uint32_t l;
        std::array<std::uint16_t, 2> w;
        std::array<std::uint8_t, 8> b;
    };

    static_assert(sizeof(data8_t) == sizeof(data64_t), "type size mismatch");
    static_assert(sizeof(data16_t) == sizeof(data64_t), "type size mismatch");
    static_assert(sizeof(data32_t) == sizeof(data64_t), "type size mismatch");
    static_assert(sizeof(layout_t) == sizeof(data64_t), "type size mismatch");

    layout_t data;

    UXS_CONSTEXPR guid() noexcept : data{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} {}
    UXS_CONSTEXPR guid(std::uint32_t l, std::uint16_t w1, std::uint16_t w2, std::uint8_t b1, std::uint8_t b2,
                       std::uint8_t b3, std::uint8_t b4, std::uint8_t b5, std::uint8_t b6, std::uint8_t b7,
                       std::uint8_t b8) noexcept
        : data{l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8} {}

    explicit guid(data8_t b) noexcept { std::memcpy(&data, &b, sizeof(data)); }
    explicit guid(data16_t w) noexcept { std::memcpy(&data, &w, sizeof(data)); }
    explicit guid(data32_t l) noexcept { std::memcpy(&data, &l, sizeof(data)); }
    explicit guid(data64_t q) noexcept { std::memcpy(&data, &q, sizeof(data)); }

    bool valid() const noexcept { return data64().valid(); }

    data8_t data8() const {
        data8_t b;
        std::memcpy(&b, &data, sizeof(data));
        return b;
    }
    data16_t data16() const {
        data16_t w;
        std::memcpy(&w, &data, sizeof(data));
        return w;
    }
    data32_t data32() const {
        data32_t l;
        std::memcpy(&l, &data, sizeof(data));
        return l;
    }
    data64_t data64() const {
        data64_t q;
        std::memcpy(&q, &data, sizeof(data));
        return q;
    }

    friend bool operator==(guid lhs, guid rhs) noexcept { return lhs.data64() == rhs.data64(); }
    friend bool operator!=(guid lhs, guid rhs) noexcept { return !(lhs.data64() == rhs.data64()); }

    friend bool operator<(guid lhs, guid rhs) noexcept { return lhs.data64() < rhs.data64(); }
    friend bool operator<=(guid lhs, guid rhs) noexcept { return !(rhs.data64() < lhs.data64()); }
    friend bool operator>(guid lhs, guid rhs) noexcept { return rhs.data64() < lhs.data64(); }
    friend bool operator>=(guid lhs, guid rhs) noexcept { return !(lhs.data64() < rhs.data64()); }

    template<typename CharT, typename Traits, typename Alloc>
    void to_per_byte_basic_string(std::basic_string<CharT, Traits, Alloc>& s) const;
    template<typename CharT, typename Traits>
    static guid from_per_byte_basic_string(std::basic_string_view<CharT, Traits> s) noexcept;

    std::string to_per_byte_string() const {
        std::string s;
        to_per_byte_basic_string(s);
        return s;
    }
    static guid from_per_byte_string(std::string_view s) noexcept { return from_per_byte_basic_string(s); }

    std::wstring to_per_byte_wstring() const {
        std::wstring s;
        to_per_byte_basic_string(s);
        return s;
    }
    static guid from_per_byte_wstring(std::wstring_view s) noexcept { return from_per_byte_basic_string(s); }

    UXS_EXPORT static guid generate();
};

template<typename CharT, typename Traits, typename Alloc>
void guid::to_per_byte_basic_string(std::basic_string<CharT, Traits, Alloc>& s) const {
    s.resize(32);
    auto* p = &s[0];
    for (const std::uint8_t b : data8()) { to_hex(b, p, 2, true), p += 2; }
}

template<typename CharT, typename Traits>
/*static*/ guid guid::from_per_byte_basic_string(std::basic_string_view<CharT, Traits> s) noexcept {
    if (s.size() < 32) { return guid{}; }
    const auto* p = s.data();
    guid::data8_t data;
    for (std::uint8_t& b : data) { b = from_hex(p, 2), p += 2; }
    return guid{data};
}

template<typename CharT>
struct from_string_impl<guid, CharT> {
    const CharT* operator()(const CharT* first, const CharT* last, guid& val) const noexcept {
        const std::size_t len = 38;
        if (static_cast<std::size_t>(last - first) < len) { return 0; }
        const auto* p = first;
        val.data.l = from_hex(p + 1, 8);
        val.data.w[0] = from_hex(p + 10, 4);
        val.data.w[1] = from_hex(p + 15, 4);
        val.data.b[0] = from_hex(p + 20, 2);
        val.data.b[1] = from_hex(p + 22, 2);
        p += 25;
        for (unsigned i = 2; i < 8; ++i, p += 2) { val.data.b[i] = from_hex(p, 2); }
        return first + len;
    }
};

template<typename CharT>
struct to_string_impl<guid, CharT> {
    template<typename StrTy>
    void operator()(StrTy& s, guid val, fmt_opts fmt) const {
        const unsigned len = 38;
        const bool upper = !!(fmt.flags & fmt_flags::uppercase);
        std::array<typename StrTy::value_type, len> buf;
        auto* p = buf.data();
        p[0] = '{', p[9] = '-', p[14] = '-', p[19] = '-', p[24] = '-', p[37] = '}';
        to_hex(val.data.l, p + 1, 8, upper);
        to_hex(val.data.w[0], p + 10, 4, upper);
        to_hex(val.data.w[1], p + 15, 4, upper);
        to_hex(val.data.b[0], p + 20, 2, upper);
        to_hex(val.data.b[1], p + 22, 2, upper);
        p += 25;
        for (unsigned i = 2; i < 8; ++i, p += 2) { to_hex(val.data.b[i], p, 2, upper); }
        const auto fn = [&buf](StrTy& s) { s.append(buf.data(), buf.size()); };
        fmt.width > len ? append_adjusted(s, fn, len, fmt) : fn(s);
    }
};

template<typename CharT>
struct formatter<guid, CharT> {
 private:
    fmt_opts opts_;
    std::size_t width_arg_id_ = unspecified_size;

 public:
    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        if (it == ctx.end() || *it != ':') { return it; }
        std::size_t dummy_id = unspecified_size;
        it = ParseCtx::parse_standard(ctx, it + 1, opts_, width_arg_id_, dummy_id);
        if (opts_.prec >= 0 || !!(opts_.flags & ~fmt_flags::adjust_field)) { ParseCtx::syntax_error(); }
        if (it == ctx.end() || (*it != 'X' && *it != 'x')) { return it; }
        if (*it == 'X') { opts_.flags |= fmt_flags::uppercase; }
        return it + 1;
    }
    template<typename FmtCtx>
    void format(FmtCtx& ctx, guid val) const {
        fmt_opts opts = opts_;
        if (width_arg_id_ != unspecified_size) {
            opts.width = ctx.arg(width_arg_id_).template get_unsigned<decltype(opts.width)>();
        }
        to_string_impl<guid, CharT>{}(ctx.out(), val, opts);
    }
};

}  // namespace uxs

namespace std {
template<>
struct hash<uxs::guid> {
    std::size_t operator()(uxs::guid id) const {
        const auto q = id.data64();
        return hash<std::uint64_t>{}(q[0]) ^ (hash<std::uint64_t>{}(q[1]) << 1);
    }
};
}  // namespace std
