#pragma once

#include "uxs/stringcvt.h"

#include <array>

namespace uxs {

class UXS_EXPORT guid {
 public:
    guid() { data64_[0] = data64_[1] = 0; }
    explicit guid(const uint32_t* id) { set_data(id); }
    guid(uint32_t l, uint16_t w1, uint16_t w2, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6,
         uint8_t b7, uint8_t b8) {
        data32_[0] = l, data16_[2] = w1, data16_[3] = w2;
        data8_[8] = b1, data8_[9] = b2, data8_[10] = b3, data8_[11] = b4;
        data8_[12] = b5, data8_[13] = b6, data8_[14] = b7, data8_[15] = b8;
    }

    bool valid() const { return data64_[0] || data64_[1]; }
    guid make_xor(uint32_t a) const;

    uint8_t data8(unsigned i) const { return data8_[i]; }
    uint16_t data16(unsigned i) const { return data16_[i]; }
    uint32_t data32(unsigned i) const { return data32_[i]; }
    uint64_t data64(unsigned i) const { return data64_[i]; }

    uint8_t& data8(unsigned i) { return data8_[i]; }
    uint16_t& data16(unsigned i) { return data16_[i]; }
    uint32_t& data32(unsigned i) { return data32_[i]; }
    uint64_t& data64(unsigned i) { return data64_[i]; }

    void get_data(uint32_t* id) const {
        id[0] = data32_[0], id[1] = data32_[1], id[2] = data32_[2], id[3] = data32_[3];
    }

    void set_data(const uint32_t* id) {
        data32_[0] = id[0], data32_[1] = id[1], data32_[2] = id[2], data32_[3] = id[3];
    }

    bool operator==(const guid& id) const { return data64_[0] == id.data64_[0] && data64_[1] == id.data64_[1]; }
    bool operator!=(const guid& id) const { return !(*this == id); }

    bool operator<(const guid& id) const {
        return data64_[0] < id.data64_[0] || (data64_[0] == id.data64_[0] && data64_[1] < id.data64_[1]);
    }
    bool operator<=(const guid& id) const { return !(id < *this); }
    bool operator>(const guid& id) const { return id < *this; }
    bool operator>=(const guid& id) const { return !(*this < id); }

    template<typename CharT, typename Traits, typename Alloc>
    void to_per_byte_basic_string(std::basic_string<CharT, Traits, Alloc>& s) const;
    template<typename CharT, typename Traits>
    static guid from_per_byte_basic_string(std::basic_string_view<CharT, Traits> s);

    std::string to_per_byte_string() const {
        std::string s;
        to_per_byte_basic_string(s);
        return s;
    }
    static guid from_per_byte_string(std::string_view s) { return from_per_byte_basic_string(s); }

    std::wstring to_per_byte_wstring() const {
        std::wstring s;
        to_per_byte_basic_string(s);
        return s;
    }
    static guid from_per_byte_wstring(std::wstring_view s) { return from_per_byte_basic_string(s); }

    static guid generate();

 private:
    union {
        std::array<uint8_t, 16> data8_;
        std::array<uint16_t, 8> data16_;
        std::array<uint32_t, 4> data32_;
        std::array<uint64_t, 2> data64_;
    };
};

inline guid guid::make_xor(uint32_t a) const {
    guid id;
    id.data32_[0] = data32_[0] ^ a, id.data32_[1] = data32_[1] ^ a;
    id.data32_[2] = data32_[2] ^ a, id.data32_[3] = data32_[3] ^ a;
    return id;
}

template<typename CharT, typename Traits, typename Alloc>
void guid::to_per_byte_basic_string(std::basic_string<CharT, Traits, Alloc>& s) const {
    s.resize(32);
    auto* p = &s[0];
    for (uint8_t b : data8_) { to_hex(b, p, 2), p += 2; }
}

template<typename CharT, typename Traits>
/*static*/ guid guid::from_per_byte_basic_string(std::basic_string_view<CharT, Traits> s) {
    guid id;
    if (s.size() < 32) { return id; }
    const auto* p = s.data();
    for (uint8_t& b : id.data8_) { b = from_hex(p, 2), p += 2; }
    return id;
}

template<>
struct string_converter<guid> : string_converter_base<guid> {
    template<typename CharT, typename Traits>
    static size_t from_string(std::basic_string_view<CharT, Traits> s, guid& val) {
        const size_t len = 38;
        if (s.size() < len) { return 0; }
        const auto* p = s.data();
        val.data32(0) = from_hex(p + 1, 8);
        val.data16(2) = from_hex(p + 10, 4);
        val.data16(3) = from_hex(p + 15, 4);
        val.data8(8) = from_hex(p + 20, 2);
        val.data8(9) = from_hex(p + 22, 2);
        p += 25;
        for (unsigned i = 10; i < 16; ++i, p += 2) { val.data8(i) = from_hex(p, 2); }
        return len;
    }
    template<typename StrTy>
    static StrTy& to_string(StrTy& s, const guid& val, const fmt_opts& fmt) {
        const unsigned len = 38;
        typename StrTy::value_type buf[len];
        auto* p = buf;
        p[0] = '{', p[9] = '-', p[14] = '-', p[19] = '-', p[24] = '-', p[37] = '}';
        to_hex(val.data32(0), p + 1, 8);
        to_hex(val.data16(2), p + 10, 4);
        to_hex(val.data16(3), p + 15, 4);
        to_hex(val.data8(8), p + 20, 2);
        to_hex(val.data8(9), p + 22, 2);
        p += 25;
        for (unsigned i = 10; i < 16; ++i, p += 2) { to_hex(val.data8(i), p, 2); }
        const auto fn = [buf](StrTy& s) -> StrTy& { return s.append(buf, buf + len); };
        return fmt.width > len ? append_adjusted(s, fn, len, fmt) : fn(s);
    }
};

}  // namespace uxs

namespace std {
template<>
struct hash<uxs::guid> {
    size_t operator()(uxs::guid id) const {
        return hash<uint64_t>{}(id.data64(0)) ^ (hash<uint64_t>{}(id.data64(1)) << 1);
    }
};
}  // namespace std
