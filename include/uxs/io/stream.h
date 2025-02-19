#pragma once

#include "iobuf.h"

#include "uxs/string_view.h"

#include <string>

namespace uxs {

template<typename Ty>
std::enable_if_t<std::is_arithmetic<Ty>::value, u8ibuf&> operator>>(u8ibuf& is, Ty& v) {
    is.read_with_endian(est::as_span(reinterpret_cast<std::uint8_t*>(&v), sizeof(Ty)), sizeof(Ty));
    return is;
}

template<typename Ty>
std::enable_if_t<std::is_arithmetic<Ty>::value, u8iobuf&> operator<<(u8iobuf& os, const Ty& v) {
    return os.write_with_endian(est::as_span(reinterpret_cast<const std::uint8_t*>(&v), sizeof(Ty)), sizeof(Ty));
}

inline u8ibuf& operator>>(u8ibuf& is, bool& b) {
    std::uint8_t v = 0;
    if (is >> v) { b = v != 0; }
    return is;
}

inline u8iobuf& operator<<(u8iobuf& os, bool b) { return os << static_cast<std::uint8_t>(b ? 1 : 0); }

template<typename Ty>
std::enable_if_t<std::is_enum<Ty>::value, u8ibuf&> operator>>(u8ibuf& is, Ty& v) {
    return is >> reinterpret_cast<typename std::underlying_type<Ty>::type&>(v);
}

template<typename Ty>
std::enable_if_t<std::is_enum<Ty>::value, u8iobuf&> operator<<(u8iobuf& os, const Ty& v) {
    return os << static_cast<typename std::underlying_type<Ty>::type>(v);
}

template<typename CharT>
u8ibuf& operator>>(u8ibuf& is, std::basic_string<CharT>& s) {
    std::uint64_t sz = 0;
    if (!(is >> sz)) { return is; }
    s.resize(static_cast<std::size_t>(sz));
    is.read_with_endian(est::as_span(reinterpret_cast<std::uint8_t*>(&s[0]), s.size() * sizeof(CharT)), sizeof(CharT));
    return is;
}

template<typename CharT>
u8iobuf& operator<<(u8iobuf& os, const std::basic_string<CharT>& s) {
    os << static_cast<std::uint64_t>(s.size());
    return os.write_with_endian(est::as_span(reinterpret_cast<const std::uint8_t*>(s.data()), s.size() * sizeof(CharT)),
                                sizeof(CharT));
}

template<typename CharT>
u8iobuf& operator<<(u8iobuf& os, std::basic_string_view<CharT> s) {
    os << static_cast<std::uint64_t>(s.size());
    return os.write_with_endian(est::as_span(reinterpret_cast<const std::uint8_t*>(s.data()), s.size() * sizeof(CharT)),
                                sizeof(CharT));
}

}  // namespace uxs
