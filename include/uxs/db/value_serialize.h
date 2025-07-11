#pragma once

#if __cplusplus < 201703L
#    error Header file `db/value_serialize.h` requires C++17
#endif  // __cplusplus < 201703L

#include "value.h"

#include "uxs/io/serialize.h"

namespace uxs {

template<typename CharT, typename Alloc>
biobuf& operator<<(biobuf& os, const db::basic_value<CharT, Alloc>& v) {
    os << v.type();
    return v.visit([&os, &v](auto x) -> biobuf& {
        if constexpr (std::is_same_v<decltype(x), decltype(v.as_string_view())>) {
            os << static_cast<std::uint64_t>(x.size());
            return os.write_with_endian(
                est::as_span(reinterpret_cast<const std::uint8_t*>(x.data()), x.size() * sizeof(CharT)), sizeof(CharT));
        } else if constexpr (std::is_same_v<decltype(x), decltype(v.as_array())>) {
            os << static_cast<std::uint64_t>(x.size());
            for (const auto& el : x) { os << el; }
        } else if constexpr (std::is_same_v<decltype(x), decltype(v.as_record())>) {
            os << static_cast<std::uint64_t>(v.size());
            for (const auto& [key, value] : x) { os << key << value; }
        } else if constexpr (!std::is_same_v<decltype(x), std::nullptr_t>) {
            os << x;
        }
        return os;
    });
}

template<typename CharT, typename Alloc>
bibuf& operator>>(bibuf& is, db::basic_value<CharT, Alloc>& v) {
    auto type = db::dtype::null;
    is >> type;
    v = db::basic_value<CharT, Alloc>(type, [&is](auto type, auto& x) {
        if constexpr (std::is_same_v<decltype(type), db::string_variant_t>) {
            std::uint64_t sz = 0;
            if (!(is >> sz)) { return; }
            x.string_resize(static_cast<std::size_t>(sz));
            const auto s = x.as_string_span();
            is.read_with_endian(est::as_span(reinterpret_cast<std::uint8_t*>(s.data()), s.size() * sizeof(CharT)),
                                sizeof(CharT));
        } else if constexpr (std::is_same_v<decltype(type), db::array_variant_t>) {
            std::uint64_t sz = 0;
            if (!(is >> sz)) { return; }
            for (; sz; --sz) { is >> x.emplace_back(x.get_allocator()); }
        } else if constexpr (std::is_same_v<decltype(type), db::record_variant_t>) {
            std::uint64_t sz = 0;
            if (!(is >> sz)) { return; }
            for (std::string key; sz; --sz) { is >> key >> x.emplace(key, x.get_allocator()).value(); }
        } else {
            is >> x;
        }
    });
    return is;
}

}  // namespace uxs
