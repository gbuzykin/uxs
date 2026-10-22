#pragma once

#if __cplusplus < 201703L
#    error Header file `db/value_serialize.h` requires C++17
#endif  // __cplusplus < 201703L

#include "value.h"

#include "uxs/serialize.h"

namespace uxs {

namespace detail {

struct span_serializer {
    biobuf& os;
    template<typename CharT>
    void operator()(std::basic_string_view<CharT> s) const {
        os.write_with_endian(est::as_span(reinterpret_cast<const std::uint8_t*>(s.data()), s.size() * sizeof(CharT)),
                             sizeof(CharT));
    }
};

template<typename CharT, typename Alloc>
void serialize_db_value(biobuf& os, const db::basic_value<CharT, Alloc>& v) {
    os << v.type();
    v.visit([&os](auto tag, const auto& x) {
        if constexpr (std::is_same_v<decltype(tag), db::string_tag_t>) {
            const auto s = x.as_string_view();
            os << static_cast<std::uint64_t>(s.size());
            span_serializer{os}(s);
        } else if constexpr (std::is_same_v<decltype(tag), db::array_tag_t>) {
            const auto r = x.as_array();
            os << static_cast<std::uint64_t>(r.size());
            for (const auto& el : r) { serialize_db_value(os, el); }
        } else if constexpr (std::is_same_v<decltype(tag), db::object_tag_t>) {
            const auto r = x.as_object();
            os << static_cast<std::uint64_t>(r.size());
            for (const auto& [key, value] : r) {
                os << static_cast<std::uint64_t>(key.size());
                span_serializer{os}(key);
                serialize_db_value(os, value);
            }
        } else if constexpr (!std::is_same_v<decltype(tag), db::null_tag_t>) {
            os << x;
        }
    });
}

struct span_deserializer {
    bibuf& is;
    template<typename CharT>
    std::size_t operator()(est::span<CharT> s) const {
        return is.read_with_endian(est::as_span(reinterpret_cast<std::uint8_t*>(s.data()), s.size() * sizeof(CharT)),
                                   sizeof(CharT));
    }
};

template<typename CharT, typename Alloc>
void deserialize_db_value(bibuf& is, db::basic_value<CharT, Alloc>& v) {
    using value_type = db::basic_value<CharT, Alloc>;
    auto al = v.get_allocator();
    auto type = db::dtype::null;
    if (!(is >> type)) { return; }
    if (v.type() != type) { v = value_type(type, al); }
    v.visit([&is, &al](auto tag, auto& x) {
        if constexpr (std::is_same_v<decltype(tag), db::string_tag_t>) {
            std::uint64_t sz = 0;
            if (!(is >> sz)) { return; }
            x.assign(db::string_tag, static_cast<std::size_t>(sz), span_deserializer{is});
        } else if constexpr (std::is_same_v<decltype(tag), db::array_tag_t>) {
            std::uint64_t sz = 0;
            if (!(is >> sz)) { return; }
            x.resize(static_cast<std::size_t>(sz));
            for (auto& el : x.as_array()) { deserialize_db_value(is, el); }
        } else if constexpr (std::is_same_v<decltype(tag), db::object_tag_t>) {
            std::uint64_t sz = 0;
            if (!(is >> sz)) { return; }
            auto chain = value_type::make_node_chain(al);
            x.extract_all(chain);
            x.reserve(db::object_tag, static_cast<std::size_t>(sz));
            for (; sz; --sz) {
                const auto make_node = [&is, &al, &chain](std::uint64_t key_sz) {
                    if (!chain.empty()) {
                        auto node = chain.pop_front();
                        node.set_key(key_sz, span_deserializer{is});
                        return node;
                    }
                    return value_type::make_node(al, key_sz, span_deserializer{is}, value_type(al));
                };
                std::uint64_t key_sz = 0;
                if (!(is >> key_sz)) { return; }
                auto node = make_node(key_sz);
                deserialize_db_value(is, (*x.append_new(std::move(node))).value());
            }
        } else if constexpr (!std::is_same_v<decltype(tag), db::null_tag_t>) {
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
    detail::deserialize_db_value(is, v);
    return is;
}

}  // namespace uxs
