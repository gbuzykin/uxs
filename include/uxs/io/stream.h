#pragma once

#include "iobuf.h"

#include "uxs/string_view.h"

#include <string>

namespace uxs {

template<typename Ty>
std::enable_if_t<std::is_arithmetic<Ty>::value, u8ibuf&> operator>>(u8ibuf& is, Ty& v) {
    is.read_with_endian(uxs::as_span(reinterpret_cast<std::uint8_t*>(&v), sizeof(Ty)), sizeof(Ty));
    return is;
}

inline u8ibuf& operator>>(u8ibuf& is, bool& b) {
    std::uint8_t v;
    is.read(uxs::as_span(&v, 1));
    if (is) { b = (v != 0); }
    return is;
}

template<typename CharT>
u8ibuf& operator>>(u8ibuf& is, std::basic_string<CharT>& s) {
    std::uint64_t sz = 0;
    is >> sz;
    s.resize(static_cast<std::size_t>(sz));
    is.read_with_endian(uxs::as_span(reinterpret_cast<std::uint8_t*>(&s[0]), s.size() * sizeof(CharT)), sizeof(CharT));
    return is;
}

template<typename Ty>
std::enable_if_t<std::is_arithmetic<Ty>::value, u8iobuf&> operator<<(u8iobuf& os, const Ty& v) {
    return os.write_with_endian(uxs::as_span(reinterpret_cast<const std::uint8_t*>(&v), sizeof(Ty)), sizeof(Ty));
}

inline u8iobuf& operator<<(u8iobuf& os, bool b) {
    std::uint8_t v = (b ? 1 : 0);
    return os.write(uxs::as_span(&v, 1));
}

template<typename CharT>
u8iobuf& operator<<(u8iobuf& os, const std::basic_string<CharT>& s) {
    os << static_cast<std::uint64_t>(s.size());
    return os.write_with_endian(uxs::as_span(reinterpret_cast<const std::uint8_t*>(s.data()), s.size() * sizeof(CharT)),
                                sizeof(CharT));
}

template<typename CharT>
u8iobuf& operator<<(u8iobuf& os, std::basic_string_view<CharT> s) {
    os << static_cast<std::uint64_t>(s.size());
    return os.write_with_endian(uxs::as_span(reinterpret_cast<const std::uint8_t*>(s.data()), s.size() * sizeof(CharT)),
                                sizeof(CharT));
}

template<typename Enum>
class enum_reader {
 public:
    explicit enum_reader(Enum& e) : e_(e) {}

 private:
    Enum& e_;

    friend u8ibuf& operator>>(u8ibuf& is, const enum_reader& v) {
        typename std::underlying_type<Enum>::type int_val;
        if (is >> int_val) { v.e_ = static_cast<Enum>(int_val); }
        return is;
    }
};

template<typename Enum, typename = std::enable_if_t<std::is_enum<Enum>::value>>
enum_reader<Enum> read_enum(Enum& e) {
    return enum_reader<Enum>(e);
}

template<typename Enum>
class enum_writer {
 public:
    explicit enum_writer(const Enum& e) : e_(e) {}

 private:
    const Enum& e_;

    friend u8iobuf& operator<<(u8iobuf& os, const enum_writer& v) {
        return os << static_cast<typename std::underlying_type<Enum>::type>(v.e_);
    }
};

template<typename Enum, typename = std::enable_if_t<std::is_enum<Enum>::value>>
enum_writer<Enum> write_enum(const Enum& e) {
    return enum_writer<Enum>(e);
}

}  // namespace uxs
