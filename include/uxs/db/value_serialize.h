#pragma once

#if __cplusplus < 201703L
#    error Header file `db/value_serialize.h` requires C++17
#endif  // __cplusplus < 201703L

#include "value.h"

#include "uxs/io/serialize.h"

namespace uxs {

template<typename CharT, typename Alloc>
biobuf& operator<<(biobuf& os, const db::basic_value<CharT, Alloc>& v) {
    using ValueTy = const db::basic_value<CharT, Alloc>;
    os << v.type();
    return v.visit([&os](auto x) -> biobuf& {
        if constexpr (std::is_same_v<decltype(x), decltype(std::declval<ValueTy>().as_string_view())>) {
            os << static_cast<std::uint64_t>(x.size());
            return os.write_with_endian(
                est::as_span(reinterpret_cast<const std::uint8_t*>(x.data()), x.size() * sizeof(CharT)), sizeof(CharT));
        } else if constexpr (std::is_same_v<decltype(x), decltype(std::declval<ValueTy>().as_array())>) {
            os << static_cast<std::uint64_t>(x.size());
            for (const auto& el : x) { os << el; }
        } else if constexpr (std::is_same_v<decltype(x), decltype(std::declval<ValueTy>().as_record())>) {
            os << static_cast<std::uint64_t>(x.size());
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
        if constexpr (std::is_same_v<decltype(type), db::string_tag_t>) {
            std::uint64_t sz = 0;
            if (!(is >> sz)) { return; }
            x.resize_and_overwrite(db::string_tag, static_cast<std::size_t>(sz), [&is](CharT* p, std::size_t count) {
                is.read_with_endian(est::as_span(reinterpret_cast<std::uint8_t*>(p), count * sizeof(CharT)),
                                    sizeof(CharT));
            });
        } else if constexpr (std::is_same_v<decltype(type), db::array_tag_t>) {
            std::uint64_t sz = 0;
            if (!(is >> sz)) { return; }
            x.reserve(static_cast<std::size_t>(sz));
            for (; sz; --sz) { is >> x.emplace_back(x.get_allocator()); }
        } else if constexpr (std::is_same_v<decltype(type), db::record_tag_t>) {
            std::uint64_t sz = 0;
            if (!(is >> sz)) { return; }
            x.reserve(db::record_tag, static_cast<std::size_t>(sz));
            for (std::string key; sz; --sz) { is >> key >> x.emplace(key, x.get_allocator()).value(); }
        } else {
            is >> x;
        }
    });
    return is;
}

}  // namespace uxs
