#pragma once

#include "chars.h"
#include "string_conv_base.h"

#include <functional>

namespace uxs {

struct guid {
    using data8_t = std::array<std::uint8_t, 16>;
    using data16_t = std::array<std::uint16_t, 8>;
    using data32_t = std::array<std::uint32_t, 4>;
    using data64_t = std::array<std::uint64_t, 2>;

    struct layout_t {
        std::uint32_t l;
        std::array<std::uint16_t, 2> w;
        std::array<std::uint8_t, 8> b;
    };

    static_assert(sizeof(data8_t) == sizeof(layout_t), "type size mismatch");
    static_assert(sizeof(data16_t) == sizeof(layout_t), "type size mismatch");
    static_assert(sizeof(data32_t) == sizeof(layout_t), "type size mismatch");
    static_assert(sizeof(data64_t) == sizeof(layout_t), "type size mismatch");

    union {
        layout_t layout;
        data8_t data8;
        data16_t data16;
        data32_t data32;
        data64_t data64;
    };

    UXS_CONSTEXPR guid() noexcept : data64{0, 0} {}
    explicit UXS_CONSTEXPR guid(data8_t b) : data8(b) {}
    explicit UXS_CONSTEXPR guid(data16_t w) : data16(w) {}
    explicit UXS_CONSTEXPR guid(data32_t l) : data32(l) {}
    explicit UXS_CONSTEXPR guid(data64_t q) : data64(q) {}
    UXS_CONSTEXPR guid(std::uint32_t l, std::uint16_t w1, std::uint16_t w2, std::uint8_t b1, std::uint8_t b2,
                       std::uint8_t b3, std::uint8_t b4, std::uint8_t b5, std::uint8_t b6, std::uint8_t b7,
                       std::uint8_t b8) noexcept
        : layout{l, {w1, w2}, {b1, b2, b3, b4, b5, b6, b7, b8}} {}

    UXS_CONSTEXPR bool valid() const noexcept { return data64[0] || data64[1]; }

    friend UXS_CONSTEXPR bool operator==(guid lhs, guid rhs) noexcept {
        return lhs.data64[0] == rhs.data64[0] && lhs.data64[1] == rhs.data64[1];
    }
    friend UXS_CONSTEXPR bool operator!=(guid lhs, guid rhs) noexcept { return !(lhs == rhs); }

    template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
    UXS_CONSTEXPR from_string_result from_per_byte_string(const StrLikeTy& s) noexcept;

    template<typename StrTy>
    void to_per_byte_string_append(StrTy& out) const;

    template<typename CharT = char>
    std::basic_string<CharT> to_per_byte_string() const {
        std::basic_string<CharT> s;
        to_per_byte_string_append(s);
        return s;
    }

    UXS_EXPORT static guid generate();
};

namespace detail {
template<typename CharT>
UXS_CONSTEXPR sconv_errc from_hex(const CharT* p, std::uint8_t& b) {
    const unsigned dig1 = dig_v(p[0]);
    const unsigned dig2 = dig_v(p[1]);
    if (dig1 >= 16 || dig2 >= 16) { return sconv_errc::invalid; }
    b = static_cast<std::uint8_t>((dig1 << 4) | dig2);
    return sconv_errc::ok;
}
template<typename CharT>
UXS_CONSTEXPR void to_hex(std::uint8_t b, CharT* p, const char* digs) {
    p[0] = digs[(b >> 4) & 0xf];
    p[1] = digs[b & 0xf];
}
}  // namespace detail

template<typename StrLikeTy, typename>
UXS_CONSTEXPR from_string_result guid::from_per_byte_string(const StrLikeTy& s) noexcept {
    const auto sv = to_string_view(s);
    const std::size_t len = 32;
    if (sv.size() < len) { return {0, sconv_errc::invalid}; }
    const auto* p = sv.data();
    data8_t bytes{};
    for (std::uint8_t& b : bytes) {
        if (detail::from_hex(p, b) != sconv_errc::ok) { return {0, sconv_errc::invalid}; }
        p += 2;
    }
    *this = guid(bytes);
    return {len, sconv_errc::ok};
}

template<typename StrTy>
void guid::to_per_byte_string_append(StrTy& out) const {
    std::array<typename StrTy::value_type, 32> buf;
    auto* p = buf.data();
    for (const std::uint8_t b : data8) { detail::to_hex(b, p, "0123456789ABCDEF"), p += 2; }
    out.append(buf.data(), p);
}

template<typename CharT>
struct from_string_impl<guid, CharT> {
    UXS_CONSTEXPR from_chars_result<CharT> operator()(const CharT* first, const CharT* last, guid& val) const noexcept {
        if (first == last) { return {first, sconv_errc::empty}; }
        const std::size_t len = 38;
        if (static_cast<std::size_t>(last - first) < len) { return {first, sconv_errc::invalid}; }
        if (first[0] != '{' || first[9] != '-' || first[14] != '-' || first[19] != '-' || first[24] != '-' ||
            first[37] != '}') {
            return {first, sconv_errc::invalid};
        }
        unsigned n = 0;
        guid::data8_t data8{};
        UXS_CONSTEXPR_DATA std::uint8_t byte_pos[16] = {7, 5, 3, 1, 12, 10, 17, 15, 20, 22, 25, 27, 29, 31, 33, 35};
        for (std::uint8_t& b : data8) {
            if (detail::from_hex(&first[byte_pos[n++]], b) != sconv_errc::ok) { return {first, sconv_errc::invalid}; }
        }
        val = guid(data8);
        return {first + len, sconv_errc::ok};
    }
};

template<typename CharT>
struct to_string_impl<guid, CharT> {
    template<typename StrTy>
    void operator()(StrTy& out, guid val, fmt_opts fmt = {}) const {
        const unsigned len = 38;
        const char* digs = !!(fmt.flags & fmt_flags::uppercase) ? "0123456789ABCDEF" : "0123456789abcdef";
        std::array<typename StrTy::value_type, len> buf;
        buf[0] = '{', buf[9] = '-', buf[14] = '-', buf[19] = '-', buf[24] = '-', buf[37] = '}';
        unsigned n = 0;
        UXS_CONSTEXPR_DATA std::uint8_t byte_pos[16] = {7, 5, 3, 1, 12, 10, 17, 15, 20, 22, 25, 27, 29, 31, 33, 35};
        for (const std::uint8_t b : val.data8) { detail::to_hex(b, &buf[byte_pos[n++]], digs); }
        const auto fn = [&buf](StrTy& out) { out.append(buf.data(), buf.size()); };
        fmt.width > len ? append_adjusted(out, fn, len, fmt) : fn(out);
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
        to_string_append(ctx.out(), val, opts);
    }
};

}  // namespace uxs

namespace std {
template<>
struct hash<uxs::guid> {
    std::size_t operator()(uxs::guid val) const {
        return hash<std::uint64_t>{}(val.data64[0]) ^ (hash<std::uint64_t>{}(val.data64[1]) << 1);
    }
};
}  // namespace std
