#pragma once

#if __cplusplus < 201703L
#    error Header file `db/value_serialize.h` requires C++17
#endif  // __cplusplus < 201703L

#include "value.h"

#include "uxs/io/serialize.h"
#include "uxs/membuffer.h"

namespace uxs {

template<typename CharT, typename Alloc>
biobuf& operator<<(biobuf& os, const db::basic_value<CharT, Alloc>& v) {
    using value_ty = const db::basic_value<CharT, Alloc>;
    os << v.type();
    return v.visit([&os](auto x) -> biobuf& {
        if constexpr (std::is_same_v<decltype(x), decltype(std::declval<value_ty>().as_string_view())>) {
            os << static_cast<std::uint64_t>(x.size());
            return os.write_with_endian(
                est::as_span(reinterpret_cast<const std::uint8_t*>(x.data()), x.size() * sizeof(CharT)), sizeof(CharT));
        } else if constexpr (std::is_same_v<decltype(x), decltype(std::declval<value_ty>().as_array())>) {
            os << static_cast<std::uint64_t>(x.size());
            for (const auto& el : x) { os << el; }
        } else if constexpr (std::is_same_v<decltype(x), decltype(std::declval<value_ty>().as_record())>) {
            os << static_cast<std::uint64_t>(x.size());
            for (const auto& [key, value] : x) {
                os << static_cast<std::uint64_t>(key.size());
                os.write_with_endian(
                    est::as_span(reinterpret_cast<const std::uint8_t*>(key.data()), key.size() * sizeof(CharT)),
                    sizeof(CharT));
                os << value;
            }
        } else if constexpr (!std::is_same_v<decltype(x), std::nullptr_t>) {
            os << x;
        }
        return os;
    });
}

namespace detail {
template<typename CharT, typename Alloc>
void deserialize(bibuf& is, db::basic_value<CharT, Alloc>& v, basic_dynbuffer<CharT>& key_buf) {
    auto type = db::dtype::null;
    is >> type;
    v = db::basic_value<CharT, Alloc>(type, [&is, &key_buf](auto type, auto& x) {
        if constexpr (std::is_same_v<decltype(type), db::string_tag_t>) {
            std::uint64_t sz = 0;
            if (!(is >> sz)) { return; }
            x.append_string(static_cast<std::size_t>(sz), [&is](est::span<CharT> s) {
                is.read_with_endian(est::as_span(reinterpret_cast<std::uint8_t*>(s.data()), s.size() * sizeof(CharT)),
                                    sizeof(CharT));
            });
        } else if constexpr (std::is_same_v<decltype(type), db::array_tag_t>) {
            std::uint64_t sz = 0;
            if (!(is >> sz)) { return; }
            x.reserve(db::array_tag, static_cast<std::size_t>(sz));
            for (; sz && is; --sz) { deserialize(is, x.emplace_back(x.get_allocator()), key_buf); }
        } else if constexpr (std::is_same_v<decltype(type), db::record_tag_t>) {
            std::uint64_t sz = 0;
            if (!(is >> sz)) { return; }
            x.reserve(db::record_tag, static_cast<std::size_t>(sz));
            for (; sz; --sz) {
                std::uint64_t key_sz = 0;
                if (!(is >> key_sz)) { return; }
                key_buf.reserve(static_cast<std::size_t>(key_sz));
                is.read_with_endian(
                    est::as_span(reinterpret_cast<std::uint8_t*>(key_buf.data()), key_sz * sizeof(CharT)),
                    sizeof(CharT));
                if (!is) { return; }
                std::basic_string_view<CharT> key(key_buf.data(), key_sz);
                deserialize(is, x.emplace(key, x.get_allocator()).value(), key_buf);
            }
        } else {
            is >> x;
        }
    });
}
}  // namespace detail

template<typename CharT, typename Alloc>
bibuf& operator>>(bibuf& is, db::basic_value<CharT, Alloc>& v) {
    basic_inline_dynbuffer<CharT> key_buf;
    detail::deserialize(is, v, key_buf);
    return is;
}

}  // namespace uxs
