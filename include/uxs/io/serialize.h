#pragma once

#include "iobuf.h"

#include "uxs/string_util.h"

#include <exception>

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

template<typename CharT, typename Traits, typename Alloc>
bibuf& operator>>(bibuf& is, std::basic_string<CharT, Traits, Alloc>& s) {
    std::uint64_t sz = 0;
    if (!(is >> sz)) { return is; }
#if defined(__cpp_lib_string_resize_and_overwrite)
    std::exception_ptr eptr = nullptr;
    s.resize_and_overwrite(static_cast<std::size_t>(sz), [&is, &eptr](CharT* p, std::size_t sz) noexcept -> std::size_t {
        try {
            return is.read_with_endian(est::as_span(reinterpret_cast<std::uint8_t*>(p), sz * sizeof(CharT)),
                                       sizeof(CharT));

        } catch (...) {
            eptr = std::current_exception();
            return 0;
        }
    });
    if (eptr) { std::rethrow_exception(eptr); }
#else   // resize_and_overwrite
    s.resize(static_cast<std::size_t>(sz));
    is.read_with_endian(est::as_span(reinterpret_cast<std::uint8_t*>(&s[0]), s.size() * sizeof(CharT)), sizeof(CharT));
#endif  // resize_and_overwrite
    return is;
}

template<typename Ty>
std::enable_if_t<is_string_like<Ty>::value, biobuf&> operator<<(biobuf& os, const Ty& v) {
    const auto sv = to_string_view(v);
    os << static_cast<std::uint64_t>(sv.size());
    return os.write_with_endian(
        est::as_span(reinterpret_cast<const std::uint8_t*>(sv.data()), sv.size() * sizeof(sv[0])), sizeof(sv[0]));
}

}  // namespace uxs
