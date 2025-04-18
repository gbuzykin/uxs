#pragma once

#include "uxs/db/value.h"
#include "uxs/string_cvt.h"

#include <cmath>

namespace uxs {
namespace db {

//-----------------------------------------------------------------------------

template<typename CharT, typename Alloc>
UXS_EXPORT bool operator==(const basic_value<CharT, Alloc>& lhs, const basic_value<CharT, Alloc>& rhs) noexcept {
    static const auto compare_long_integer = [](std::int64_t lhs, const basic_value<CharT, Alloc>& rhs) {
        switch (rhs.type_) {
            case dtype::integer: return lhs == rhs.value_.i;
            case dtype::unsigned_integer: return lhs == static_cast<std::int64_t>(rhs.value_.u);
            case dtype::long_integer: return lhs == rhs.value_.i64;
            case dtype::unsigned_long_integer: {
                return rhs.value_.u64 <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) &&
                       lhs == static_cast<std::int64_t>(rhs.value_.u64);
            } break;
            default: return false;
        }
    };

    static const auto compare_unsigned_long_integer = [](std::uint64_t lhs, const basic_value<CharT, Alloc>& rhs) {
        switch (rhs.type_) {
            case dtype::integer: return rhs.value_.i >= 0 && lhs == static_cast<std::uint64_t>(rhs.value_.i);
            case dtype::unsigned_integer: return lhs == rhs.value_.u;
            case dtype::long_integer: return rhs.value_.i64 >= 0 && lhs == static_cast<std::uint64_t>(rhs.value_.i64);
            case dtype::unsigned_long_integer: return lhs == rhs.value_.u64;
            default: return false;
        }
    };

    switch (lhs.type_) {
        case dtype::null: return rhs.type_ == dtype::null;
        case dtype::boolean: return rhs.type_ == dtype::boolean && lhs.value_.b == rhs.value_.b;
        case dtype::integer: return compare_long_integer(lhs.value_.i, rhs);
        case dtype::unsigned_integer: return compare_unsigned_long_integer(lhs.value_.u, rhs);
        case dtype::long_integer: return compare_long_integer(lhs.value_.i64, rhs);
        case dtype::unsigned_long_integer: return compare_unsigned_long_integer(lhs.value_.u64, rhs);
        case dtype::double_precision: return rhs.type_ == dtype::double_precision && lhs.value_.dbl == rhs.value_.dbl;
        case dtype::string: return rhs.type_ == dtype::string && lhs.value_.str == rhs.value_.str;
        case dtype::array: return rhs.type_ == dtype::array && lhs.value_.arr == rhs.value_.arr;
        case dtype::record: return rhs.type_ == dtype::record && lhs.value_.rec == rhs.value_.rec;
        default: UXS_UNREACHABLE_CODE;
    }
}

//-----------------------------------------------------------------------------
// Flexible array implementation
namespace detail {

template<typename Alloc, typename Ty,
         typename = std::enable_if_t<
             std::is_trivially_move_constructible<Ty>::value ||
             std::is_same<typename std::allocator_traits<Alloc>::template rebind_alloc<Ty>, std::allocator<Ty>>::value>>
static void move_values(const Ty* first, const Ty* last, Ty* dest) noexcept {
    std::memcpy(static_cast<void*>(dest), static_cast<const void*>(first), (last - first) * sizeof(Ty));
}

template<typename Alloc, typename Ty, typename... Dummy>
static void move_values(const Ty* first, const Ty* last, Ty* dest, Dummy&&...) noexcept {
    for (; first != last; ++first, ++dest) { new (dest) Ty(std::move(*first)); }
}

template<typename Alloc, typename Ty,
         typename = std::enable_if_t<
             std::is_trivially_destructible<Ty>::value ||
             std::is_same<typename std::allocator_traits<Alloc>::template rebind_alloc<Ty>, std::allocator<Ty>>::value>>
static void destruct_moved_values(Ty* first, Ty* last) noexcept {}

template<typename Alloc, typename Ty, typename... Dummy>
static void destruct_moved_values(Ty* first, Ty* last, Dummy&&...) noexcept {
    for (; first != last; ++first) { first->~Ty(); };
}

template<typename Ty, typename Alloc>
/*static*/ auto flexarray_t<Ty, Alloc>::alloc(alloc_type& al, std::size_t sz, std::size_t cap) -> data_t* {
    const std::size_t alloc_sz = get_alloc_sz(cap);
    data_t* p = al.allocate(alloc_sz);
    p->size = sz;
    p->capacity = (alloc_sz * sizeof(data_t) - offsetof(data_t, x)) / sizeof(Ty);
    assert(p->capacity >= cap && get_alloc_sz(p->capacity) == alloc_sz);
    return p;
}

template<typename Ty, typename Alloc>
void flexarray_t<Ty, Alloc>::grow(alloc_type& al, std::size_t extra) {
    assert(p_);
    std::size_t delta_sz = std::max(extra, p_->size >> 1);
    const std::size_t max_sz = max_size(al);
    if (delta_sz > max_sz - p_->size) {
        if (extra > max_sz - p_->size) { throw std::length_error("too much to reserve"); }
        delta_sz = std::max(extra, (max_sz - p_->size) >> 1);
    }
    data_t* p_new = alloc(al, p_->size, p_->size + delta_sz);
    detail::move_values<alloc_type>(p_->data(), p_->data() + p_->size, p_new->data());
    detail::destruct_moved_values<alloc_type>(p_->data(), p_->data() + p_->size);
    dealloc(al, p_);
    p_ = p_new;
}

template<typename Ty, typename Alloc>
void flexarray_t<Ty, Alloc>::rotate_back(std::size_t pos) noexcept {
    assert(p_ && pos < p_->size - 1);
    Ty* item = p_->data() + p_->size;
    Ty t(std::move(*(item - 1)));
    while (--item != p_->data() + pos) { *item = std::move(*(item - 1)); }
    *item = std::move(t);
}

template<typename Ty, typename Alloc>
void flexarray_t<Ty, Alloc>::construct(alloc_type& al, const_view_type init) {
    p_ = nullptr;
    try {
        create_impl(al, init.size(), init.data());
    } catch (...) {
        destruct(al);
        throw;
    }
}

#if __cplusplus < 201703L
template<typename Ty, typename Alloc>
void flexarray_t<Ty, Alloc>::construct(alloc_type& al, std::initializer_list<Ty> init) {
    construct(al, init.begin(), init.end());
}
#endif  // __cplusplus < 201703L

template<typename Ty, typename Alloc>
void flexarray_t<Ty, Alloc>::assign(alloc_type& al, const_view_type init) {
    if (!p_) { return create_impl(al, init.size(), init.data()); }
    assign_impl(al, init.size(), init.data());
}

template<typename Ty, typename Alloc>
void flexarray_t<Ty, Alloc>::append(alloc_type& al, const_view_type init) {
    if (!p_) { return create_impl(al, init.size(), init.data()); }
    append_impl(al, init.size(), init.data());
}

template<typename Ty, typename Alloc>
void flexarray_t<Ty, Alloc>::clear() noexcept {
    if (!p_) { return; }
    destruct_items(p_->data(), p_->data() + p_->size);
    p_->size = 0;
}

template<typename Ty, typename Alloc>
void flexarray_t<Ty, Alloc>::reserve(alloc_type& al, std::size_t sz) {
    if (!p_) {
        if (!sz) { return; }
        p_ = alloc_checked(al, 0, sz);
    } else if (sz > p_->capacity) {
        grow(al, sz - p_->size);
    }
}

template<typename Ty, typename Alloc>
void flexarray_t<Ty, Alloc>::resize(alloc_type& al, std::size_t sz, const Ty& v) {
    reserve(al, sz);
    if (!p_) { return; }
    if (sz <= p_->size) {
        destruct_items(p_->data() + sz, p_->data() + p_->size);
        p_->size = sz;
    } else {
        for (Ty* item = p_->data() + p_->size; item != p_->data() + sz; ++item) {
            new (item) Ty(v);
            ++p_->size;
        }
    }
}

template<typename Ty, typename Alloc>
Ty* flexarray_t<Ty, Alloc>::erase(const Ty* item_to_erase) {
    assert(p_ && item_to_erase >= p_->data() && item_to_erase < p_->data() + p_->size);
    const std::size_t pos = item_to_erase - p_->data();
    Ty* next_item = p_->data() + pos;
    Ty* last = p_->data() + --p_->size;
    for (Ty* item = next_item; item != last; ++item) { *item = std::move(*(item + 1)); }
    last->~Ty();
    return next_item;
}

}  // namespace detail

//-----------------------------------------------------------------------------
// Record container implementation
namespace detail {

template<typename CharT, typename Alloc>
/*static*/ record_value<CharT, Alloc>* record_value<CharT, Alloc>::alloc(alloc_type& al, key_type key) {
    if (key.size() > max_name_size(al)) { throw std::length_error("too much to reserve"); }
    const std::size_t alloc_sz = get_alloc_sz(key.size());
    record_value* node = al.allocate(alloc_sz);
    node->key_sz_ = key.size();
    std::copy_n(key.data(), key.size(), node->key_chars_);
    return node;
}

template<typename CharT, typename Alloc>
void record_t<CharT, Alloc>::data_t::init() noexcept {
    dllist_make_cycle(&head);
    size = 0;
    node_traits::set_head(&head, &head);
    for (auto& item : est::as_span(hashtbl, bucket_count)) { item = nullptr; }
}

template<typename CharT, typename Alloc>
void record_t<CharT, Alloc>::data_t::init_from(data_t* p) noexcept {
    if (p->size) {
        head = p->head;
        head.next->prev = &head;
        head.prev->next = &head;
    } else {
        dllist_make_cycle(&head);
    }
    size = p->size;
    node_traits::set_head(&head, &head);
    for (auto& item : est::as_span(hashtbl, bucket_count)) { item = nullptr; }
}

template<typename CharT, typename Alloc>
/*static*/ auto record_t<CharT, Alloc>::alloc(alloc_type& al, std::size_t bucket_count) -> data_t* {
    const std::size_t alloc_sz = get_alloc_sz(bucket_count);
    data_t* p = al.allocate(alloc_sz);
    p->bucket_count = (alloc_sz * sizeof(data_t) - offsetof(data_t, hashtbl)) / sizeof(list_links_t*);
    assert(p->bucket_count >= bucket_count && get_alloc_sz(p->bucket_count) == alloc_sz);
    return p;
}

template<typename CharT, typename Alloc>
void record_t<CharT, Alloc>::construct(alloc_type& al, record_t rec) {
    construct(al, rec.size());
    try {
        insert_impl(al, rec);
    } catch (...) {
        destruct(al);
        throw;
    }
}

template<typename CharT, typename Alloc>
void record_t<CharT, Alloc>::construct(alloc_type& al, std::initializer_list<mapped_type> init) {
    construct(al, init.size());
    try {
        insert_impl(al, init);
    } catch (...) {
        destruct(al);
        throw;
    }
}

template<typename CharT, typename Alloc>
void record_t<CharT, Alloc>::construct(alloc_type& al, std::initializer_list<std::pair<key_type, mapped_type>> init) {
    construct(al, init.begin(), init.end());
}

template<typename CharT, typename Alloc>
void record_t<CharT, Alloc>::assign(alloc_type& al, record_t rec) {
    clear(al);
    insert_impl(al, rec);
}

template<typename CharT, typename Alloc>
void record_t<CharT, Alloc>::assign(alloc_type& al, std::initializer_list<mapped_type> init) {
    clear(al);
    insert_impl(al, init);
}

template<typename CharT, typename Alloc>
void record_t<CharT, Alloc>::destruct_items(alloc_type& al) noexcept {
    typename node_t::alloc_type node_al(al);
    list_links_t* node = p_->head.next;
    while (node != &p_->head) {
        list_links_t* next = node->next;
        node_t::destroy(node_al, node_t::from_links(node));
        node = next;
    }
}

template<typename CharT, typename Alloc>
void record_t<CharT, Alloc>::add_to_hash(node_t* node) noexcept {
    node_traits::set_head(&node->links_, &p_->head);
    list_links_t** p_next_bucket = &p_->hashtbl[node->hash_code_ % p_->bucket_count];
    node->next_bucket_ = *p_next_bucket;
    *p_next_bucket = &node->links_;
}

template<typename CharT, typename Alloc>
void record_t<CharT, Alloc>::insert_node(node_t* node, std::size_t hash_code) noexcept {
    node->hash_code_ = hash_code;
    add_to_hash(node);
    dllist_insert_before(&p_->head, &node->links_);
    ++p_->size;
}

template<typename CharT, typename Alloc>
void record_t<CharT, Alloc>::insert_impl(alloc_type& al, record_t rec) {
    if (p_->bucket_count - p_->size < rec.p_->size) { rehash(al, rec.p_->size); }
    typename node_t::alloc_type node_al(al);
    for (list_links_t* item = rec.p_->head.next; item != &rec.p_->head; item = item->next) {
        const auto& v = *node_t::from_links(item);
        node_t* node = node_t::create(node_al, v.key(), v.value());
        insert_node(node, v.hash_code_);
    }
}

template<typename CharT, typename Alloc>
void record_t<CharT, Alloc>::insert_impl(alloc_type& al, std::initializer_list<mapped_type> init) {
    if (p_->bucket_count - p_->size < init.size()) { rehash(al, init.size()); }
    typename node_t::alloc_type node_al(al);
    for (auto first = init.begin(); first != init.end(); ++first) {
        const auto key = (*first).value_.arr[0].value_.str.cview();
        node_t* node = node_t::create(node_al, key, (*first).value_.arr[1]);
        insert_node(node, hasher_t{}(key));
    }
}

template<typename CharT, typename Alloc>
void record_t<CharT, Alloc>::rehash(alloc_type& al, std::size_t extra) {
    std::size_t delta_count = std::max(extra, p_->size >> 1);
    const std::size_t max_count = max_size(al);
    if (delta_count > max_count - p_->size) {
        if (extra > max_count - p_->size) { throw std::length_error("too much to reserve"); }
        delta_count = std::max(extra, (max_count - p_->size) >> 1);
    }
    data_t* p_new = alloc(al, p_->size + delta_count);
    p_new->init_from(p_);
    dealloc(al, p_);
    p_ = p_new;
    for (list_links_t* node = p_->head.next; node != &p_->head; node = node->next) {
        add_to_hash(node_t::from_links(node));
    }
}

template<typename CharT, typename Alloc>
list_links_t* record_t<CharT, Alloc>::find_impl(key_type key, std::size_t hash_code) const noexcept {
    list_links_t* next_bucket = p_->hashtbl[hash_code % p_->bucket_count];
    while (next_bucket) {
        const auto& v = *node_t::from_links(next_bucket);
        if (v.hash_code_ == hash_code && v.key() == key) { return next_bucket; }
        next_bucket = v.next_bucket_;
    }
    return &p_->head;
}

template<typename CharT, typename Alloc>
std::size_t record_t<CharT, Alloc>::count(key_type key) const noexcept {
    std::size_t count = 0;
    const std::size_t hash_code = hasher_t{}(key);
    list_links_t* next_bucket = p_->hashtbl[hash_code % p_->bucket_count];
    while (next_bucket) {
        const auto& v = *node_t::from_links(next_bucket);
        if (v.hash_code_ == hash_code && v.key() == key) { ++count; }
        next_bucket = v.next_bucket_;
    }
    return count;
}

template<typename CharT, typename Alloc>
void record_t<CharT, Alloc>::clear(alloc_type& al) noexcept {
    destruct_items(al);
    p_->init();
}

template<typename CharT, typename Alloc>
list_links_t* record_t<CharT, Alloc>::erase(alloc_type& al, list_links_t* node) {
    assert(node != &p_->head);
    auto& v = *node_t::from_links(node);
    list_links_t** p_next_bucket = &p_->hashtbl[v.hash_code_ % p_->bucket_count];
    while (*p_next_bucket != node) { p_next_bucket = &node_t::from_links(*p_next_bucket)->next_bucket_; }
    *p_next_bucket = v.next_bucket_;
    list_links_t* next = dllist_remove(node);
    typename node_t::alloc_type node_al(al);
    node_t::destroy(node_al, &v);
    --p_->size;
    return next;
}

template<typename CharT, typename Alloc>
std::size_t record_t<CharT, Alloc>::erase(alloc_type& al, key_type key) {
    const std::size_t old_sz = p_->size;
    const std::size_t hash_code = hasher_t{}(key);
    typename node_t::alloc_type node_al(al);
    list_links_t** p_next_bucket = &p_->hashtbl[hash_code % p_->bucket_count];
    while (*p_next_bucket) {
        auto& v = *node_t::from_links(*p_next_bucket);
        if (v.hash_code_ == hash_code && v.key() == key) {
            *p_next_bucket = v.next_bucket_;
            dllist_remove(&v.links_);
            node_t::destroy(node_al, &v);
            --p_->size;
        } else {
            p_next_bucket = &v.next_bucket_;
        }
    }
    return old_sz - p_->size;
}

}  // namespace detail

//-----------------------------------------------------------------------------

namespace detail {
template<typename CharT, typename Alloc>
bool is_record(std::initializer_list<basic_value<CharT, Alloc>> init) noexcept {
    return std::all_of(init.begin(), init.end(), [](const basic_value<CharT, Alloc>& v) {
        return v.is_array() && v.size() == 2 && v[0].is_string();
    });
}
}  // namespace detail

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc>::basic_value(std::initializer_list<basic_value> init, const Alloc& al)
    : alloc_type(al), type_(detail::is_record(init) ? dtype::record : dtype::array) {
    if (type_ == dtype::record) {
        typename record_t::alloc_type rec_al(*this);
        value_.rec.construct(rec_al, init);
    } else {
        typename value_array_t::alloc_type arr_al(*this);
        value_.arr.construct(arr_al, init);
    }
}

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc>& basic_value<CharT, Alloc>::operator=(std::basic_string_view<char_type> s) {
    if (type_ != dtype::string) {
        if (type_ != dtype::null) { destroy(); }
        value_.str.construct();
        type_ = dtype::string;
    }
    typename char_array_t::alloc_type str_al(*this);
    value_.str.assign(str_al, s);
    return *this;
}

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc>& basic_value<CharT, Alloc>::append_string(std::basic_string_view<char_type> s) {
    if (type_ != dtype::string) { init_as_string(); }
    typename char_array_t::alloc_type str_al(*this);
    value_.str.append(str_al, s);
    return *this;
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::assign(std::initializer_list<basic_value> init) {
    if (!detail::is_record(init)) { return assign(array_construct_t{}, init.begin(), init.end()); }
    typename record_t::alloc_type rec_al(*this);
    if (type_ != dtype::record) {
        if (type_ != dtype::null) { destroy(); }
        value_.rec.construct(rec_al, init.size());
        type_ = dtype::record;
    }
    value_.rec.assign(rec_al, init);
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::assign(array_construct_t, std::initializer_list<basic_value> init) {
    assign(array_construct_t{}, init.begin(), init.end());
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::assign(record_construct_t,
                                       std::initializer_list<std::pair<key_type, basic_value>> init) {
    assign(record_construct_t{}, init.begin(), init.end());
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::insert(std::size_t pos, std::initializer_list<basic_value> init) {
    insert(pos, init.begin(), init.end());
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::insert(std::initializer_list<std::pair<key_type, basic_value>> init) {
    insert(init.begin(), init.end());
}

//-----------------------------------------------------------------------------

namespace detail {
inline bool is_integral(double d) noexcept {
    double integral_part = 0.;
    return std::modf(d, &integral_part) == 0.;
}
}  // namespace detail

template<typename CharT, typename Alloc>
est::optional<bool> basic_value<CharT, Alloc>::get_bool() const {
    switch (type_) {
        case dtype::null: return est::nullopt();
        case dtype::boolean: return value_.b;
        case dtype::integer: return value_.i != 0;
        case dtype::unsigned_integer: return value_.u != 0;
        case dtype::long_integer: return value_.i64 != 0;
        case dtype::unsigned_long_integer: return value_.u64 != 0;
        case dtype::double_precision: return value_.dbl != 0;
        case dtype::string: {
            est::optional<bool> result(est::in_place());
            return from_basic_string(value_.str.cview(), *result) ? result : est::nullopt();
        } break;
        case dtype::array: return est::nullopt();
        case dtype::record: return est::nullopt();
        default: UXS_UNREACHABLE_CODE;
    }
}

template<typename CharT, typename Alloc>
est::optional<std::int32_t> basic_value<CharT, Alloc>::get_int() const {
    switch (type_) {
        case dtype::null: return est::nullopt();
        case dtype::boolean: return est::nullopt();
        case dtype::integer: return value_.i;
        case dtype::unsigned_integer:
            return value_.u <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ?
                       est::make_optional(static_cast<std::int32_t>(value_.u)) :
                       est::nullopt();
        case dtype::long_integer:
            return value_.i64 >= std::numeric_limits<std::int32_t>::min() &&
                           value_.i64 <= std::numeric_limits<std::int32_t>::max() ?
                       est::make_optional(static_cast<std::int32_t>(value_.i64)) :
                       est::nullopt();
        case dtype::unsigned_long_integer:
            return value_.u64 <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) ?
                       est::make_optional(static_cast<std::int32_t>(value_.u64)) :
                       est::nullopt();
        case dtype::double_precision:
            return value_.dbl >= std::numeric_limits<std::int32_t>::min() &&
                           value_.dbl <= std::numeric_limits<std::int32_t>::max() ?
                       est::make_optional(static_cast<std::int32_t>(value_.dbl)) :
                       est::nullopt();
        case dtype::string: {
            est::optional<std::int32_t> result(est::in_place());
            return from_basic_string(value_.str.cview(), *result) ? result : est::nullopt();
        } break;
        case dtype::array: return est::nullopt();
        case dtype::record: return est::nullopt();
        default: UXS_UNREACHABLE_CODE;
    }
}

template<typename CharT, typename Alloc>
est::optional<std::uint32_t> basic_value<CharT, Alloc>::get_uint() const {
    switch (type_) {
        case dtype::null: return est::nullopt();
        case dtype::boolean: return est::nullopt();
        case dtype::integer:
            return value_.i >= 0 ? est::make_optional(static_cast<std::uint32_t>(value_.i)) : est::nullopt();
        case dtype::unsigned_integer: return value_.u;
        case dtype::long_integer:
            return value_.i64 >= 0 &&
                           value_.i64 <= static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()) ?
                       est::make_optional(static_cast<std::uint32_t>(value_.i64)) :
                       est::nullopt();
        case dtype::unsigned_long_integer:
            return value_.u64 <= std::numeric_limits<std::uint32_t>::max() ?
                       est::make_optional(static_cast<std::uint32_t>(value_.u64)) :
                       est::nullopt();
        case dtype::double_precision:
            return value_.dbl >= 0 && value_.dbl <= std::numeric_limits<std::uint32_t>::max() ?
                       est::make_optional(static_cast<std::uint32_t>(value_.dbl)) :
                       est::nullopt();
        case dtype::string: {
            est::optional<std::uint32_t> result(est::in_place());
            return from_basic_string(value_.str.cview(), *result) ? result : est::nullopt();
        } break;
        case dtype::array: return est::nullopt();
        case dtype::record: return est::nullopt();
        default: UXS_UNREACHABLE_CODE;
    }
}

template<typename CharT, typename Alloc>
est::optional<std::int64_t> basic_value<CharT, Alloc>::get_int64() const {
    switch (type_) {
        case dtype::null: return est::nullopt();
        case dtype::boolean: return est::nullopt();
        case dtype::integer: return value_.i;
        case dtype::unsigned_integer: return static_cast<std::int64_t>(value_.u);
        case dtype::long_integer: return value_.i64;
        case dtype::unsigned_long_integer:
            return value_.u64 <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ?
                       est::make_optional(static_cast<std::int64_t>(value_.u64)) :
                       est::nullopt();
        case dtype::double_precision:
            // Note that double(2^63 - 1) will be rounded up to 2^63, so maximum is excluded
            return value_.dbl >= static_cast<double>(std::numeric_limits<std::int64_t>::min()) &&
                           value_.dbl < static_cast<double>(std::numeric_limits<std::int64_t>::max()) ?
                       est::make_optional(static_cast<std::int64_t>(value_.dbl)) :
                       est::nullopt();
        case dtype::string: {
            est::optional<std::int64_t> result(est::in_place());
            return from_basic_string(value_.str.cview(), *result) ? result : est::nullopt();
        } break;
        case dtype::array: return est::nullopt();
        case dtype::record: return est::nullopt();
        default: UXS_UNREACHABLE_CODE;
    }
}

template<typename CharT, typename Alloc>
est::optional<std::uint64_t> basic_value<CharT, Alloc>::get_uint64() const {
    switch (type_) {
        case dtype::null: return est::nullopt();
        case dtype::boolean: return est::nullopt();
        case dtype::integer:
            return value_.i >= 0 ? est::make_optional(static_cast<std::uint64_t>(value_.i)) : est::nullopt();
        case dtype::unsigned_integer: return value_.u;
        case dtype::long_integer:
            return value_.i64 >= 0 ? est::make_optional(static_cast<std::uint64_t>(value_.i64)) : est::nullopt();
        case dtype::unsigned_long_integer: return value_.u64;
        case dtype::double_precision:
            // Note that double(2^64 - 1) will be rounded up to 2^64, so maximum is excluded
            return value_.dbl >= 0 && value_.dbl < static_cast<double>(std::numeric_limits<std::uint64_t>::max()) ?
                       est::make_optional(static_cast<std::uint64_t>(value_.dbl)) :
                       est::nullopt();
        case dtype::string: {
            est::optional<std::uint64_t> result(est::in_place());
            return from_basic_string(value_.str.cview(), *result) ? result : est::nullopt();
        } break;
        case dtype::array: return est::nullopt();
        case dtype::record: return est::nullopt();
        default: UXS_UNREACHABLE_CODE;
    }
}

template<typename CharT, typename Alloc>
est::optional<double> basic_value<CharT, Alloc>::get_double() const {
    switch (type_) {
        case dtype::null: return est::nullopt();
        case dtype::boolean: return est::nullopt();
        case dtype::integer: return value_.i;
        case dtype::unsigned_integer: return value_.u;
        case dtype::long_integer: return static_cast<double>(value_.i64);
        case dtype::unsigned_long_integer: return static_cast<double>(value_.u64);
        case dtype::double_precision: return value_.dbl;
        case dtype::string: {
            est::optional<double> result(est::in_place());
            return from_basic_string(value_.str.cview(), *result) ? result : est::nullopt();
        } break;
        case dtype::array: return est::nullopt();
        case dtype::record: return est::nullopt();
        default: UXS_UNREACHABLE_CODE;
    }
}

template<typename CharT, typename Alloc>
est::optional<std::basic_string<CharT>> basic_value<CharT, Alloc>::get_string() const {
    switch (type_) {
        case dtype::null: {
            return est::make_optional<std::basic_string<CharT>>(string_literal<CharT, 'n', 'u', 'l', 'l'>{}());
        } break;
        case dtype::boolean: {
            return est::make_optional<std::basic_string<CharT>>(value_.b ?
                                                                    string_literal<CharT, 't', 'r', 'u', 'e'>{}() :
                                                                    string_literal<CharT, 'f', 'a', 'l', 's', 'e'>{}());
        } break;
        case dtype::integer: {
            inline_basic_dynbuffer<CharT> buf;
            to_basic_string(buf, value_.i);
            return est::make_optional<std::basic_string<CharT>>(buf.data(), buf.size());
        } break;
        case dtype::unsigned_integer: {
            inline_basic_dynbuffer<CharT> buf;
            to_basic_string(buf, value_.u);
            return est::make_optional<std::basic_string<CharT>>(buf.data(), buf.size());
        } break;
        case dtype::long_integer: {
            inline_basic_dynbuffer<CharT> buf;
            to_basic_string(buf, value_.i64);
            return est::make_optional<std::basic_string<CharT>>(buf.data(), buf.size());
        } break;
        case dtype::unsigned_long_integer: {
            inline_basic_dynbuffer<CharT> buf;
            to_basic_string(buf, value_.u64);
            return est::make_optional<std::basic_string<CharT>>(buf.data(), buf.size());
        } break;
        case dtype::double_precision: {
            inline_basic_dynbuffer<CharT> buf;
            to_basic_string(buf, value_.dbl, fmt_opts{fmt_flags::json_compat});
            return est::make_optional<std::basic_string<CharT>>(buf.data(), buf.size());
        } break;
        case dtype::string: return est::make_optional<std::basic_string<CharT>>(value_.str.cview());
        case dtype::array: return est::nullopt();
        case dtype::record: return est::nullopt();
        default: UXS_UNREACHABLE_CODE;
    }
}

// --------------------------

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::is_int() const noexcept {
    switch (type_) {
        case dtype::integer: return true;
        case dtype::unsigned_integer:
            return value_.u <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
        case dtype::long_integer:
            return value_.i64 >= std::numeric_limits<std::int32_t>::min() &&
                   value_.i64 <= std::numeric_limits<std::int32_t>::max();
        case dtype::unsigned_long_integer:
            return value_.u64 <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
        case dtype::double_precision:
            return value_.dbl >= std::numeric_limits<std::int32_t>::min() &&
                   value_.dbl <= std::numeric_limits<std::int32_t>::max() && detail::is_integral(value_.dbl);
        default: break;
    }
    return false;
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::is_uint() const noexcept {
    switch (type_) {
        case dtype::integer: return value_.i >= 0;
        case dtype::unsigned_integer: return true;
        case dtype::long_integer:
            return value_.i64 >= 0 &&
                   value_.i64 <= static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max());
        case dtype::unsigned_long_integer: return value_.u64 <= std::numeric_limits<std::uint32_t>::max();
        case dtype::double_precision:
            return value_.dbl >= 0 && value_.dbl <= std::numeric_limits<std::uint32_t>::max() &&
                   detail::is_integral(value_.dbl);
        default: break;
    }
    return false;
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::is_int64() const noexcept {
    switch (type_) {
        case dtype::integer:
        case dtype::unsigned_integer:
        case dtype::long_integer: return true;
        case dtype::unsigned_long_integer:
            return value_.u64 <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        case dtype::double_precision:
            // Note that double(2^63 - 1) will be rounded up to 2^63, so maximum is excluded
            return value_.dbl >= static_cast<double>(std::numeric_limits<std::int64_t>::min()) &&
                   value_.dbl < static_cast<double>(std::numeric_limits<std::int64_t>::max()) &&
                   detail::is_integral(value_.dbl);
        default: break;
    }
    return false;
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::is_uint64() const noexcept {
    switch (type_) {
        case dtype::integer: return value_.i >= 0;
        case dtype::unsigned_integer: return true;
        case dtype::long_integer: return value_.i64 >= 0;
        case dtype::unsigned_long_integer: return true;
        case dtype::double_precision:
            // Note that double(2^64 - 1) will be rounded up to 2^64, so maximum is excluded
            return value_.dbl >= 0 && value_.dbl < static_cast<double>(std::numeric_limits<std::uint64_t>::max()) &&
                   detail::is_integral(value_.dbl);
        default: break;
    }
    return false;
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::is_integral() const noexcept {
    switch (type_) {
        case dtype::integer:
        case dtype::unsigned_integer:
        case dtype::long_integer:
        case dtype::unsigned_long_integer: return true;
        case dtype::double_precision:
            // Note that double(2^64 - 1) will be rounded up to 2^64, so maximum is excluded
            return value_.dbl >= static_cast<double>(std::numeric_limits<std::int64_t>::min()) &&
                   value_.dbl < static_cast<double>(std::numeric_limits<std::uint64_t>::max()) &&
                   detail::is_integral(value_.dbl);
        default: break;
    }
    return false;
}

// --------------------------

template<typename CharT, typename Alloc>
std::size_t basic_value<CharT, Alloc>::size() const noexcept {
    switch (type_) {
        case dtype::null: return 0;
        case dtype::array: return value_.arr.size();
        case dtype::record: return value_.rec.size();
        default: break;
    }
    return 1;
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::clear() noexcept {
    if (type_ == dtype::record) {
        typename record_t::alloc_type rec_al(*this);
        value_.rec.clear(rec_al);
    } else if (type_ == dtype::array) {
        value_.arr.clear();
    }
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::reserve(std::size_t sz) {
    if (type_ != dtype::array) { init_as_array(); }
    typename value_array_t::alloc_type arr_al(*this);
    value_.arr.reserve(arr_al, sz);
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::resize(std::size_t sz) {
    if (type_ != dtype::array) { init_as_array(); }
    typename value_array_t::alloc_type arr_al(*this);
    value_.arr.resize(arr_al, sz, basic_value());
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::resize(std::size_t sz, const basic_value& v) {
    if (type_ != dtype::array) { init_as_array(); }
    typename value_array_t::alloc_type arr_al(*this);
    value_.arr.resize(arr_al, sz, v);
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::erase(std::size_t pos) {
    if (type_ != dtype::array) { throw database_error("not an array"); }
    assert(pos < value_.arr.size());
    value_.arr.erase(value_.arr.cbegin() + pos);
}

template<typename CharT, typename Alloc>
auto basic_value<CharT, Alloc>::erase(const_iterator it) -> iterator {
    if (it.is_record()) {
        if (type_ != dtype::record) { throw database_error("not a record"); }
        detail::list_links_t* node = static_cast<detail::list_links_t*>(it.ptr_);
        uxs_iterator_assert(record_t::node_traits::get_head(node) == value_.rec.cend());
        typename record_t::alloc_type rec_al(*this);
        return iterator(value_.rec.erase(rec_al, node));
    }
    if (type_ != dtype::array) { throw database_error("not an array"); }
    basic_value* item = static_cast<basic_value*>(it.ptr_);
    uxs_iterator_assert(it.begin_ == value_.arr.cbegin() && it.end_ == value_.arr.cend());
    item = value_.arr.erase(item);
    return iterator(item, value_.arr.cbegin(), value_.arr.cend());
}

template<typename CharT, typename Alloc>
std::size_t basic_value<CharT, Alloc>::erase(key_type key) {
    if (type_ != dtype::record) { throw database_error("not a record"); }
    typename record_t::alloc_type rec_al(*this);
    return value_.rec.erase(rec_al, key);
}

// --------------------------

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::init_from(const basic_value& other) {
    switch (other.type_) {
        case dtype::string: {
            typename char_array_t::alloc_type str_al(*this);
            value_.str.construct(str_al, other.value_.str.cview());
        } break;
        case dtype::array: {
            typename value_array_t::alloc_type arr_al(*this);
            value_.arr.construct(arr_al, other.value_.arr.cview());
        } break;
        case dtype::record: {
            typename record_t::alloc_type rec_al(*this);
            value_.rec.construct(rec_al, other.value_.rec);
        } break;
        default: value_ = other.value_; break;
    }
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::assign_from(const basic_value& other) {
    if (type_ != other.type_) {
        if (type_ != dtype::null) { destroy(); }
        init_from(other);
        type_ = other.type_;
        return;
    }
    switch (other.type_) {
        case dtype::string: {
            typename char_array_t::alloc_type str_al(*this);
            value_.str.assign(str_al, other.value_.str.cview());
        } break;
        case dtype::array: {
            typename value_array_t::alloc_type arr_al(*this);
            value_.arr.assign(arr_al, other.value_.arr.cview());
        } break;
        case dtype::record: {
            typename record_t::alloc_type rec_al(*this);
            value_.rec.assign(rec_al, other.value_.rec);
        } break;
        default: value_ = other.value_; break;
    }
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::destroy() noexcept {
    switch (type_) {
        case dtype::string: {
            typename char_array_t::alloc_type str_al(*this);
            value_.str.destruct(str_al);
        } break;
        case dtype::array: {
            typename value_array_t::alloc_type arr_al(*this);
            value_.arr.destruct(arr_al);
        } break;
        case dtype::record: {
            typename record_t::alloc_type rec_al(*this);
            value_.rec.destruct(rec_al);
        } break;
        default: break;
    }
    type_ = dtype::null;
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::init_as_string() {
    if (type_ != dtype::null) { throw database_error("not a string"); }
    value_.str.construct();
    type_ = dtype::string;
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::init_as_array() {
    if (type_ != dtype::null) { throw database_error("not an array"); }
    value_.arr.construct();
    type_ = dtype::array;
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::init_as_record() {
    if (type_ != dtype::null) { throw database_error("not a record"); }
    typename record_t::alloc_type rec_al(*this);
    value_.rec.construct(rec_al);
    type_ = dtype::record;
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::convert_to_array() {
    value_array_t arr;
    arr.construct();
    if (type_ != dtype::null) {
        typename value_array_t::alloc_type arr_al(*this);
        arr.emplace_back(arr_al, std::move(*this));
    }
    value_.arr = arr;
    type_ = dtype::array;
}

}  // namespace db
}  // namespace uxs
