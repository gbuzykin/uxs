#pragma once

#include "string_cvt.h"

#include <functional>

namespace uxs {

class guid {
 public:
    guid() noexcept { data64_[0] = data64_[1] = 0; }
    explicit guid(const std::uint32_t* id) noexcept { set_data(id); }
    guid(std::uint32_t l, std::uint16_t w1, std::uint16_t w2, std::uint8_t b1, std::uint8_t b2, std::uint8_t b3,
         std::uint8_t b4, std::uint8_t b5, std::uint8_t b6, std::uint8_t b7, std::uint8_t b8) noexcept {
        data32_[0] = l, data16_[2] = w1, data16_[3] = w2;
        data8_[8] = b1, data8_[9] = b2, data8_[10] = b3, data8_[11] = b4;
        data8_[12] = b5, data8_[13] = b6, data8_[14] = b7, data8_[15] = b8;
    }

    bool valid() const noexcept { return data64_[0] || data64_[1]; }
    guid make_xor(std::uint32_t a) const noexcept;

    std::uint8_t data8(unsigned i) const noexcept { return data8_[i]; }
    std::uint16_t data16(unsigned i) const noexcept { return data16_[i]; }
    std::uint32_t data32(unsigned i) const noexcept { return data32_[i]; }
    std::uint64_t data64(unsigned i) const noexcept { return data64_[i]; }

    std::uint8_t& data8(unsigned i) noexcept { return data8_[i]; }
    std::uint16_t& data16(unsigned i) noexcept { return data16_[i]; }
    std::uint32_t& data32(unsigned i) noexcept { return data32_[i]; }
    std::uint64_t& data64(unsigned i) noexcept { return data64_[i]; }

    void get_data(std::uint32_t* id) const noexcept {
        id[0] = data32_[0], id[1] = data32_[1], id[2] = data32_[2], id[3] = data32_[3];
    }

    void set_data(const std::uint32_t* id) noexcept {
        data32_[0] = id[0], data32_[1] = id[1], data32_[2] = id[2], data32_[3] = id[3];
    }

    bool operator==(const guid& id) const noexcept {
        return data64_[0] == id.data64_[0] && data64_[1] == id.data64_[1];
    }
    bool operator!=(const guid& id) const noexcept { return !(*this == id); }

    bool operator<(const guid& id) const noexcept {
        return data64_[0] < id.data64_[0] || (data64_[0] == id.data64_[0] && data64_[1] < id.data64_[1]);
    }
    bool operator<=(const guid& id) const noexcept { return !(id < *this); }
    bool operator>(const guid& id) const noexcept { return id < *this; }
    bool operator>=(const guid& id) const noexcept { return !(*this < id); }

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

 private:
    union {
        std::array<std::uint8_t, 16> data8_;
        std::array<std::uint16_t, 8> data16_;
        std::array<std::uint32_t, 4> data32_;
        std::array<std::uint64_t, 2> data64_;
    };
};

inline guid guid::make_xor(std::uint32_t a) const noexcept {
    guid id;
    id.data32_[0] = data32_[0] ^ a, id.data32_[1] = data32_[1] ^ a;
    id.data32_[2] = data32_[2] ^ a, id.data32_[3] = data32_[3] ^ a;
    return id;
}

template<typename CharT, typename Traits, typename Alloc>
void guid::to_per_byte_basic_string(std::basic_string<CharT, Traits, Alloc>& s) const {
    s.resize(32);
    auto* p = &s[0];
    for (const std::uint8_t b : data8_) { to_hex(b, p, 2, true), p += 2; }
}

template<typename CharT, typename Traits>
/*static*/ guid guid::from_per_byte_basic_string(std::basic_string_view<CharT, Traits> s) noexcept {
    guid id;
    if (s.size() < 32) { return id; }
    const auto* p = s.data();
    for (std::uint8_t& b : id.data8_) { b = from_hex(p, 2), p += 2; }
    return id;
}

template<typename CharT>
struct from_string_impl<guid, CharT> {
    const CharT* operator()(const CharT* first, const CharT* last, guid& val) const noexcept {
        const std::size_t len = 38;
        if (static_cast<std::size_t>(last - first) < len) { return 0; }
        const auto* p = first;
        val.data32(0) = from_hex(p + 1, 8);
        val.data16(2) = from_hex(p + 10, 4);
        val.data16(3) = from_hex(p + 15, 4);
        val.data8(8) = from_hex(p + 20, 2);
        val.data8(9) = from_hex(p + 22, 2);
        p += 25;
        for (unsigned i = 10; i < 16; ++i, p += 2) { val.data8(i) = from_hex(p, 2); }
        return first + len;
    }
};

template<typename CharT>
struct to_string_impl<guid, CharT> {
    template<typename StrTy>
    void operator()(StrTy& s, const guid& val, fmt_opts fmt) const {
        const unsigned len = 38;
        const bool upper = !!(fmt.flags & fmt_flags::uppercase);
        std::array<typename StrTy::value_type, len> buf;
        auto* p = buf.data();
        p[0] = '{', p[9] = '-', p[14] = '-', p[19] = '-', p[24] = '-', p[37] = '}';
        to_hex(val.data32(0), p + 1, 8, upper);
        to_hex(val.data16(2), p + 10, 4, upper);
        to_hex(val.data16(3), p + 15, 4, upper);
        to_hex(val.data8(8), p + 20, 2, upper);
        to_hex(val.data8(9), p + 22, 2, upper);
        p += 25;
        for (unsigned i = 10; i < 16; ++i, p += 2) { to_hex(val.data8(i), p, 2, upper); }
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
    void format(FmtCtx& ctx, const guid& val) const {
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
        return hash<std::uint64_t>{}(id.data64(0)) ^ (hash<std::uint64_t>{}(id.data64(1)) << 1);
    }
};
}  // namespace std
