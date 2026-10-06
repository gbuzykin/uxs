#pragma once

#include "uxs/db/value.h"
#include "uxs/dllist.h"
#include "uxs/string_conv.h"

#include <cmath>

namespace uxs {
namespace db {

//-----------------------------------------------------------------------------
// Flexible array implementation
namespace detail {

template<typename Alloc, typename Ty,
         typename = std::enable_if_t<std::is_trivially_copyable<Ty>::value || std::is_empty<Alloc>::value ||
                                     std::is_trivially_copyable<Alloc>::value>>
static void move_values(const Ty* first, const Ty* last, Ty* dst) noexcept {
    std::memcpy(static_cast<void*>(dst), static_cast<const void*>(first), (last - first) * sizeof(Ty));
}

template<typename Alloc, typename Ty, typename... Dummy>
static void move_values(const Ty* first, const Ty* last, Ty* dst, Dummy&&...) noexcept {
    static_assert(sizeof...(Dummy) == 0, "invalid function argument count");
    static_assert(!std::is_trivially_copyable<Ty>::value, "Ty must not be trivially move constructible");
    static_assert(!std::is_empty<Alloc>::value, "allocator must not be empty");
    static_assert(!std::is_trivially_copyable<Alloc>::value, "allocator must not be trivially move constructible");
    for (; first != last; ++first, ++dst) { ::new (dst) Ty(std::move(*first)); }
}

template<typename Alloc, typename Ty,
         typename = std::enable_if_t<std::is_trivially_destructible<Ty>::value || std::is_empty<Alloc>::value ||
                                     std::is_trivially_destructible<Alloc>::value>>
static void destruct_moved_values(Ty* /*first*/, Ty* /*last*/) noexcept {}

template<typename Alloc, typename Ty, typename... Dummy>
static void destruct_moved_values(Ty* first, Ty* last, Dummy&&...) noexcept {
    static_assert(sizeof...(Dummy) == 0, "invalid function argument count");
    static_assert(!std::is_trivially_destructible<Ty>::value, "Ty must not be trivially destructible");
    static_assert(!std::is_empty<Alloc>::value, "allocator must not be empty");
    static_assert(!std::is_trivially_destructible<Alloc>::value, "allocator must not be trivially destructible");
    for (; first != last; ++first) { first->~Ty(); };
}

template<typename Ty, typename Alloc>
auto flexarray_t<Ty, Alloc>::alloc(alloc_type& al, std::size_t size, std::size_t cap) -> data_t* {
    constexpr std::size_t min_capacity = std::is_integral<Ty>::value ? 2 * sizeof(std::uintptr_t) / sizeof(Ty) : 4;
    const std::size_t alloc_sz = get_alloc_sz(std::max(cap, min_capacity));
    data_t* p = reinterpret_cast<data_t*>(alloc_traits::allocate(al, alloc_sz));
    ::new (&p->ref_count) std::atomic<std::size_t>{1};
    p->size = size;
    p->capacity = (alloc_sz * sizeof(typename alloc_traits::value_type) - offsetof(data_t, data_buf)) / sizeof(Ty);
    assert(p->capacity >= cap && get_alloc_sz(p->capacity) == alloc_sz);
    return p;
}

template<typename Ty, typename Alloc>
void flexarray_t<Ty, Alloc>::grow(alloc_type& al, std::size_t extra) {
    static_assert(std::is_nothrow_move_constructible<Ty>::value, "Ty must not be nothrow move constructible");
    static_assert(std::is_nothrow_copy_constructible<Ty>::value, "Ty must not be nothrow copy constructible");

    assert(p_);
    std::size_t delta_sz = std::max(extra, p_->size >> 1);
    const std::size_t max_sz = max_size(al);
    if (delta_sz > max_sz - p_->size) {
        if (extra > max_sz - p_->size) { report_too_much_to_allocate_error(); }
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
void flexarray_t<Ty, Alloc>::destruct(alloc_type& al) noexcept {
    destruct_items(p_->data(), p_->data() + p_->size);
    dealloc(al, p_);
}

template<typename Ty, typename Alloc>
void flexarray_t<Ty, Alloc>::construct_from_view(alloc_type& al, const_view_type view, std::size_t extra) {
    if (view.size() + extra) {
        p_ = alloc_checked(al, view.size() + extra + tail_zero);
        initialize_constructed(al, [this, view]() {
            init_items_copy(p_->data(), p_->data() + view.size(), view.data());
            p_->size = view.size();
        });
    } else {
        p_ = nullptr;
    }
}

template<typename Ty, typename Alloc>
void flexarray_t<Ty, Alloc>::construct_fill_value(alloc_type& al, std::size_t count, const Ty& v) {
    if (count) {
        p_ = alloc_checked(al, count + tail_zero);
        initialize_constructed(al, [this, count, &v]() {
            init_items_fill(p_->data(), p_->data() + count, v);
            p_->size = count;
        });
    } else {
        p_ = nullptr;
    }
}

template<typename Ty, typename Alloc>
void flexarray_t<Ty, Alloc>::assign_view(alloc_type& al, const_view_type view) {
    if (p_ && p_->ref_count == 1 && view.size() + tail_zero <= p_->capacity) {
        return assign_no_realloc(view.begin(), view.size());
    }
    flexarray_t new_arr;
    new_arr.construct_from_view(al, view, 0);
    reset(al, new_arr);
}

template<typename Ty, typename Alloc>
void flexarray_t<Ty, Alloc>::append_view(alloc_type& al, const_view_type view) {
    append_range(al, view.begin(), view.end());
}

template<typename Ty, typename Alloc>
void flexarray_t<Ty, Alloc>::clear(alloc_type& al) noexcept {
    if (!p_) { return; }
    if (p_->ref_count > 1) {
        flexarray_t new_arr;
        new_arr.construct_empty();
        return reset(al, new_arr);
    }
    destruct_items(p_->data(), p_->data() + p_->size);
    p_->size = 0;
}

template<typename Ty, typename Alloc>
void flexarray_t<Ty, Alloc>::reserve(alloc_type& al, std::size_t size) {
    if (!p_) {
        if (!size) { return; }
        p_ = alloc_checked(al, size + tail_zero);
        put_tail_zero(p_->data());
    } else if (p_->ref_count > 1) {
        flexarray_t new_arr;
        new_arr.construct_from_view(al, const_view_type(p_->data(), p_->size), size > p_->size ? size - p_->size : 0);
        reset(al, new_arr);
    } else if (size + tail_zero > p_->capacity) {
        grow(al, size - p_->size + tail_zero);
        put_tail_zero(p_->data() + p_->size);
    }
}

template<typename Ty, typename Alloc>
void flexarray_t<Ty, Alloc>::resize(alloc_type& al, std::size_t size, const Ty& v) {
    reserve(al, size);
    if (!p_) { return; }
    if (size > p_->size) {
        init_items_fill(p_->data() + p_->size, p_->data() + size, v);
    } else {
        destruct_items(p_->data() + size, p_->data() + p_->size);
    }
    p_->size = size;
}

template<typename Ty, typename Alloc>
Ty* flexarray_t<Ty, Alloc>::erase(alloc_type& al, std::size_t pos) {
    assert(p_ && pos < p_->size);
    ensure_unique(al);
    --p_->size;
    Ty* next = p_->data() + pos;
    Ty* last = p_->data() + p_->size;
    for (Ty* item = next; item != last; ++item) { *item = std::move(*(item + 1)); }
    last->~Ty();
    put_tail_zero(last);
    return next;
}

}  // namespace detail

//-----------------------------------------------------------------------------
// Object container implementation
namespace detail {

template<typename CharT, typename Alloc>
auto object_item<CharT, Alloc>::alloc(alloc_type& al, key_type key) -> object_item* {
    if (key.size() + 1 > max_name_alloc_cap(al)) { report_too_much_to_allocate_error(); }
    const std::size_t alloc_sz = get_alloc_sz(std::max<std::size_t>(key.size() + 1, min_char_count));
    object_item* node = reinterpret_cast<object_item*>(alloc_traits::allocate(al, alloc_sz));
    node->key_chars_cap_ = (alloc_sz * sizeof(typename alloc_traits::value_type) -
                            offsetof(object_item, key_chars_buf_)) /
                           sizeof(char_type);
    node->key_sz_ = key.size();
    std::copy_n(key.data(), key.size(), node->key_chars());
    node->key_chars()[node->key_sz_] = '\0';
    return node;
}

template<typename CharT, typename Alloc>
void object_t<CharT, Alloc>::data_t::init() noexcept {
    dllist_make_cycle(&head);
    size = 0;
    node_traits::set_head(&head, &head);
    std::fill_n(hashtbl(), bucket_count, nullptr);
}

template<typename CharT, typename Alloc>
void object_t<CharT, Alloc>::data_t::init_from(object_t other) noexcept {
    if (!other.p_->size) { return init(); }
    head = other.p_->head;
    head.next->prev = &head;
    head.prev->next = &head;
    size = other.p_->size;
    node_traits::set_head(&head, &head);
    std::fill_n(hashtbl(), bucket_count, nullptr);
}

template<typename CharT, typename Alloc>
auto object_t<CharT, Alloc>::alloc(alloc_type& al, std::size_t bucket_count) -> data_t* {
    constexpr std::size_t min_bucket_count = 4;
    const std::size_t alloc_sz = get_alloc_sz(std::max(bucket_count, min_bucket_count));
    data_t* p = reinterpret_cast<data_t*>(alloc_traits::allocate(al, alloc_sz));
    ::new (&p->ref_count) std::atomic<std::size_t>{1};
    p->bucket_count = (alloc_sz * sizeof(typename alloc_traits::value_type) - offsetof(data_t, data_buf)) /
                      sizeof(list_links_t*);
    assert(p->bucket_count >= bucket_count && get_alloc_sz(p->bucket_count) == alloc_sz);
    return p;
}

template<typename CharT, typename Alloc>
void object_t<CharT, Alloc>::construct_copy(alloc_type& al, object_t other, std::size_t extra) {
    construct_empty(al, other.size() + extra);
    initialize_constructed(al, [this, &al, other]() {
        for (list_links_t* node = other.p_->head.next; node != &other.p_->head; node = node->next) {
            const auto& v = *node_t::from_links(node);
            node_t* new_node = node_t::construct(al, v.key(), mapped_type(v.value()));
            insert_node(new_node, &v.hash_code_);
        }
    });
}

template<typename CharT, typename Alloc>
void object_t<CharT, Alloc>::construct_from_common_initializer(alloc_type& al, std::initializer_list<mapped_type> init) {
    construct_empty(al, init.size());
    initialize_constructed(al, [this, &al, init]() { insert_initializer_no_realloc(al, init); });
}

template<typename CharT, typename Alloc>
void object_t<CharT, Alloc>::construct_from_initializer(alloc_type& al,
                                                        std::initializer_list<std::pair<key_type, mapped_type>> init) {
    construct_from_range(al, init.begin(), init.end());
}

template<typename CharT, typename Alloc>
void object_t<CharT, Alloc>::assign_initializer(alloc_type& al, std::initializer_list<mapped_type> init) {
    if (p_->ref_count == 1 && init.size() <= p_->bucket_count) {
        destruct_items(al);
        p_->init();
        return insert_initializer_no_realloc(al, init);
    }
    object_t new_obj;
    new_obj.construct_from_common_initializer(al, init);
    reset(al, new_obj);
}

template<typename CharT, typename Alloc>
void object_t<CharT, Alloc>::reserve(alloc_type& al, std::size_t size) {
    if (p_->ref_count > 1) {
        object_t new_obj;
        new_obj.construct_copy(al, *this, size > p_->size ? size - p_->size : 0);
        reset(al, new_obj);
    } else if (p_->bucket_count < size) {
        rehash(al, size - p_->size);
    }
}

template<typename CharT, typename Alloc>
void object_t<CharT, Alloc>::destruct_items(alloc_type& al) noexcept {
    list_links_t* node = p_->head.next;
    while (node != &p_->head) {
        list_links_t* next = node->next;
        node_t::destroy(al, node_t::from_links(node));
        node = next;
    }
}

template<typename CharT, typename Alloc>
void object_t<CharT, Alloc>::add_to_hash(node_t* node) noexcept {
    node_traits::set_head(&node->links_, &p_->head);
    list_links_t** p_next_bucket = &p_->hashtbl()[node->hash_code_ % p_->bucket_count];
    node->next_bucket_ = *p_next_bucket;
    *p_next_bucket = &node->links_;
}

template<typename CharT, typename Alloc>
void object_t<CharT, Alloc>::insert_node(node_t* node, const std::size_t* p_hash_code) noexcept {
    node->hash_code_ = p_hash_code ? *p_hash_code : hasher_t{}(node->key());
    add_to_hash(node);
    dllist_insert_before(&p_->head, &node->links_);
    ++p_->size;
}

template<typename CharT, typename Alloc>
void object_t<CharT, Alloc>::insert_initializer_no_realloc(alloc_type& al, std::initializer_list<mapped_type> init) {
    for (auto first = init.begin(); first != init.end(); ++first) {
        const auto key = (*first).value_.arr[0].value_.str.cview();
        node_t* node = node_t::construct(al, key, mapped_type((*first).value_.arr[1]));
        insert_node(node, nullptr);
    }
}

template<typename CharT, typename Alloc>
void object_t<CharT, Alloc>::rehash(alloc_type& al, std::size_t extra) {
    std::size_t delta_count = std::max(extra, p_->size >> 1);
    const std::size_t max_count = max_size(al);
    if (delta_count > max_count - p_->size) {
        if (extra > max_count - p_->size) { report_too_much_to_allocate_error(); }
        delta_count = std::max(extra, (max_count - p_->size) >> 1);
    }
    data_t* p_new = alloc(al, p_->size + delta_count);
    p_new->init_from(*this);
    dealloc(al, p_);
    p_ = p_new;
    for (list_links_t* node = p_->head.next; node != &p_->head; node = node->next) {
        add_to_hash(node_t::from_links(node));
    }
}

template<typename CharT, typename Alloc>
void object_t<CharT, Alloc>::clear(alloc_type& al) {
    if (p_->ref_count > 1) {
        object_t new_obj;
        new_obj.construct_empty(al);
        return reset(al, new_obj);
    }
    destruct_items(al);
    p_->init();
}

template<typename CharT, typename Alloc>
void object_t<CharT, Alloc>::destruct(alloc_type& al) noexcept {
    destruct_items(al);
    dealloc(al, p_);
}

template<typename CharT, typename Alloc>
list_links_t* object_t<CharT, Alloc>::find_impl(key_type key, std::size_t* p_hash_code) const noexcept {
    const std::size_t hash_code = hasher_t{}(key);
    if (p_hash_code) { *p_hash_code = hash_code; }
    list_links_t* next_bucket = p_->hashtbl()[hash_code % p_->bucket_count];
    while (next_bucket) {
        const auto& v = *node_t::from_links(next_bucket);
        if (v.hash_code_ == hash_code && v.key() == key) { return next_bucket; }
        next_bucket = v.next_bucket_;
    }
    return &p_->head;
}

template<typename CharT, typename Alloc>
std::size_t object_t<CharT, Alloc>::count(key_type key) const noexcept {
    std::size_t count = 0;
    const std::size_t hash_code = hasher_t{}(key);
    list_links_t* next_bucket = p_->hashtbl()[hash_code % p_->bucket_count];
    while (next_bucket) {
        const auto& v = *node_t::from_links(next_bucket);
        if (v.hash_code_ == hash_code && v.key() == key) { ++count; }
        next_bucket = v.next_bucket_;
    }
    return count;
}

template<typename CharT, typename Alloc>
list_links_t* object_t<CharT, Alloc>::map_node(object_t other, list_links_t* node_to_map) noexcept {
    list_links_t* mapped_node = other.p_->head.next;
    for (list_links_t* node = p_->head.next; node != node_to_map; node = node->next) {
        mapped_node = mapped_node->next;
    }
    return mapped_node;
}

template<typename CharT, typename Alloc>
list_links_t* object_t<CharT, Alloc>::erase(alloc_type& al, list_links_t* node) {
    assert(node != &p_->head);
    if (p_->ref_count > 1) {
        object_t new_obj;
        new_obj.construct_copy(al, *this, 0);
        node = map_node(new_obj, node);
        reset(al, new_obj);
    }
    auto& v = *node_t::from_links(node);
    list_links_t** p_next_bucket = &p_->hashtbl()[v.hash_code_ % p_->bucket_count];
    while (*p_next_bucket != node) { p_next_bucket = &node_t::from_links(*p_next_bucket)->next_bucket_; }
    *p_next_bucket = v.next_bucket_;
    --p_->size;
    list_links_t* next = dllist_remove(node);
    node_t::destroy(al, &v);
    return next;
}

template<typename CharT, typename Alloc>
std::size_t object_t<CharT, Alloc>::erase(alloc_type& al, key_type key) {
    ensure_unique(al);
    const std::size_t prev_sz = p_->size;
    const std::size_t hash_code = hasher_t{}(key);
    list_links_t** p_next_bucket = &p_->hashtbl()[hash_code % p_->bucket_count];
    while (*p_next_bucket) {
        auto& v = *node_t::from_links(*p_next_bucket);
        if (v.hash_code_ == hash_code && v.key() == key) {
            *p_next_bucket = v.next_bucket_;
            --p_->size;
            dllist_remove(&v.links_);
            node_t::destroy(al, &v);
        } else {
            p_next_bucket = &v.next_bucket_;
        }
    }
    return prev_sz - p_->size;
}

}  // namespace detail

//-----------------------------------------------------------------------------

namespace detail {
template<typename CharT, typename Alloc>
bool is_object(std::initializer_list<basic_value<CharT, Alloc>> init) noexcept {
    return std::all_of(init.begin(), init.end(), [](const basic_value<CharT, Alloc>& v) {
        return v.is_array() && v.size() == 2 && v[0].is_string();
    });
}
}  // namespace detail

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc>::basic_value(std::initializer_list<value_type> init, const Alloc& al)
    : alloc_type(al), type_(detail::is_object(init) ? dtype::object : dtype::array) {
    if (type_ == dtype::object) {
        value_.obj.construct_from_common_initializer(*this, init);
    } else {
        value_.arr.construct_from_initializer(*this, init);
    }
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::assign(std::initializer_list<value_type> init) {
    if (!detail::is_object(init)) { return assign(array_tag, init); }
    if (type_ == dtype::object) {
        value_.obj.assign_initializer(*this, init);
    } else {
        object_t new_obj;
        new_obj.construct_from_common_initializer(*this, init);
        destroy();
        type_ = dtype::object;
        value_.obj = new_obj;
    }
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::assign(array_tag_t, std::initializer_list<value_type> init) {
    if (type_ == dtype::array) {
        value_.arr.assign_view(*this, est::as_span(init.begin(), init.size()));
    } else {
        value_array_t new_arr;
        new_arr.construct_from_initializer(*this, init);
        destroy();
        type_ = dtype::array;
        value_.arr = new_arr;
    }
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::assign(object_tag_t, std::initializer_list<std::pair<key_type, value_type>> init) {
    assign(object_tag, init.begin(), init.end());
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::insert(size_type pos, std::initializer_list<value_type> init) {
    insert(pos, init.begin(), init.end());
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::insert(std::initializer_list<std::pair<key_type, value_type>> init) {
    insert(init.begin(), init.end());
}

// --------------------------

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::erase(size_type pos) {
    if (type_ != dtype::array) { report_not_an_array_error(); }
    assert(pos < value_.arr.size());
    value_.arr.erase(*this, pos);
}

template<typename CharT, typename Alloc>
auto basic_value<CharT, Alloc>::erase(const_iterator it) -> iterator {
    if (it.is_object()) {
        if (type_ != dtype::object) { report_not_an_object_error(); }
        detail::list_links_t* node = static_cast<detail::list_links_t*>(it.ptr_);
        uxs_iterator_assert(object_t::node_traits::get_head(node) == value_.obj.cend());
        return iterator(value_.obj.erase(*this, node));
    }
    if (type_ != dtype::array) { report_not_an_array_error(); }
    uxs_iterator_assert(it.begin_ == value_.arr.cbegin() && it.end_ == value_.arr.cend());
    const std::size_t pos = static_cast<value_type*>(it.ptr_) - static_cast<const value_type*>(it.begin_);
    value_type* next = value_.arr.erase(*this, pos);
    return iterator(next, value_.arr.cbegin(), value_.arr.cend());
}

template<typename CharT, typename Alloc>
auto basic_value<CharT, Alloc>::erase(key_type key) -> size_type {
    if (type_ != dtype::object) { report_not_an_object_error(); }
    return value_.obj.erase(*this, key);
}

// --------------------------

namespace detail {
inline bool is_integral(double d) noexcept {
    double integral_part = 0.;
    return std::modf(d, &integral_part) == 0.;
}
}  // namespace detail

template<typename CharT, typename Alloc>
est::optional<bool> basic_value<CharT, Alloc>::get_bool() const {
    switch (type_) {
        case dtype::null: return est::nullopt;
        case dtype::boolean: return value_.b;
        case dtype::integer: return value_.i != 0;
        case dtype::unsigned_integer: return value_.u != 0;
        case dtype::long_integer: return value_.i64 != 0;
        case dtype::unsigned_long_integer: return value_.u64 != 0;
        case dtype::double_precision: return value_.dbl != 0;
        case dtype::string: {
            est::optional<bool> result(est::in_place);
            return from_string_v(value_.str.cview(), *result) ? result : est::nullopt;
        } break;
        case dtype::array: return est::nullopt;
        case dtype::object: return est::nullopt;
        default: UXS_UNREACHABLE_CODE;
    }
}

template<typename CharT, typename Alloc>
est::optional<std::int32_t> basic_value<CharT, Alloc>::get_int() const {
    switch (type_) {
        case dtype::null: return est::nullopt;
        case dtype::boolean: return est::nullopt;
        case dtype::integer: return value_.i;
        case dtype::unsigned_integer:
            return value_.u <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ?
                       est::make_optional(static_cast<std::int32_t>(value_.u)) :
                       est::nullopt;
        case dtype::long_integer:
            return value_.i64 >= std::numeric_limits<std::int32_t>::min() &&
                           value_.i64 <= std::numeric_limits<std::int32_t>::max() ?
                       est::make_optional(static_cast<std::int32_t>(value_.i64)) :
                       est::nullopt;
        case dtype::unsigned_long_integer:
            return value_.u64 <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) ?
                       est::make_optional(static_cast<std::int32_t>(value_.u64)) :
                       est::nullopt;
        case dtype::double_precision:
            return value_.dbl >= std::numeric_limits<std::int32_t>::min() &&
                           value_.dbl <= std::numeric_limits<std::int32_t>::max() ?
                       est::make_optional(static_cast<std::int32_t>(value_.dbl)) :
                       est::nullopt;
        case dtype::string: {
            est::optional<std::int32_t> result(est::in_place);
            return from_string_v(value_.str.cview(), *result) ? result : est::nullopt;
        } break;
        case dtype::array: return est::nullopt;
        case dtype::object: return est::nullopt;
        default: UXS_UNREACHABLE_CODE;
    }
}

template<typename CharT, typename Alloc>
est::optional<std::uint32_t> basic_value<CharT, Alloc>::get_uint() const {
    switch (type_) {
        case dtype::null: return est::nullopt;
        case dtype::boolean: return est::nullopt;
        case dtype::integer:
            return value_.i >= 0 ? est::make_optional(static_cast<std::uint32_t>(value_.i)) : est::nullopt;
        case dtype::unsigned_integer: return value_.u;
        case dtype::long_integer:
            return value_.i64 >= 0 &&
                           value_.i64 <= static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()) ?
                       est::make_optional(static_cast<std::uint32_t>(value_.i64)) :
                       est::nullopt;
        case dtype::unsigned_long_integer:
            return value_.u64 <= std::numeric_limits<std::uint32_t>::max() ?
                       est::make_optional(static_cast<std::uint32_t>(value_.u64)) :
                       est::nullopt;
        case dtype::double_precision:
            return value_.dbl >= 0 && value_.dbl <= std::numeric_limits<std::uint32_t>::max() ?
                       est::make_optional(static_cast<std::uint32_t>(value_.dbl)) :
                       est::nullopt;
        case dtype::string: {
            est::optional<std::uint32_t> result(est::in_place);
            return from_string_v(value_.str.cview(), *result) ? result : est::nullopt;
        } break;
        case dtype::array: return est::nullopt;
        case dtype::object: return est::nullopt;
        default: UXS_UNREACHABLE_CODE;
    }
}

template<typename CharT, typename Alloc>
est::optional<std::int64_t> basic_value<CharT, Alloc>::get_int64() const {
    switch (type_) {
        case dtype::null: return est::nullopt;
        case dtype::boolean: return est::nullopt;
        case dtype::integer: return value_.i;
        case dtype::unsigned_integer: return static_cast<std::int64_t>(value_.u);
        case dtype::long_integer: return value_.i64;
        case dtype::unsigned_long_integer:
            return value_.u64 <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ?
                       est::make_optional(static_cast<std::int64_t>(value_.u64)) :
                       est::nullopt;
        case dtype::double_precision:
            // Note that double(2^63 - 1) will be rounded up to 2^63, so maximum is excluded
            return value_.dbl >= static_cast<double>(std::numeric_limits<std::int64_t>::min()) &&
                           value_.dbl < static_cast<double>(std::numeric_limits<std::int64_t>::max()) ?
                       est::make_optional(static_cast<std::int64_t>(value_.dbl)) :
                       est::nullopt;
        case dtype::string: {
            est::optional<std::int64_t> result(est::in_place);
            return from_string_v(value_.str.cview(), *result) ? result : est::nullopt;
        } break;
        case dtype::array: return est::nullopt;
        case dtype::object: return est::nullopt;
        default: UXS_UNREACHABLE_CODE;
    }
}

template<typename CharT, typename Alloc>
est::optional<std::uint64_t> basic_value<CharT, Alloc>::get_uint64() const {
    switch (type_) {
        case dtype::null: return est::nullopt;
        case dtype::boolean: return est::nullopt;
        case dtype::integer:
            return value_.i >= 0 ? est::make_optional(static_cast<std::uint64_t>(value_.i)) : est::nullopt;
        case dtype::unsigned_integer: return value_.u;
        case dtype::long_integer:
            return value_.i64 >= 0 ? est::make_optional(static_cast<std::uint64_t>(value_.i64)) : est::nullopt;
        case dtype::unsigned_long_integer: return value_.u64;
        case dtype::double_precision:
            // Note that double(2^64 - 1) will be rounded up to 2^64, so maximum is excluded
            return value_.dbl >= 0 && value_.dbl < static_cast<double>(std::numeric_limits<std::uint64_t>::max()) ?
                       est::make_optional(static_cast<std::uint64_t>(value_.dbl)) :
                       est::nullopt;
        case dtype::string: {
            est::optional<std::uint64_t> result(est::in_place);
            return from_string_v(value_.str.cview(), *result) ? result : est::nullopt;
        } break;
        case dtype::array: return est::nullopt;
        case dtype::object: return est::nullopt;
        default: UXS_UNREACHABLE_CODE;
    }
}

template<typename CharT, typename Alloc>
est::optional<double> basic_value<CharT, Alloc>::get_double() const {
    switch (type_) {
        case dtype::null: return est::nullopt;
        case dtype::boolean: return est::nullopt;
        case dtype::integer: return value_.i;
        case dtype::unsigned_integer: return value_.u;
        case dtype::long_integer: return static_cast<double>(value_.i64);
        case dtype::unsigned_long_integer: return static_cast<double>(value_.u64);
        case dtype::double_precision: return value_.dbl;
        case dtype::string: {
            est::optional<double> result(est::in_place);
            return from_string_v(value_.str.cview(), *result) ? result : est::nullopt;
        } break;
        case dtype::array: return est::nullopt;
        case dtype::object: return est::nullopt;
        default: UXS_UNREACHABLE_CODE;
    }
}

template<typename CharT, typename Alloc>
auto basic_value<CharT, Alloc>::get_string() const -> est::optional<std::basic_string<char_type>> {
    switch (type_) {
        case dtype::null: {
            return est::make_optional<std::basic_string<char_type>>(string_literal<char_type, 'n', 'u', 'l', 'l'>{}());
        } break;
        case dtype::boolean: {
            return est::make_optional<std::basic_string<char_type>>(
                value_.b ? string_literal<char_type, 't', 'r', 'u', 'e'>{}() :
                           string_literal<char_type, 'f', 'a', 'l', 's', 'e'>{}());
        } break;
        case dtype::integer: {
            basic_inline_dynbuffer<char_type> buf;
            sconv::fmt_integer(buf, value_.i);
            return est::make_optional<std::basic_string<char_type>>(buf.data(), buf.size());
        } break;
        case dtype::unsigned_integer: {
            basic_inline_dynbuffer<char_type> buf;
            sconv::fmt_integer(buf, value_.u);
            return est::make_optional<std::basic_string<char_type>>(buf.data(), buf.size());
        } break;
        case dtype::long_integer: {
            basic_inline_dynbuffer<char_type> buf;
            sconv::fmt_integer(buf, value_.i64);
            return est::make_optional<std::basic_string<char_type>>(buf.data(), buf.size());
        } break;
        case dtype::unsigned_long_integer: {
            basic_inline_dynbuffer<char_type> buf;
            sconv::fmt_integer(buf, value_.u64);
            return est::make_optional<std::basic_string<char_type>>(buf.data(), buf.size());
        } break;
        case dtype::double_precision: {
            basic_inline_dynbuffer<char_type> buf;
            sconv::fmt_float(buf, value_.dbl);
            return est::make_optional<std::basic_string<char_type>>(buf.data(), buf.size());
        } break;
        case dtype::string: return est::make_optional<std::basic_string<char_type>>(value_.str.cview());
        case dtype::array: return est::nullopt;
        case dtype::object: return est::nullopt;
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

//-----------------------------------------------------------------------------

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::is_equal_to(const basic_value& other) const noexcept {
    static const auto compare_long_integer = [](std::int64_t lhs, const basic_value& rhs) {
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

    static const auto compare_unsigned_long_integer = [](std::uint64_t lhs, const basic_value& rhs) {
        switch (rhs.type_) {
            case dtype::integer: return rhs.value_.i >= 0 && lhs == static_cast<std::uint64_t>(rhs.value_.i);
            case dtype::unsigned_integer: return lhs == rhs.value_.u;
            case dtype::long_integer: return rhs.value_.i64 >= 0 && lhs == static_cast<std::uint64_t>(rhs.value_.i64);
            case dtype::unsigned_long_integer: return lhs == rhs.value_.u64;
            default: return false;
        }
    };

    switch (type_) {
        case dtype::null: return other.type_ == dtype::null;
        case dtype::boolean: return other.type_ == dtype::boolean && value_.b == other.value_.b;
        case dtype::integer: return compare_long_integer(value_.i, other);
        case dtype::unsigned_integer: return compare_unsigned_long_integer(value_.u, other);
        case dtype::long_integer: return compare_long_integer(value_.i64, other);
        case dtype::unsigned_long_integer: return compare_unsigned_long_integer(value_.u64, other);
        case dtype::double_precision: return other.type_ == dtype::double_precision && value_.dbl == other.value_.dbl;
        case dtype::string: return other.type_ == dtype::string && value_.str.is_equal_to(other.value_.str);
        case dtype::array: return other.type_ == dtype::array && value_.arr.is_equal_to(other.value_.arr);
        case dtype::object: return other.type_ == dtype::object && value_.obj.is_equal_to(other.value_.obj);
        default: UXS_UNREACHABLE_CODE;
    }
}

//-----------------------------------------------------------------------------

template<typename CharT, typename Alloc, bool Const>
void detail::value_iterator_proxy<CharT, Alloc, Const>::create_index_string() const noexcept {
    const std::size_t index = static_cast<const value_type*>(ptr_) - static_cast<const value_type*>(begin_);
    while (lock_.test_and_set(std::memory_order_acquire)) {}
    char_type* end_p = to_chars(index_string_, index);
    *end_p = '\0';
    index_string_len_ = static_cast<std::uint8_t>(end_p - index_string_);
    lock_.clear(std::memory_order_release);
    index_cached_ = true;
}

}  // namespace db
}  // namespace uxs

#define UXS_DB_VALUE_INSTANTIATE_IMPLEMENTATION(char_type, alloc_type) \
    template class uxs::db::detail::flexarray_t<char_type, alloc_type>; \
    template class UXS_EXPORT_ALL_STUFF_FOR_GNUC \
        uxs::db::detail::flexarray_t<uxs::db::basic_value<char_type, alloc_type>, alloc_type>; \
    template class uxs::db::detail::object_t<char_type, alloc_type>; \
    template class uxs::db::detail::object_item<char_type, alloc_type>; \
    template class uxs::db::detail::value_iterator_proxy<char_type, alloc_type, false>; \
    template class uxs::db::detail::value_iterator_proxy<char_type, alloc_type, true>; \
    template class uxs::db::basic_value<char_type, alloc_type>
