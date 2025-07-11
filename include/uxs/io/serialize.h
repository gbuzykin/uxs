#pragma once

#include "iobuf.h"

#include "uxs/string_view.h"

#include <string>

namespace uxs {

template<typename Ty>
std::enable_if_t<std::is_arithmetic<Ty>::value, bibuf&> operator>>(bibuf& is, Ty& v) {
    is.read_with_endian(est::as_span(reinterpret_cast<std::uint8_t*>(&v), sizeof(Ty)), sizeof(Ty));
    return is;
}

template<typename Ty>
std::enable_if_t<std::is_arithmetic<Ty>::value, biobuf&> operator<<(biobuf& os, const Ty& v) {
    return os.write_with_endian(est::as_span(reinterpret_cast<const std::uint8_t*>(&v), sizeof(Ty)), sizeof(Ty));
}

inline bibuf& operator>>(bibuf& is, bool& b) {
    std::uint8_t v = 0;
    if (is >> v) { b = v != 0; }
    return is;
}

inline biobuf& operator<<(biobuf& os, bool b) { return os << static_cast<std::uint8_t>(b ? 1 : 0); }

template<typename Ty>
std::enable_if_t<std::is_enum<Ty>::value, bibuf&> operator>>(bibuf& is, Ty& v) {
    return is >> reinterpret_cast<typename std::underlying_type<Ty>::type&>(v);
}

template<typename Ty>
std::enable_if_t<std::is_enum<Ty>::value, biobuf&> operator<<(biobuf& os, const Ty& v) {
    return os << static_cast<typename std::underlying_type<Ty>::type>(v);
}

template<typename CharT>
bibuf& operator>>(bibuf& is, std::basic_string<CharT>& s) {
    std::uint64_t sz = 0;
    if (!(is >> sz)) { return is; }
    s.resize(static_cast<std::size_t>(sz));
    is.read_with_endian(est::as_span(reinterpret_cast<std::uint8_t*>(&s[0]), s.size() * sizeof(CharT)), sizeof(CharT));
    return is;
}

template<typename CharT>
biobuf& operator<<(biobuf& os, std::basic_string_view<CharT> s) {
    os << static_cast<std::uint64_t>(s.size());
    return os.write_with_endian(est::as_span(reinterpret_cast<const std::uint8_t*>(s.data()), s.size() * sizeof(CharT)),
                                sizeof(CharT));
}

template<typename CharT>
biobuf& operator<<(biobuf& os, const std::basic_string<CharT>& s) {
    return os << std::basic_string_view<CharT>{s};
}

}  // namespace uxs
