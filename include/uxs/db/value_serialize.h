#pragma once

#if __cplusplus < 201703L
#    error Header file `db/value_serialize.h` requires C++17
#endif  // __cplusplus < 201703L

#include "value.h"

#include "uxs/io/serialize.h"
#include "uxs/membuffer.h"

namespace uxs {

namespace detail {

template<typename CharT, typename Alloc>
void serialize_db_value(biobuf& os, const db::basic_value<CharT, Alloc>& v) {
    using value_ty = const db::basic_value<CharT, Alloc>;
    os << v.type();
    v.visit([&os](auto x) {
        if constexpr (std::is_same_v<decltype(x), decltype(std::declval<value_ty>().as_string_view())>) {
            os << static_cast<std::uint64_t>(x.size());
            os.write_with_endian(
                est::as_span(reinterpret_cast<const std::uint8_t*>(x.data()), x.size() * sizeof(CharT)), sizeof(CharT));
        } else if constexpr (std::is_same_v<decltype(x), decltype(std::declval<value_ty>().as_array())>) {
            os << static_cast<std::uint64_t>(x.size());
            for (const auto& el : x) { serialize_db_value(os, el); }
        } else if constexpr (std::is_same_v<decltype(x), decltype(std::declval<value_ty>().as_record())>) {
            os << static_cast<std::uint64_t>(x.size());
            for (const auto& [key, value] : x) {
                os << static_cast<std::uint64_t>(key.size());
                os.write_with_endian(
                    est::as_span(reinterpret_cast<const std::uint8_t*>(key.data()), key.size() * sizeof(CharT)),
                    sizeof(CharT));
                serialize_db_value(os, value);
            }
        } else if constexpr (!std::is_same_v<decltype(x), std::nullptr_t>) {
            os << x;
        }
    });
}

template<typename CharT, typename Alloc>
void deserialize_db_value(bibuf& is, db::basic_value<CharT, Alloc>& v, basic_dynbuffer<CharT>& key_buf) {
    auto type = db::dtype::null;
    if (!(is >> type)) { return; }
    v = db::basic_value<CharT, Alloc>(type, [&is, &key_buf](auto type, auto& x) {
        if constexpr (std::is_same_v<decltype(type), db::string_tag_t>) {
            std::uint64_t sz = 0;
            if (!(is >> sz)) { return; }
            x.append_string(static_cast<std::size_t>(sz), [&is](est::span<CharT> s) {
                return is.read_with_endian(
                    est::as_span(reinterpret_cast<std::uint8_t*>(s.data()), s.size() * sizeof(CharT)), sizeof(CharT));
            });
        } else if constexpr (std::is_same_v<decltype(type), db::array_tag_t>) {
            std::uint64_t sz = 0;
            if (!(is >> sz)) { return; }
            x.reserve(db::array_tag, static_cast<std::size_t>(sz));
            for (; sz && is; --sz) { deserialize_db_value(is, x.emplace_back(x.get_allocator()), key_buf); }
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
                deserialize_db_value(is, x.emplace(key, x.get_allocator()).value(), key_buf);
            }
        } else {
            is >> x;
        }
    });
}

}  // namespace detail

template<typename CharT, typename Alloc>
biobuf& operator<<(biobuf& os, const db::basic_value<CharT, Alloc>& v) {
    detail::serialize_db_value(os, v);
    return os;
}

template<typename CharT, typename Alloc>
bibuf& operator>>(bibuf& is, db::basic_value<CharT, Alloc>& v) {
    basic_inline_dynbuffer<CharT> key_buf;
    detail::deserialize_db_value(is, v, key_buf);
    return is;
}

}  // namespace uxs
