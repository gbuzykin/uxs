#pragma once

#include "value.h"

#include "uxs/algorithm.h"
#include "uxs/stringcvt.h"

#include <cmath>
#include <memory>

namespace uxs {
namespace db {

// --------------------------

template<typename CharT, typename Alloc>
bool operator==(const basic_value<CharT, Alloc>& lhs, const basic_value<CharT, Alloc>& rhs) {
    if (lhs.type_ != rhs.type_) { return false; }
    switch (lhs.type_) {
        case dtype::kNull: return true;
        case dtype::kBoolean: return lhs.value_.b == rhs.value_.b;
        case dtype::kInteger: return lhs.value_.i == rhs.value_.i;
        case dtype::kUInteger: return lhs.value_.u == rhs.value_.u;
        case dtype::kInteger64: return lhs.value_.i64 == rhs.value_.i64;
        case dtype::kUInteger64: return lhs.value_.u64 == rhs.value_.u64;
        case dtype::kDouble: return lhs.value_.dbl == rhs.value_.dbl;
        case dtype::kString: return lhs.str_view() == rhs.str_view();
        case dtype::kArray: {
            auto range1 = lhs.as_array(), range2 = rhs.as_array();
            return range1.size() == range2.size() && uxs::equal(range1, range2.begin());
        } break;
        case dtype::kRecord: {
            if (lhs.value_.rec->size != rhs.value_.rec->size) { return false; }
            return uxs::equal(lhs.as_record(), rhs.as_record().begin());
        } break;
        default: break;
    }
    return false;
}

// --------------------------

template<typename CharT, typename Alloc>
template<typename Ty>
/*static*/ size_t basic_value<CharT, Alloc>::flexarray_t<Ty>::max_size(const alloc_type& arr_al) {
    return (std::allocator_traits<alloc_type>::max_size(arr_al) * sizeof(flexarray_t) - offsetof(flexarray_t, buf[0])) /
           sizeof(Ty);
}

template<typename CharT, typename Alloc>
template<typename Ty>
/*static*/ size_t basic_value<CharT, Alloc>::flexarray_t<Ty>::get_alloc_sz(size_t cap) {
    return (offsetof(flexarray_t, buf[cap]) + sizeof(flexarray_t) - 1) / sizeof(flexarray_t);
}

template<typename CharT, typename Alloc>
template<typename Ty>
/*static*/ auto basic_value<CharT, Alloc>::flexarray_t<Ty>::alloc(alloc_type& arr_al, size_t cap) -> flexarray_t<Ty>* {
    const size_t alloc_sz = get_alloc_sz(cap);
    flexarray_t* arr = arr_al.allocate(alloc_sz);
    arr->capacity = (alloc_sz * sizeof(flexarray_t) - offsetof(flexarray_t, buf[0])) / sizeof(Ty);
    assert(arr->capacity >= cap && get_alloc_sz(arr->capacity) == alloc_sz);
    return arr;
}

template<typename CharT, typename Alloc>
template<typename Ty>
/*static*/ auto basic_value<CharT, Alloc>::flexarray_t<Ty>::alloc_checked(alloc_type& arr_al, size_t cap)
    -> flexarray_t<Ty>* {
    if (cap > max_size(arr_al)) { throw std::length_error("too much to reserve"); }
    return alloc(arr_al, cap);
}

template<typename CharT, typename Alloc>
template<typename Ty>
/*static*/ auto basic_value<CharT, Alloc>::flexarray_t<Ty>::grow(alloc_type& arr_al, flexarray_t* arr, size_t extra)
    -> flexarray_t<Ty>* {
    size_t delta_sz = std::max(extra, arr->size >> 1);
    const size_t max_sz = max_size(arr_al);
    if (delta_sz > max_sz - arr->size) {
        if (extra > max_sz - arr->size) { throw std::length_error("too much to reserve"); }
        delta_sz = std::max(extra, (max_sz - arr->size) >> 1);
    }
    flexarray_t* new_arr = alloc(arr_al, std::max<size_t>(arr->size + delta_sz, kStartCapacity));
    basic_value<CharT, Alloc>::move_values(&(*arr)[0], &(*arr)[arr->size], &(*new_arr)[0]);
    new_arr->size = arr->size;
    basic_value<CharT, Alloc>::destruct_moved_values(arr_al, &(*arr)[0], &(*arr)[arr->size]);
    dealloc(arr_al, arr);
    return new_arr;
}

template<typename CharT, typename Alloc>
template<typename Ty>
/*static*/ void basic_value<CharT, Alloc>::flexarray_t<Ty>::dealloc(alloc_type& arr_al, flexarray_t* arr) {
    arr_al.deallocate(arr, get_alloc_sz(arr->capacity));
}

// --------------------------

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::record_t::init() {
    dllist_make_cycle(&head);
    list_node_traits::set_head(&head, &head);
    for (auto& item : as_span(hashtbl, bucket_count)) { item = nullptr; }
    size = 0;
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::record_t::destroy(alloc_type& rec_al, list_links_type* node) {
    while (node != &head) {
        list_links_type* next = node->next;
        delete_node(rec_al, node);
        node = next;
    }
}

template<typename CharT, typename Alloc>
typename basic_value<CharT, Alloc>::list_links_type* basic_value<CharT, Alloc>::record_t::find(
    std::basic_string_view<char_type> name, size_t hash_code) const {
    list_links_type* bucket_next = hashtbl[hash_code % bucket_count];
    while (bucket_next) {
        if (list_node_type::from_links(bucket_next)->hash_code == hash_code &&
            list_node_traits::get_value(bucket_next).name() == name) {
            return bucket_next;
        }
        bucket_next = list_node_type::from_links(bucket_next)->bucket_next;
    }
    return &head;
}

template<typename CharT, typename Alloc>
size_t basic_value<CharT, Alloc>::record_t::count(std::basic_string_view<char_type> name) const {
    size_t count = 0;
    const size_t hash_code = calc_hash_code(name);
    list_links_type* bucket_next = hashtbl[hash_code % bucket_count];
    while (bucket_next) {
        if (list_node_type::from_links(bucket_next)->hash_code == hash_code &&
            list_node_traits::get_value(bucket_next).name() == name) {
            ++count;
        }
        bucket_next = list_node_type::from_links(bucket_next)->bucket_next;
    }
    return count;
}

template<typename CharT, typename Alloc>
/*static*/ typename basic_value<CharT, Alloc>::record_t* basic_value<CharT, Alloc>::record_t::create(
    alloc_type& rec_al, const std::initializer_list<basic_value>& init) {
    record_t* rec = alloc(rec_al, init.size() ? std::min(init.size(), max_size(rec_al)) : 1);
    rec->init();
    try {
        uxs::for_each(init, [&rec_al, &rec](const basic_value& v) {
            std::basic_string_view<char_type> name = (*v.value_.arr)[0].str_view();
            list_links_type* node = rec->new_node(rec_al, name, (*v.value_.arr)[1]);
            rec = insert(rec_al, rec, calc_hash_code(name), node);
        });
        return rec;
    } catch (...) {
        rec->destroy(rec_al, rec->head.next);
        dealloc(rec_al, rec);
        throw;
    }
}

template<typename CharT, typename Alloc>
/*static*/ typename basic_value<CharT, Alloc>::record_t* basic_value<CharT, Alloc>::record_t::create(
    alloc_type& rec_al, const std::initializer_list<std::pair<std::basic_string_view<char_type>, basic_value>>& init) {
    record_t* rec = alloc(rec_al, init.size() ? std::min(init.size(), max_size(rec_al)) : 1);
    rec->init();
    try {
        uxs::for_each(init, [&rec_al, &rec](const std::pair<std::basic_string_view<char_type>, basic_value>& v) {
            list_links_type* node = rec->new_node(rec_al, v.first, v.second);
            rec = insert(rec_al, rec, calc_hash_code(v.first), node);
        });
        return rec;
    } catch (...) {
        rec->destroy(rec_al, rec->head.next);
        dealloc(rec_al, rec);
        throw;
    }
}

template<typename CharT, typename Alloc>
/*static*/ typename basic_value<CharT, Alloc>::record_t* basic_value<CharT, Alloc>::record_t::create(
    alloc_type& rec_al, const record_t& src) {
    record_t* rec = alloc(rec_al, src.size ? std::min(src.size, max_size(rec_al)) : 0);
    rec->init();
    try {
        list_links_type* node = src.head.next;
        while (node != &src.head) {
            const auto& v = list_node_traits::get_value(node);
            rec = insert(rec_al, rec, list_node_type::from_links(node)->hash_code,
                         rec->new_node(rec_al, v.name(), v.val()));
            node = node->next;
        }
        return rec;
    } catch (...) {
        rec->destroy(rec_al, rec->head.next);
        dealloc(rec_al, rec);
        throw;
    }
}

template<typename CharT, typename Alloc>
/*static*/ typename basic_value<CharT, Alloc>::record_t* basic_value<CharT, Alloc>::record_t::assign(
    alloc_type& rec_al, record_t* rec, const record_t& src) {
    rec->clear(rec_al);
    list_links_type* node = src.head.next;
    while (node != &src.head) {
        const auto& v = list_node_traits::get_value(node);
        rec = insert(rec_al, rec, list_node_type::from_links(node)->hash_code, rec->new_node(rec_al, v.name(), v.val()));
        node = node->next;
    }
    return rec;
}

template<typename CharT, typename Alloc>
/*static*/ size_t basic_value<CharT, Alloc>::record_t::calc_hash_code(std::basic_string_view<char_type> name) {
    // Implementation of Murmur hash for 64-bit size_t is taken from GNU libstdc++
    const size_t seed = 0xc70f6907;
    const size_t sz = name.size() * sizeof(CharT);
    const uint8_t* data = reinterpret_cast<const uint8_t*>(name.data());
#if UINTPTR_MAX == 0xffffffffffffffff
    const size_t mul = 0xc6a4a7935bd1e995ull;
    const uint8_t* end0 = data + (sz & ~7);
    size_t hash = seed ^ (sz * mul);
    auto shift_mix = [](size_t v) { return v ^ (v >> 47); };
    for (const uint8_t* p = data; p != end0; p += 8) {
        size_t data;
        std::memcpy(&data, p, 8);  // unaligned load
        hash ^= shift_mix(data * mul) * mul;
        hash *= mul;
    }
    if (sz & 7) {
        size_t a = 0;
        const uint8_t* end = data + sz;
        do { a = (a << 8) + *--end; } while (end != end0);
        hash ^= a;
        hash *= mul;
    }
    hash = shift_mix(shift_mix(hash) * mul);
#else
    size_t hash = seed;
    for (uint8_t ch : as_span(data, sz)) { hash = (hash * 131) + ch; }
#endif
    return hash;
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::record_t::add_to_hash(list_links_type* node, size_t hash_code) {
    list_links_type** slot = &hashtbl[hash_code % bucket_count];
    list_node_type::from_links(node)->bucket_next = *slot;
    *slot = node;
}

template<typename CharT, typename Alloc>
/*static*/ typename basic_value<CharT, Alloc>::record_t* basic_value<CharT, Alloc>::record_t::insert(
    alloc_type& rec_al, record_t* rec, size_t hash_code, list_links_type* node) {
    assert(rec->size <= rec->bucket_count);
    if (rec->size == rec->bucket_count) {
        const size_t sz = next_bucket_count(rec_al, rec->size);
        if (sz > rec->size) { rec = record_t::rehash(rec_al, rec, sz); }
    }
    list_node_traits::set_head(node, &rec->head);
    list_node_type::from_links(node)->hash_code = hash_code;
    rec->add_to_hash(node, hash_code);
    dllist_insert_before(&rec->head, node);
    ++rec->size;
    return rec;
}

template<typename CharT, typename Alloc>
typename basic_value<CharT, Alloc>::list_links_type*  //
basic_value<CharT, Alloc>::record_t::erase(alloc_type& rec_al, list_links_type* node) {
    list_links_type** p_bucket_next = &hashtbl[list_node_type::from_links(node)->hash_code % bucket_count];
    while (*p_bucket_next != node) {
        assert(*p_bucket_next);
        p_bucket_next = &list_node_type::from_links(*p_bucket_next)->bucket_next;
    }
    *p_bucket_next = list_node_type::from_links(*p_bucket_next)->bucket_next;
    list_links_type* next = dllist_remove(node);
    delete_node(rec_al, node);
    --size;
    return next;
}

template<typename CharT, typename Alloc>
size_t basic_value<CharT, Alloc>::record_t::erase(alloc_type& rec_al, std::basic_string_view<char_type> name) {
    size_t old_size = size;
    const size_t hash_code = calc_hash_code(name);
    list_links_type** p_bucket_next = &hashtbl[hash_code % bucket_count];
    while (*p_bucket_next) {
        if (list_node_type::from_links(*p_bucket_next)->hash_code == hash_code &&
            list_node_traits::get_value(*p_bucket_next).name() == name) {
            list_links_type* erased_node = *p_bucket_next;
            *p_bucket_next = list_node_type::from_links(*p_bucket_next)->bucket_next;
            --size;
            dllist_remove(erased_node);
            delete_node(rec_al, erased_node);
        } else {
            p_bucket_next = &list_node_type::from_links(*p_bucket_next)->bucket_next;
        }
    }
    return old_size - size;
}

template<typename CharT, typename Alloc>
/*static*/ size_t basic_value<CharT, Alloc>::record_t::max_size(const alloc_type& rec_al) {
    return (std::allocator_traits<alloc_type>::max_size(rec_al) * sizeof(record_t) - offsetof(record_t, hashtbl[0])) /
           sizeof(list_links_type*);
}

template<typename CharT, typename Alloc>
/*static*/ size_t basic_value<CharT, Alloc>::record_t::get_alloc_sz(size_t bckt_cnt) {
    return (offsetof(record_t, hashtbl[bckt_cnt]) + sizeof(record_t) - 1) / sizeof(record_t);
}

template<typename CharT, typename Alloc>
/*static*/ size_t basic_value<CharT, Alloc>::record_t::next_bucket_count(const alloc_type& rec_al, size_t sz) {
    size_t delta_sz = std::max<size_t>(kMinBucketCountInc, sz >> 1);
    const size_t max_size = (std::allocator_traits<alloc_type>::max_size(rec_al) * sizeof(record_t) -
                             offsetof(record_t, hashtbl[0])) /
                            sizeof(list_links_type*);
    if (delta_sz > max_size - sz) {
        if (sz == max_size) { return sz; }
        delta_sz = std::max<size_t>(1, (max_size - sz) >> 1);
    }
    return sz + delta_sz;
}

template<typename CharT, typename Alloc>
/*static*/ typename basic_value<CharT, Alloc>::record_t*  //
basic_value<CharT, Alloc>::record_t::alloc(alloc_type& rec_al, size_t bckt_cnt) {
    const size_t alloc_sz = get_alloc_sz(bckt_cnt);
    record_t* rec = rec_al.allocate(alloc_sz);
    rec->bucket_count = (alloc_sz * sizeof(record_t) - offsetof(record_t, hashtbl[0])) / sizeof(list_links_type*);
    assert(rec->bucket_count >= bckt_cnt && get_alloc_sz(rec->bucket_count) == alloc_sz);
    return rec;
}

template<typename CharT, typename Alloc>
/*static*/ typename basic_value<CharT, Alloc>::record_t*  //
basic_value<CharT, Alloc>::record_t::rehash(alloc_type& rec_al, record_t* rec, size_t bckt_cnt) {
    assert(rec->size);
    record_t* new_rec = alloc(rec_al, bckt_cnt);
    new_rec->head = rec->head;
    new_rec->size = rec->size;
    dealloc(rec_al, rec);
    list_node_traits::set_head(&new_rec->head, &new_rec->head);
    for (auto& item : as_span(new_rec->hashtbl, new_rec->bucket_count)) { item = nullptr; }
    list_links_type* node = new_rec->head.next;
    node->prev = &new_rec->head;
    new_rec->head.prev->next = &new_rec->head;
    while (node != &new_rec->head) {
        list_node_traits::set_head(node, &new_rec->head);
        new_rec->add_to_hash(node, list_node_type::from_links(node)->hash_code);
        node = node->next;
    }
    return new_rec;
}

template<typename CharT, typename Alloc>
/*static*/ void basic_value<CharT, Alloc>::record_t::dealloc(alloc_type& rec_al, record_t* rec) {
    rec_al.deallocate(rec, get_alloc_sz(rec->bucket_count));
}

template<typename CharT, typename Alloc>
/*static*/ size_t basic_value<CharT, Alloc>::list_node_type::max_name_size(const alloc_type& node_al) {
    return (std::allocator_traits<alloc_type>::max_size(node_al) * sizeof(list_node_type) -
            offsetof(list_node_type, v.name_chars[0])) /
           sizeof(char_type);
}

template<typename CharT, typename Alloc>
/*static*/ size_t basic_value<CharT, Alloc>::list_node_type::get_alloc_sz(size_t name_sz) {
    return (offsetof(list_node_type, v.name_chars[name_sz]) + sizeof(list_node_type) - 1) / sizeof(list_node_type);
}

template<typename CharT, typename Alloc>
/*static*/ typename basic_value<CharT, Alloc>::list_node_type*  //
basic_value<CharT, Alloc>::list_node_type::alloc_checked(alloc_type& node_al, std::basic_string_view<char_type> name) {
    if (name.size() > max_name_size(node_al)) { throw std::length_error("too much to reserve"); }
    const size_t alloc_sz = get_alloc_sz(name.size());
    list_node_type* node = node_al.allocate(alloc_sz);
    node->v.name_sz = name.size();
    std::copy_n(name.data(), name.size(), static_cast<char_type*>(node->v.name_chars));
    return node;
}

template<typename CharT, typename Alloc>
/*static*/ void basic_value<CharT, Alloc>::list_node_type::dealloc(alloc_type& node_al, list_node_type* node) {
    node_al.deallocate(node, get_alloc_sz(node->v.name_sz));
}

// --------------------------

namespace detail {
template<typename CharT, typename Alloc>
bool is_record(const std::initializer_list<basic_value<CharT, Alloc>>& init) {
    return uxs::all_of(
        init, [](const basic_value<CharT, Alloc>& v) { return v.is_array() && v.size() == 2 && v[0].is_string(); });
}
}  // namespace detail

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc>::basic_value(std::initializer_list<basic_value> init, const Alloc& al)
    : alloc_type(al), type_(dtype::kArray) {
    if (detail::is_record(init)) {
        typename record_t::alloc_type rec_al(*this);
        value_.rec = record_t::create(rec_al, init);
        type_ = dtype::kRecord;
    } else {
        value_.arr = alloc_array(init.size(), init.begin());
    }
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::assign(std::initializer_list<basic_value> init) {
    if (detail::is_record(init)) {
        typename record_t::alloc_type rec_al(*this);
        if (type_ != dtype::kRecord) {
            if (type_ != dtype::kNull) { destroy(); }
            value_.rec = record_t::create(rec_al, init);
            type_ = dtype::kRecord;
        } else {
            value_.rec->clear(rec_al);
            uxs::for_each(init, [this, &rec_al](const basic_value& v) {
                std::basic_string_view<char_type> name = (*v.value_.arr)[0].str_view();
                list_links_type* node = value_.rec->new_node(rec_al, name, (*v.value_.arr)[1]);
                value_.rec = record_t::insert(rec_al, value_.rec, record_t::calc_hash_code(name), node);
            });
        }
    } else if (type_ != dtype::kArray) {
        if (type_ != dtype::kNull) { destroy(); }
        value_.arr = alloc_array(init.size(), init.begin());
        type_ = dtype::kArray;
    } else {
        assign_array(init.size(), init.begin());
    }
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::insert(size_t pos, std::initializer_list<basic_value> init) {
    if (type_ != dtype::kArray) {
        if (type_ != dtype::kNull) { throw exception("not an array"); }
        value_.arr = alloc_array(init.size(), init.begin());
        type_ = dtype::kArray;
    } else if (init.size()) {
        append_array(init.size(), init.begin());
        if (pos < value_.arr->size - init.size()) {
            std::rotate(&(*value_.arr)[pos], &(*value_.arr)[value_.arr->size - init.size()],
                        &(*value_.arr)[value_.arr->size]);
        }
    }
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::insert(
    std::initializer_list<std::pair<std::basic_string_view<char_type>, basic_value>> init) {
    typename record_t::alloc_type rec_al(*this);
    if (type_ != dtype::kRecord) {
        if (type_ != dtype::kNull) { throw exception("not a record"); }
        value_.rec = record_t::create(rec_al, init);
        type_ = dtype::kRecord;
    } else {
        uxs::for_each(init, [this, &rec_al](const std::pair<std::basic_string_view<char_type>, basic_value>& v) {
            list_links_type* node = value_.rec->new_node(rec_al, v.first, v.second);
            value_.rec = record_t::insert(rec_al, value_.rec, record_t::calc_hash_code(v.first), node);
        });
    }
}

// --------------------------

namespace detail {
inline bool is_integral(double d) {
    double integral_part;
    return std::modf(d, &integral_part) == 0.;
}
}  // namespace detail

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::as_bool(bool& res) const {
    switch (type_) {
        case dtype::kNull: return false;
        case dtype::kBoolean: res = value_.b; return true;
        case dtype::kInteger: res = !!value_.i; return true;
        case dtype::kUInteger: res = !!value_.u; return true;
        case dtype::kInteger64: res = !!value_.i64; return true;
        case dtype::kUInteger64: res = !!value_.u64; return true;
        case dtype::kDouble: res = value_.dbl != 0.; return true;
        case dtype::kString: return basic_stoval(str_view(), res) != 0;
        case dtype::kArray: return false;
        case dtype::kRecord: return false;
        default: UNREACHABLE_CODE;
    }
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::as_int(int32_t& res) const {
    switch (type_) {
        case dtype::kNull: return false;
        case dtype::kBoolean: res = value_.b ? 1 : 0; return true;
        case dtype::kInteger: res = value_.i; return true;
        case dtype::kUInteger: {
            if (value_.u <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
                res = static_cast<int32_t>(value_.u);
                return true;
            }
        } break;
        case dtype::kInteger64: {
            if (value_.i64 >= std::numeric_limits<int32_t>::min() && value_.i64 <= std::numeric_limits<int32_t>::max()) {
                res = static_cast<int32_t>(value_.i64);
                return true;
            }
        } break;
        case dtype::kUInteger64: {
            if (value_.u64 <= static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
                res = static_cast<int32_t>(value_.u64);
                return true;
            }
        } break;
        case dtype::kDouble: {
            if (value_.dbl >= std::numeric_limits<int32_t>::min() && value_.dbl <= std::numeric_limits<int32_t>::max()) {
                res = static_cast<int32_t>(value_.dbl);
                return true;
            }
        } break;
        case dtype::kString: return basic_stoval(str_view(), res) != 0;
        case dtype::kArray: return false;
        case dtype::kRecord: return false;
        default: UNREACHABLE_CODE;
    }
    return false;
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::as_uint(uint32_t& res) const {
    switch (type_) {
        case dtype::kNull: return false;
        case dtype::kBoolean: res = value_.b ? 1 : 0; return true;
        case dtype::kInteger: {
            if (value_.i >= 0) {
                res = static_cast<uint32_t>(value_.i);
                return true;
            }
        } break;
        case dtype::kUInteger: res = value_.u; return true;
        case dtype::kInteger64: {
            if (value_.i64 >= 0 && value_.i64 <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
                res = static_cast<uint32_t>(value_.i64);
                return true;
            }
        } break;
        case dtype::kUInteger64: {
            if (value_.u64 <= std::numeric_limits<uint32_t>::max()) {
                res = static_cast<uint32_t>(value_.u64);
                return true;
            }
        } break;
        case dtype::kDouble: {
            if (value_.dbl >= 0 && value_.dbl <= std::numeric_limits<uint32_t>::max()) {
                res = static_cast<uint32_t>(value_.dbl);
                return true;
            }
        } break;
        case dtype::kString: return basic_stoval(str_view(), res) != 0;
        case dtype::kArray: return false;
        case dtype::kRecord: return false;
        default: UNREACHABLE_CODE;
    }
    return false;
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::as_int64(int64_t& res) const {
    switch (type_) {
        case dtype::kNull: return false;
        case dtype::kBoolean: res = value_.b ? 1 : 0; return true;
        case dtype::kInteger: res = value_.i; return true;
        case dtype::kUInteger: res = static_cast<int64_t>(value_.u); return true;
        case dtype::kInteger64: res = value_.i64; return true;
        case dtype::kUInteger64: {
            if (value_.u64 <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                res = static_cast<int64_t>(value_.u64);
                return true;
            }
        } break;
        case dtype::kDouble: {
            // Note that double(2^63 - 1) will be rounded up to 2^63, so maximum is excluded
            if (value_.dbl >= static_cast<double>(std::numeric_limits<int64_t>::min()) &&
                value_.dbl < static_cast<double>(std::numeric_limits<int64_t>::max())) {
                res = static_cast<int64_t>(value_.dbl);
                return true;
            }
        } break;
        case dtype::kString: return basic_stoval(str_view(), res) != 0;
        case dtype::kArray: return false;
        case dtype::kRecord: return false;
        default: UNREACHABLE_CODE;
    }
    return false;
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::as_uint64(uint64_t& res) const {
    switch (type_) {
        case dtype::kNull: return false;
        case dtype::kBoolean: res = value_.b ? 1 : 0; return true;
        case dtype::kInteger: {
            if (value_.i >= 0) {
                res = static_cast<uint64_t>(value_.i);
                return true;
            }
        } break;
        case dtype::kUInteger: res = value_.u; return true;
        case dtype::kInteger64: {
            if (value_.i64 >= 0) {
                res = static_cast<uint64_t>(value_.i64);
                return true;
            }
        } break;
        case dtype::kUInteger64: res = value_.u64; return true;
        case dtype::kDouble: {
            // Note that double(2^64 - 1) will be rounded up to 2^64, so maximum is excluded
            if (value_.dbl >= 0 && value_.dbl < static_cast<double>(std::numeric_limits<uint64_t>::max())) {
                res = static_cast<uint64_t>(value_.dbl);
                return true;
            }
        } break;
        case dtype::kString: return basic_stoval(str_view(), res) != 0;
        case dtype::kArray: return false;
        case dtype::kRecord: return false;
        default: UNREACHABLE_CODE;
    }
    return false;
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::as_float(float& res) const {
    switch (type_) {
        case dtype::kNull: return false;
        case dtype::kBoolean: res = value_.b ? 1.f : 0.f; return true;
        case dtype::kInteger: res = static_cast<float>(value_.i); return true;
        case dtype::kUInteger: res = static_cast<float>(value_.u); return true;
        case dtype::kInteger64: res = static_cast<float>(value_.i64); return true;
        case dtype::kUInteger64: res = static_cast<float>(value_.u64); return true;
        case dtype::kDouble: res = static_cast<float>(value_.dbl); return true;
        case dtype::kString: return basic_stoval(str_view(), res) != 0;
        case dtype::kArray: return false;
        case dtype::kRecord: return false;
        default: UNREACHABLE_CODE;
    }
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::as_double(double& res) const {
    switch (type_) {
        case dtype::kNull: return false;
        case dtype::kBoolean: res = value_.b ? 1. : 0.; return true;
        case dtype::kInteger: res = static_cast<double>(value_.i); return true;
        case dtype::kUInteger: res = static_cast<double>(value_.u); return true;
        case dtype::kInteger64: res = static_cast<double>(value_.i64); return true;
        case dtype::kUInteger64: res = static_cast<double>(value_.u64); return true;
        case dtype::kDouble: res = value_.dbl; return true;
        case dtype::kString: return basic_stoval(str_view(), res) != 0;
        case dtype::kArray: return false;
        case dtype::kRecord: return false;
        default: UNREACHABLE_CODE;
    }
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::as_string(std::basic_string<char_type>& res) const {
    switch (type_) {
        case dtype::kNull: {
            auto sval = std::make_pair("null", 4u);
            res.assign(sval.first, sval.first + sval.second);
            return true;
        } break;
        case dtype::kBoolean: {
            auto sval = value_.b ? std::make_pair("true", 4u) : std::make_pair("false", 5u);
            res.assign(sval.first, sval.first + sval.second);
            return true;
        } break;
        case dtype::kInteger: {
            basic_inline_dynbuffer<CharT> buf;
            to_basic_string(buf.base(), value_.i, {});
            res.assign(buf.data(), buf.size());
            return true;
        } break;
        case dtype::kUInteger: {
            basic_inline_dynbuffer<CharT> buf;
            to_basic_string(buf.base(), value_.u, {});
            res.assign(buf.data(), buf.size());
            return true;
        } break;
        case dtype::kInteger64: {
            basic_inline_dynbuffer<CharT> buf;
            to_basic_string(buf.base(), value_.i64, {});
            res.assign(buf.data(), buf.size());
            return true;
        } break;
        case dtype::kUInteger64: {
            basic_inline_dynbuffer<CharT> buf;
            to_basic_string(buf.base(), value_.u64, {});
            res.assign(buf.data(), buf.size());
            return true;
        } break;
        case dtype::kDouble: {
            basic_inline_dynbuffer<CharT> buf;
            to_basic_string(buf.base(), value_.dbl, fmt_flags::kAlternate);
            res.assign(buf.data(), buf.size());
            return true;
        } break;
        case dtype::kString: {
            res = std::basic_string<char_type>(str_view());
            return true;
        } break;
        case dtype::kArray: return false;
        case dtype::kRecord: return false;
        default: UNREACHABLE_CODE;
    }
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::as_string_view(std::basic_string_view<char_type>& res) const {
    if (type_ != dtype::kString) { return false; }
    res = str_view();
    return true;
}

// --------------------------

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::is_int() const {
    switch (type_) {
        case dtype::kInteger: return true;
        case dtype::kUInteger: return value_.u <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max());
        case dtype::kInteger64:
            return value_.i64 >= std::numeric_limits<int32_t>::min() &&
                   value_.i64 <= std::numeric_limits<int32_t>::max();
        case dtype::kUInteger64: return value_.u64 <= static_cast<uint64_t>(std::numeric_limits<int32_t>::max());
        case dtype::kDouble:
            return value_.dbl >= std::numeric_limits<int32_t>::min() &&
                   value_.dbl <= std::numeric_limits<int32_t>::max() && detail::is_integral(value_.dbl);
        default: break;
    }
    return false;
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::is_uint() const {
    switch (type_) {
        case dtype::kInteger: return value_.i >= 0;
        case dtype::kUInteger: return true;
        case dtype::kInteger64:
            return value_.i64 >= 0 && value_.i64 <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max());
        case dtype::kUInteger64: return value_.u64 <= std::numeric_limits<uint32_t>::max();
        case dtype::kDouble:
            return value_.dbl >= 0 && value_.dbl <= std::numeric_limits<uint32_t>::max() &&
                   detail::is_integral(value_.dbl);
        default: break;
    }
    return false;
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::is_int64() const {
    switch (type_) {
        case dtype::kInteger:
        case dtype::kUInteger:
        case dtype::kInteger64: return true;
        case dtype::kUInteger64: return value_.u64 <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
        case dtype::kDouble:
            // Note that double(2^63 - 1) will be rounded up to 2^63, so maximum is excluded
            return value_.dbl >= static_cast<double>(std::numeric_limits<int64_t>::min()) &&
                   value_.dbl < static_cast<double>(std::numeric_limits<int64_t>::max()) &&
                   detail::is_integral(value_.dbl);
        default: break;
    }
    return false;
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::is_uint64() const {
    switch (type_) {
        case dtype::kInteger: return value_.i >= 0;
        case dtype::kUInteger: return true;
        case dtype::kInteger64: return value_.i64 >= 0;
        case dtype::kUInteger64: return true;
        case dtype::kDouble:
            // Note that double(2^64 - 1) will be rounded up to 2^64, so maximum is excluded
            return value_.dbl >= 0 && value_.dbl < static_cast<double>(std::numeric_limits<uint64_t>::max()) &&
                   detail::is_integral(value_.dbl);
        default: break;
    }
    return false;
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::is_integral() const {
    switch (type_) {
        case dtype::kInteger:
        case dtype::kUInteger:
        case dtype::kInteger64:
        case dtype::kUInteger64: return true;
        case dtype::kDouble:
            // Note that double(2^64 - 1) will be rounded up to 2^64, so maximum is excluded
            return value_.dbl >= static_cast<double>(std::numeric_limits<int64_t>::min()) &&
                   value_.dbl < static_cast<double>(std::numeric_limits<uint64_t>::max()) &&
                   detail::is_integral(value_.dbl);
        default: break;
    }
    return false;
}

// --------------------------

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::convert(dtype type) {
    if (type == type_) { return true; }
    switch (type) {
        case dtype::kNull: *this = basic_value(); break;
        case dtype::kBoolean: *this = as_bool(); break;
        case dtype::kInteger: *this = as_int(); break;
        case dtype::kUInteger: *this = as_uint(); break;
        case dtype::kInteger64: *this = as_int64(); break;
        case dtype::kUInteger64: *this = as_uint64(); break;
        case dtype::kDouble: *this = as_double(); break;
        case dtype::kString: *this = as_string(); break;
        case dtype::kArray: {
            if (type_ != dtype::kNull) {
                typename value_flexarray_t::alloc_type arr_al(*this);
                value_flexarray_t* arr = value_flexarray_t::alloc(arr_al, 1);
                basic_value* p = &(*arr)[0];
                new (p) basic_value(static_cast<const Alloc&>(*this));
                p->type_ = type_, p->value_ = value_;
                arr->size = 1;
                value_.arr = arr;
            } else {
                value_.arr = nullptr;
            }
            type_ = dtype::kArray;
        } break;
        case dtype::kRecord: {
            if (type_ != dtype::kNull) { return false; }
            typename record_t::alloc_type rec_al(*this);
            value_.rec = record_t::create(rec_al);
            type_ = dtype::kRecord;
        } break;
        default: UNREACHABLE_CODE;
    }
    return true;
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::empty() const NOEXCEPT {
    switch (type_) {
        case dtype::kNull: return true;
        case dtype::kArray: return !value_.arr || !value_.arr->size;
        case dtype::kRecord: return !value_.rec->size;
        default: break;
    }
    return false;
}

template<typename CharT, typename Alloc>
size_t basic_value<CharT, Alloc>::size() const NOEXCEPT {
    switch (type_) {
        case dtype::kNull: return 0;
        case dtype::kArray: return value_.arr ? value_.arr->size : 0;
        case dtype::kRecord: return value_.rec->size;
        default: break;
    }
    return 1;
}

template<typename CharT, typename Alloc>
const basic_value<CharT, Alloc>& basic_value<CharT, Alloc>::operator[](std::basic_string_view<char_type> name) const {
    static const basic_value null;
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    list_links_type* node = value_.rec->find(name, record_t::calc_hash_code(name));
    return node != &value_.rec->head ? list_node_traits::get_value(node).val() : null;
}

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc>& basic_value<CharT, Alloc>::operator[](std::basic_string_view<char_type> name) {
    typename record_t::alloc_type rec_al(*this);
    if (type_ != dtype::kRecord) {
        if (type_ != dtype::kNull) { throw exception("not a record"); }
        value_.rec = record_t::create(rec_al);
        type_ = dtype::kRecord;
    }
    const size_t hash_code = record_t::calc_hash_code(name);
    list_links_type* node = value_.rec->find(name, hash_code);
    if (node == &value_.rec->head) {
        node = value_.rec->new_node(rec_al, name, static_cast<const Alloc&>(*this));
        value_.rec = record_t::insert(rec_al, value_.rec, hash_code, node);
    }
    return list_node_traits::get_value(node).val();
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::clear() NOEXCEPT {
    if (type_ == dtype::kRecord) {
        typename record_t::alloc_type rec_al(*this);
        value_.rec->clear(rec_al);
    } else if (type_ == dtype::kArray && value_.arr) {
        uxs::for_each(value_.arr->view(), [](basic_value& v) { v.~basic_value(); });
        value_.arr->size = 0;
    }
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::reserve(size_t sz) {
    if (type_ != dtype::kArray || !value_.arr) {
        if (type_ != dtype::kArray && type_ != dtype::kNull) { throw exception("not an array"); }
        if (sz) {
            typename value_flexarray_t::alloc_type arr_al(*this);
            value_.arr = value_flexarray_t::alloc_checked(arr_al, sz);
            value_.arr->size = 0;
        } else {
            value_.arr = nullptr;
        }
        type_ = dtype::kArray;
    } else if (sz > value_.arr->capacity) {
        typename value_flexarray_t::alloc_type arr_al(*this);
        value_.arr = value_flexarray_t::grow(arr_al, value_.arr, sz - value_.arr->size);
    }
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::resize(size_t sz) {
    reserve(sz);
    if (!value_.arr) {
        return;
    } else if (sz <= value_.arr->size) {
        std::for_each(&(*value_.arr)[sz], &(*value_.arr)[value_.arr->size], [](basic_value& v) { v.~basic_value(); });
    } else {
        std::for_each(&(*value_.arr)[value_.arr->size], &(*value_.arr)[sz],
                      [this](basic_value& v) { new (&v) basic_value(static_cast<const Alloc&>(*this)); });
    }
    value_.arr->size = sz;
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::erase(size_t pos) {
    if (type_ != dtype::kArray) { throw exception("not an array"); }
    assert(value_.arr && pos < value_.arr->size);
    basic_value* v = &(*value_.arr)[pos];
    basic_value* v_end = &(*value_.arr)[--value_.arr->size];
    for (; v != v_end; ++v) { *v = std::move(*(v + 1)); }
    v->~basic_value();
}

template<typename CharT, typename Alloc>
typename basic_value<CharT, Alloc>::record_iterator basic_value<CharT, Alloc>::erase(const_record_iterator it) {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    list_links_type* node = it.node();
    iterator_assert(list_node_traits::get_head(node) == &value_.rec->head);
    assert(node != &value_.rec->head);
    typename record_t::alloc_type rec_al(*this);
    return record_iterator(value_.rec->erase(rec_al, node));
}

template<typename CharT, typename Alloc>
size_t basic_value<CharT, Alloc>::erase(std::basic_string_view<char_type> name) {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    typename record_t::alloc_type rec_al(*this);
    return value_.rec->erase(rec_al, name);
}

// --------------------------

template<typename CharT, typename Alloc>
auto basic_value<CharT, Alloc>::alloc_string(std::basic_string_view<char_type> s) -> char_flexarray_t* {
    if (!s.size()) { return nullptr; }
    typename char_flexarray_t::alloc_type arr_al(*this);
    char_flexarray_t* str = char_flexarray_t::alloc_checked(arr_al, s.size());
    str->size = s.size();
    std::copy_n(s.data(), s.size(), &(*str)[0]);
    return str;
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::assign_string(std::basic_string_view<char_type> s) {
    if (!value_.str || s.size() > value_.str->capacity) {
        if (!s.size()) { return; }
        typename char_flexarray_t::alloc_type arr_al(*this);
        char_flexarray_t* new_str = char_flexarray_t::alloc_checked(arr_al, s.size());
        if (value_.str) { char_flexarray_t::dealloc(arr_al, value_.str); }
        value_.str = new_str;
    }
    value_.str->size = s.size();
    std::copy_n(s.data(), s.size(), &(*value_.str)[0]);
}

// --------------------------

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::init_from(const basic_value& other) {
    switch (other.type_) {
        case dtype::kString: value_.str = alloc_string(other.str_view()); break;
        case dtype::kArray: value_.arr = alloc_array(other.array_view()); break;
        case dtype::kRecord: {
            typename record_t::alloc_type rec_al(*this);
            value_.rec = record_t::create(rec_al, *other.value_.rec);
        } break;
        default: value_ = other.value_; break;
    }
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::assign_impl(const basic_value& other) {
    if (type_ != other.type_) {
        if (type_ != dtype::kNull) { destroy(); }
        init_from(other);
        type_ = other.type_;
    } else {
        switch (other.type_) {
            case dtype::kString: assign_string(other.str_view()); break;
            case dtype::kArray: assign_array(other.array_view()); break;
            case dtype::kRecord: {
                typename record_t::alloc_type rec_al(*this);
                value_.rec = record_t::assign(rec_al, value_.rec, *other.value_.rec);
            } break;
            default: value_ = other.value_; break;
        }
    }
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::destroy() {
    switch (type_) {
        case dtype::kString: {
            if (value_.str) {
                typename char_flexarray_t::alloc_type arr_al(*this);
                char_flexarray_t::dealloc(arr_al, value_.str);
            }
        } break;
        case dtype::kArray: {
            if (value_.arr) {
                typename value_flexarray_t::alloc_type arr_al(*this);
                uxs::for_each(value_.arr->view(), [](basic_value& v) { v.~basic_value(); });
                value_flexarray_t::dealloc(arr_al, value_.arr);
            }
        } break;
        case dtype::kRecord: {
            typename record_t::alloc_type rec_al(*this);
            value_.rec->destroy(rec_al, value_.rec->head.next);
            record_t::dealloc(rec_al, value_.rec);
        } break;
        default: break;
    }
    type_ = dtype::kNull;
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::reserve_back() {
    typename value_flexarray_t::alloc_type arr_al(*this);
    if (type_ != dtype::kArray || !value_.arr) {
        if (type_ != dtype::kArray && type_ != dtype::kNull) { throw exception("not an array"); }
        value_.arr = value_flexarray_t::alloc(arr_al, kStartCapacity);
        value_.arr->size = 0;
        type_ = dtype::kArray;
    } else {
        value_.arr = value_flexarray_t::grow(arr_al, value_.arr, 1);
    }
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::reserve_string(size_t extra) {
    typename char_flexarray_t::alloc_type arr_al(*this);
    if (type_ != dtype::kString || !value_.str) {
        if (type_ != dtype::kString && type_ != dtype::kNull) { throw exception("not a string"); }
        value_.str = char_flexarray_t::alloc_checked(arr_al, extra);
        value_.str->size = 0;
        type_ = dtype::kString;
    } else {
        value_.str = char_flexarray_t::grow(arr_al, value_.str, extra);
    }
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::rotate_back(size_t pos) {
    assert(pos != value_.arr->size - 1);
    basic_value* v = &(*value_.arr)[value_.arr->size];
    basic_value t(std::move(*(v - 1)));
    while (--v != &(*value_.arr)[pos]) { *v = std::move(*(v - 1)); }
    *v = std::move(t);
}

}  // namespace db
}  // namespace uxs
