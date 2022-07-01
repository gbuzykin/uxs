#include "uxs/db/value.h"

#include "uxs/algorithm.h"
#include "uxs/stringcvt.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>

using namespace uxs;
using namespace uxs::db;

// --------------------------

namespace uxs {
namespace db {
bool operator==(const value& lhs, const value& rhs) {
    if (lhs.type_ != rhs.type_) { return false; }
    switch (lhs.type_) {
        case value::dtype::kNull: return true;
        case value::dtype::kBoolean: return lhs.value_.b == rhs.value_.b;
        case value::dtype::kInteger: return lhs.value_.i == rhs.value_.i;
        case value::dtype::kUInteger: return lhs.value_.u == rhs.value_.u;
        case value::dtype::kInteger64: return lhs.value_.i64 == rhs.value_.i64;
        case value::dtype::kUInteger64: return lhs.value_.u64 == rhs.value_.u64;
        case value::dtype::kDouble: return lhs.value_.dbl == rhs.value_.dbl;
        case value::dtype::kString: return lhs.str_view() == rhs.str_view();
        case value::dtype::kArray: {
            auto range1 = lhs.as_array(), range2 = rhs.as_array();
            return range1.size() == range2.size() && uxs::equal(range1, range2.begin());
        } break;
        case value::dtype::kRecord: {
            if (lhs.value_.rec->size != rhs.value_.rec->size) { return false; }
            return uxs::equal(lhs.as_map(), rhs.as_map().begin());
        } break;
        default: break;
    }
    return false;
}
}  // namespace db
}  // namespace uxs

// --------------------------

template<typename Ty>
/*static*/ value::flexarray<Ty>* value::flexarray<Ty>::alloc(size_t cap) {
    size_t alloc_sz = get_alloc_sz(cap);
    flexarray* arr = std::allocator<flexarray>().allocate(alloc_sz);
    arr->capacity = (alloc_sz * sizeof(flexarray) - offsetof(flexarray, data[0])) / sizeof(Ty);
    assert(arr->capacity >= cap && get_alloc_sz(arr->capacity) == alloc_sz);
    return arr;
}

template<typename Ty>
/*static*/ value::flexarray<Ty>* value::flexarray<Ty>::grow(flexarray* arr, size_t extra) {
    size_t delta_sz = std::max(extra, arr->size >> 1);
    using alloc_traits = std::allocator_traits<std::allocator<flexarray>>;
    size_t max_size = (alloc_traits::max_size(std::allocator<flexarray>()) * sizeof(flexarray) -
                       offsetof(flexarray, data[0])) /
                      sizeof(Ty);
    if (delta_sz > max_size - arr->size) {
        if (extra > max_size - arr->size) { throw std::length_error("too much to reserve"); }
        delta_sz = std::max(extra, (max_size - arr->size) >> 1);
    }
    flexarray* new_arr = alloc(arr->size + delta_sz);
    if (arr->size != 0) { std::memcpy(new_arr->data, arr->data, arr->size * sizeof(Ty)); }
    new_arr->size = arr->size;
    dealloc(arr);
    return new_arr;
}

template<typename Ty>
/*static*/ void value::flexarray<Ty>::dealloc(flexarray* arr) {
    std::allocator<flexarray>().deallocate(arr, get_alloc_sz(arr->capacity));
}

// --------------------------

void value::record::init() {
    dllist_make_cycle(&head);
    list_node_traits::set_head(&head, &head);
    for (auto& item : as_span(hashtbl, bucket_count)) { item = nullptr; }
    size = 0;
}

void value::record::destroy(list_links_type* node) {
    while (node != &head) {
        list_links_type* next = node->next;
        delete_node(node);
        node = next;
    }
}

value::list_links_type* value::record::find(std::string_view s, size_t hash_code) const {
    list_links_type* bucket_next = hashtbl[hash_code % bucket_count];
    while (bucket_next != nullptr) {
        if (static_cast<list_node_type*>(bucket_next)->hash_code == hash_code &&
            list_node_traits::get_value(bucket_next).name() == s) {
            return bucket_next;
        }
        bucket_next = static_cast<list_node_type*>(bucket_next)->bucket_next;
    }
    return nullptr;
}

value::record* value::record::copy() const {
    record* rec = alloc(size);
    rec->init();
    try {
        list_links_type* node = head.next;
        while (node != &head) {
            const map_value& v = list_node_traits::get_value(node);
            rec->insert(static_cast<list_node_type*>(node)->hash_code, rec->new_node(v.name(), v.val()));
            node = node->next;
        }
    } catch (...) {
        rec->destroy(rec->head.next);
        dealloc(rec);
        throw;
    }
    return rec;
}

/*static*/ value::record* value::record::assign(record* rec, const record* src) {
    rec->clear();
    list_links_type* node = src->head.next;
    while (node != &src->head) {
        const map_value& v = list_node_traits::get_value(node);
        rec->insert(static_cast<list_node_type*>(node)->hash_code, rec->new_node(v.name(), v.val()));
        node = node->next;
    }
    return rec;
}

/*static*/ size_t value::record::calc_hash_code(std::string_view s) {
    // Implementation of Murmur hash for 64-bit size_t is taken from GNU libstdc++
    const size_t seed = 0xc70f6907;
#if UINTPTR_MAX == 0xffffffffffffffff
    const size_t mul = 0xc6a4a7935bd1e995ull;
    const char* end0 = s.data() + (s.size() & ~7);
    size_t hash = seed ^ (s.size() * mul);
    auto shift_mix = [](size_t v) { return v ^ (v >> 47); };
    for (const char* p = s.data(); p != end0; p += 8) {
        size_t data;
        std::memcpy(&data, p, 8);  // unaligned load
        hash ^= shift_mix(data * mul) * mul;
        hash *= mul;
    }
    if ((s.size() & 7) != 0) {
        size_t data = 0;
        const char* end = s.data() + s.size();
        do { data = (data << 8) + static_cast<unsigned char>(*--end); } while (end != end0);
        hash ^= data;
        hash *= mul;
    }
    hash = shift_mix(shift_mix(hash) * mul);
#else
    size_t hash = seed;
    for (char ch : s) { hash = (hash * 131) + static_cast<unsigned char>(ch); }
#endif
    return hash;
}

void value::record::insert(size_t hash_code, list_links_type* node) {
    static_cast<list_node_type*>(node)->hash_code = hash_code;
    add_to_hash(node, hash_code);
    dllist_insert_before(&head, node);
    ++size;
}

value::list_links_type* value::record::erase(std::string_view s) {
    size_t hash_code = calc_hash_code(s);
    list_links_type** p_bucket_next = &hashtbl[hash_code % bucket_count];
    while (*p_bucket_next != nullptr) {
        if (static_cast<list_node_type*>(*p_bucket_next)->hash_code == hash_code &&
            list_node_traits::get_value(*p_bucket_next).name() == s) {
            list_links_type* erased_node = *p_bucket_next;
            *p_bucket_next = static_cast<list_node_type*>(*p_bucket_next)->bucket_next;
            dllist_remove(erased_node);
            --size;
            return erased_node;
        }
        p_bucket_next = &static_cast<list_node_type*>(*p_bucket_next)->bucket_next;
    }
    return nullptr;
}

size_t value::record::next_bucket_count(size_t sz) {
    size_t delta_sz = std::max<size_t>(kMinBucketCountInc, sz >> 1);
    using alloc_traits = std::allocator_traits<std::allocator<record>>;
    size_t max_size = (alloc_traits::max_size(std::allocator<record>()) * sizeof(record) -
                       offsetof(record, hashtbl[0])) /
                      sizeof(list_links_type*);
    if (delta_sz > max_size - sz) {
        if (sz == max_size) { return sz; }
        delta_sz = std::max<size_t>(1, (max_size - sz) >> 1);
    }
    return sz + delta_sz;
}

/*static*/ value::record* value::record::alloc(size_t bckt_cnt) {
    size_t alloc_sz = get_alloc_sz(bckt_cnt);
    record* rec = std::allocator<record>().allocate(alloc_sz);
    rec->bucket_count = (alloc_sz * sizeof(record) - offsetof(record, hashtbl[0])) / sizeof(list_links_type*);
    assert(rec->bucket_count >= bckt_cnt && get_alloc_sz(rec->bucket_count) == alloc_sz);
    return rec;
}

/*static*/ value::record* value::record::rehash(record* rec, size_t bckt_cnt) {
    if (bckt_cnt <= rec->bucket_count) { return rec; }  // do not rehash
    record* new_rec = alloc(bckt_cnt);
    new_rec->head = rec->head;
    new_rec->size = rec->size;
    dealloc(rec);
    list_node_traits::set_head(&new_rec->head, &new_rec->head);
    for (auto& item : as_span(new_rec->hashtbl, new_rec->bucket_count)) { item = nullptr; }
    list_links_type* node = new_rec->head.next;
    node->prev = &new_rec->head;
    new_rec->head.prev->next = &new_rec->head;
    while (node != &new_rec->head) {
        list_node_traits::set_head(node, &new_rec->head);
        new_rec->add_to_hash(node, static_cast<list_node_type*>(node)->hash_code);
        node = node->next;
    }
    return new_rec;
}

/*static*/ void value::record::dealloc(record* rec) {
    std::allocator<record>().deallocate(rec, get_alloc_sz(rec->bucket_count));
}

// --------------------------

inline bool is_integral(double d) {
    double integral_part;
    return std::modf(d, &integral_part) == 0.;
}

bool value::as_bool(bool& res) const {
    switch (type_) {
        case dtype::kNull: return false;
        case dtype::kBoolean: res = value_.b; return true;
        case dtype::kInteger: res = !!value_.i; return true;
        case dtype::kUInteger: res = !!value_.u; return true;
        case dtype::kInteger64: res = !!value_.i64; return true;
        case dtype::kUInteger64: res = !!value_.u64; return true;
        case dtype::kDouble: res = value_.dbl != 0.; return true;
        case dtype::kString: return stoval(str_view(), res) != 0;
        case dtype::kArray: return false;
        case dtype::kRecord: return false;
        default: UNREACHABLE_CODE;
    }
}

bool value::as_int(int32_t& res) const {
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
        case dtype::kString: return stoval(str_view(), res) != 0;
        case dtype::kArray: return false;
        case dtype::kRecord: return false;
        default: UNREACHABLE_CODE;
    }
    return false;
}

bool value::as_uint(uint32_t& res) const {
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
        case dtype::kString: return stoval(str_view(), res) != 0;
        case dtype::kArray: return false;
        case dtype::kRecord: return false;
        default: UNREACHABLE_CODE;
    }
    return false;
}

bool value::as_int64(int64_t& res) const {
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
            // Note that double(2^63 - 1) will be rounded up to 2^63, so the left limit is excluded
            if (value_.dbl >= static_cast<double>(std::numeric_limits<int64_t>::min()) &&
                value_.dbl < static_cast<double>(std::numeric_limits<int64_t>::max())) {
                res = static_cast<int64_t>(value_.dbl);
                return true;
            }
        } break;
        case dtype::kString: return stoval(str_view(), res) != 0;
        case dtype::kArray: return false;
        case dtype::kRecord: return false;
        default: UNREACHABLE_CODE;
    }
    return false;
}

bool value::as_uint64(uint64_t& res) const {
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
            // Note that double(2^64 - 1) will be rounded up to 2^64, so the left limit is excluded
            if (value_.dbl >= 0 && value_.dbl < static_cast<double>(std::numeric_limits<uint64_t>::max())) {
                res = static_cast<uint64_t>(value_.dbl);
                return true;
            }
        } break;
        case dtype::kString: return stoval(str_view(), res) != 0;
        case dtype::kArray: return false;
        case dtype::kRecord: return false;
        default: UNREACHABLE_CODE;
    }
    return false;
}

bool value::as_float(float& res) const {
    switch (type_) {
        case dtype::kNull: return false;
        case dtype::kBoolean: res = value_.b ? 1.f : 0.f; return true;
        case dtype::kInteger: res = static_cast<float>(value_.i); return true;
        case dtype::kUInteger: res = static_cast<float>(value_.u); return true;
        case dtype::kInteger64: res = static_cast<float>(value_.i64); return true;
        case dtype::kUInteger64: res = static_cast<float>(value_.u64); return true;
        case dtype::kDouble: res = static_cast<float>(value_.dbl); return true;
        case dtype::kString: return stoval(str_view(), res) != 0;
        case dtype::kArray: return false;
        case dtype::kRecord: return false;
        default: UNREACHABLE_CODE;
    }
}

bool value::as_double(double& res) const {
    switch (type_) {
        case dtype::kNull: return false;
        case dtype::kBoolean: res = value_.b ? 1. : 0.; return true;
        case dtype::kInteger: res = static_cast<double>(value_.i); return true;
        case dtype::kUInteger: res = static_cast<double>(value_.u); return true;
        case dtype::kInteger64: res = static_cast<double>(value_.i64); return true;
        case dtype::kUInteger64: res = static_cast<double>(value_.u64); return true;
        case dtype::kDouble: res = value_.dbl; return true;
        case dtype::kString: return stoval(str_view(), res) != 0;
        case dtype::kArray: return false;
        case dtype::kRecord: return false;
        default: UNREACHABLE_CODE;
    }
}

bool value::as_string(std::string& res) const {
    switch (type_) {
        case dtype::kNull: res = "null"; return true;
        case dtype::kBoolean: res = value_.b ? "true" : "false"; return true;
        case dtype::kInteger: res = to_string(value_.i); return true;
        case dtype::kUInteger: res = to_string(value_.u); return true;
        case dtype::kInteger64: res = to_string(value_.i64); return true;
        case dtype::kUInteger64: res = to_string(value_.u64); return true;
        case dtype::kDouble: res = to_string(value_.dbl, fmt_flags::kAlternate); return true;
        case dtype::kString: res = std::string(str_view()); return true;
        case dtype::kArray: return false;
        case dtype::kRecord: return false;
        default: UNREACHABLE_CODE;
    }
}

bool value::as_string_view(std::string_view& res) const {
    if (type_ != dtype::kString) { return false; }
    res = str_view();
    return true;
}

// --------------------------

bool value::is_int() const {
    switch (type_) {
        case dtype::kInteger: return true;
        case dtype::kUInteger: return value_.u <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max());
        case dtype::kInteger64:
            return value_.i64 >= std::numeric_limits<int32_t>::min() &&
                   value_.i64 <= std::numeric_limits<int32_t>::max();
        case dtype::kUInteger64: return value_.u64 <= static_cast<uint64_t>(std::numeric_limits<int32_t>::max());
        case dtype::kDouble:
            return value_.dbl >= std::numeric_limits<int32_t>::min() &&
                   value_.dbl <= std::numeric_limits<int32_t>::max() && ::is_integral(value_.dbl);
        default: break;
    }
    return false;
}

bool value::is_uint() const {
    switch (type_) {
        case dtype::kInteger: return value_.i >= 0;
        case dtype::kUInteger: return true;
        case dtype::kInteger64:
            return value_.i64 >= 0 && value_.i64 <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max());
        case dtype::kUInteger64: return value_.u64 <= std::numeric_limits<uint32_t>::max();
        case dtype::kDouble:
            return value_.dbl >= 0 && value_.dbl <= std::numeric_limits<uint32_t>::max() && ::is_integral(value_.dbl);
        default: break;
    }
    return false;
}

bool value::is_int64() const {
    switch (type_) {
        case dtype::kInteger:
        case dtype::kUInteger:
        case dtype::kInteger64: return true;
        case dtype::kUInteger64: return value_.u64 <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
        case dtype::kDouble:
            // Note that double(2^63 - 1) will be rounded up to 2^63, so the left limit is excluded
            return value_.dbl >= static_cast<double>(std::numeric_limits<int64_t>::min()) &&
                   value_.dbl < static_cast<double>(std::numeric_limits<int64_t>::max()) && ::is_integral(value_.dbl);
        default: break;
    }
    return false;
}

bool value::is_uint64() const {
    switch (type_) {
        case dtype::kInteger: return value_.i >= 0;
        case dtype::kUInteger: return true;
        case dtype::kInteger64: return value_.i64 >= 0;
        case dtype::kUInteger64: return true;
        case dtype::kDouble:
            // Note that double(2^64 - 1) will be rounded up to 2^64, so the left limit is excluded
            return value_.dbl >= 0 && value_.dbl < static_cast<double>(std::numeric_limits<uint64_t>::max()) &&
                   ::is_integral(value_.dbl);
        default: break;
    }
    return false;
}

bool value::is_integral() const {
    switch (type_) {
        case dtype::kInteger:
        case dtype::kUInteger:
        case dtype::kInteger64:
        case dtype::kUInteger64: return true;
        case dtype::kDouble:
            // Note that double(2^64 - 1) will be rounded up to 2^64, so the left limit is excluded
            return value_.dbl >= static_cast<double>(std::numeric_limits<int64_t>::min()) &&
                   value_.dbl < static_cast<double>(std::numeric_limits<uint64_t>::max()) && ::is_integral(value_.dbl);
        default: break;
    }
    return false;
}

bool value::convert(dtype type) {
    if (type == type_) { return true; }
    switch (type) {
        case dtype::kNull: *this = value(); break;
        case dtype::kBoolean: *this = as_bool(); break;
        case dtype::kInteger: *this = as_int(); break;
        case dtype::kUInteger: *this = as_uint(); break;
        case dtype::kInteger64: *this = as_int64(); break;
        case dtype::kUInteger64: *this = as_uint64(); break;
        case dtype::kDouble: *this = as_double(); break;
        case dtype::kString: *this = as_string(); break;
        case dtype::kArray: {
            if (type_ != dtype::kNull) {
                flexarray<value>* arr = flexarray<value>::alloc(kMinCapacity);
                arr->data[0] = std::move(*this);
                arr->size = 1;
                value_.arr = arr;
            } else {
                value_.arr = nullptr;
            }
            type_ = dtype::kArray;
        } break;
        case dtype::kRecord: {
            if (type_ != dtype::kNull) { return false; }
            value_.rec = record::alloc(1);
            value_.rec->init();
            type_ = dtype::kRecord;
        } break;
        default: UNREACHABLE_CODE;
    }
    return true;
}

size_t value::size() const {
    switch (type_) {
        case dtype::kNull: return 0;
        case dtype::kArray: return value_.arr ? value_.arr->size : 0;
        case dtype::kRecord: return value_.rec->size;
        default: break;
    }
    return 1;
}

bool value::empty() const {
    switch (type_) {
        case dtype::kNull: return true;
        case dtype::kArray: return !value_.arr || value_.arr->size == 0;
        case dtype::kRecord: return value_.rec->size == 0;
        default: break;
    }
    return false;
}

bool value::contains(std::string_view name) const {
    if (type_ != dtype::kRecord) { return false; }
    return value_.rec->find(name, record::calc_hash_code(name)) != nullptr;
}

std::vector<std::string_view> value::members() const {
    if (type_ != dtype::kRecord) { return {}; }
    std::vector<std::string_view> names(value_.rec->size);
    auto p = names.begin();
    for (const auto& item : as_map()) { *p++ = item.name(); }
    return names;
}

span<const value> value::as_array() const {
    if (type_ == dtype::kArray) { return array_view(); }
    return type_ != dtype::kNull ? as_span(this, 1) : span<value>();
}

span<value> value::as_array() {
    if (type_ == dtype::kArray) { return array_view(); }
    return type_ != dtype::kNull ? as_span(this, 1) : span<value>();
}

iterator_range<value::const_record_iterator> value::as_map() const {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    return uxs::make_range(const_record_iterator(value_.rec->head.next), const_record_iterator(&value_.rec->head));
}

iterator_range<value::record_iterator> value::as_map() {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    return uxs::make_range(record_iterator(value_.rec->head.next), record_iterator(&value_.rec->head));
}

const value& value::operator[](size_t i) const {
    auto range = as_array();
    if (i >= range.size()) { throw exception("index out of range"); }
    return range[i];
}

value& value::operator[](size_t i) {
    auto range = as_array();
    if (i >= range.size()) { throw exception("index out of range"); }
    return range[i];
}

const value& value::operator[](std::string_view name) const {
    static const value null;
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    list_links_type* node = value_.rec->find(name, record::calc_hash_code(name));
    return node != nullptr ? list_node_traits::get_value(node).val() : null;
}

value& value::operator[](std::string_view name) {
    if (type_ != dtype::kRecord) {
        if (type_ != dtype::kNull) { throw exception("not a record"); }
        value_.rec = record::alloc(1);
        value_.rec->init();
        type_ = dtype::kRecord;
    }
    size_t hash_code = record::calc_hash_code(name);
    if (value_.rec->size >= value_.rec->bucket_count) {
        value_.rec = record::rehash(value_.rec, record::next_bucket_count(value_.rec->size));
    }
    list_links_type* node = value_.rec->find(name, hash_code);
    if (!node) {
        node = value_.rec->new_node(name);
        value_.rec->insert(hash_code, node);
    }
    return list_node_traits::get_value(node).val();
}

const value* value::find(std::string_view name) const {
    if (type_ != dtype::kRecord) { return nullptr; }
    list_links_type* node = value_.rec->find(name, record::calc_hash_code(name));
    return node != nullptr ? &list_node_traits::get_value(node).val() : nullptr;
}

void value::clear() {
    if (type_ == dtype::kRecord) {
        value_.rec->clear();
    } else if (type_ == dtype::kArray && value_.arr) {
        uxs::for_each(value_.arr->view(), [](value& v) { v.destroy(); });
        value_.arr->size = 0;
    }
}

void value::resize(size_t sz) {
    if (type_ != dtype::kArray || !value_.arr) {
        if (type_ != dtype::kArray && type_ != dtype::kNull) { throw exception("not an array"); }
        if (sz == 0) {
            value_.arr = nullptr;
            type_ = dtype::kArray;
            return;
        }
        value_.arr = flexarray<value>::alloc(sz);
        value_.arr->size = 0;
        type_ = dtype::kArray;
    } else if (sz > value_.arr->capacity) {
        value_.arr = flexarray<value>::grow(value_.arr, sz - value_.arr->size);
    } else if (sz < value_.arr->size) {
        std::for_each(&value_.arr->data[sz], &value_.arr->data[value_.arr->size], [](value& v) { v.destroy(); });
        value_.arr->size = sz;
        return;
    }
    std::for_each(&value_.arr->data[value_.arr->size], &value_.arr->data[sz], [](value& v) { new (&v) value(); });
    value_.arr->size = sz;
}

value& value::append(std::string_view s) {
    if (type_ != dtype::kString) { throw exception("not a string"); }
    if (!value_.str) {
        value_.str = flexarray<char>::alloc(s.size());
        value_.str->size = 0;
    } else if (s.size() > value_.str->capacity - value_.str->size) {
        value_.str = flexarray<char>::grow(value_.str, s.size());
    }
    std::copy_n(s.data(), s.size(), &value_.str->data[value_.str->size]);
    value_.str->size += s.size();
    return *this;
}

void value::push_back(char ch) {
    if (type_ != dtype::kString) { throw exception("not a string"); }
    if (!value_.str) {
        value_.str = flexarray<char>::alloc(1);
        value_.str->size = 0;
    } else if (value_.str->size == value_.str->capacity) {
        value_.str = flexarray<char>::grow(value_.str, 1);
    }
    value_.str->data[value_.str->size++] = ch;
}

void value::remove(size_t pos) {
    if (type_ != dtype::kArray) { throw exception("not an array"); }
    if (!value_.arr || pos >= value_.arr->size) { throw exception("index out of range"); }
    value* v = &value_.arr->data[pos];
    value* v_end = &value_.arr->data[--value_.arr->size];
    for (; v != v_end; ++v) { *v = std::move(*(v + 1)); }
    v->~value();
}

void value::remove(size_t pos, value& removed) {
    if (type_ != dtype::kArray) { throw exception("not an array"); }
    if (!value_.arr || pos >= value_.arr->size) { throw exception("index out of range"); }
    value* v = &value_.arr->data[pos];
    value* v_end = &value_.arr->data[--value_.arr->size];
    removed = std::move(*v);
    for (; v != v_end; ++v) { *v = std::move(*(v + 1)); }
    v->~value();
}

bool value::remove(std::string_view name) {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    list_links_type* node = value_.rec->erase(name);
    if (!node) { return false; }
    record::delete_node(node);
    return true;
}

bool value::remove(std::string_view name, value& removed) {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    list_links_type* node = value_.rec->erase(name);
    if (!node) { return false; }
    removed = std::move(list_node_traits::get_value(node).val());
    record::delete_node(node);
    return true;
}

void value::print_scalar(iobuf& out) const {
    switch (type_) {
        case value::dtype::kNull: out.write("null"); break;
        case value::dtype::kBoolean: out.write(value_.b ? "true" : "false"); break;
        case value::dtype::kInteger: {
            inline_dynbuffer buf;
            basic_to_string(buf.base(), value_.i, fmt_state());
            out.write(as_span(buf.data(), buf.size()));
        } break;
        case value::dtype::kUInteger: {
            inline_dynbuffer buf;
            basic_to_string(buf.base(), value_.u, fmt_state());
            out.write(as_span(buf.data(), buf.size()));
        } break;
        case value::dtype::kInteger64: {
            inline_dynbuffer buf;
            basic_to_string(buf.base(), value_.i64, fmt_state());
            out.write(as_span(buf.data(), buf.size()));
        } break;
        case value::dtype::kUInteger64: {
            inline_dynbuffer buf;
            basic_to_string(buf.base(), value_.u64, fmt_state());
            out.write(as_span(buf.data(), buf.size()));
        } break;
        case value::dtype::kDouble: {
            inline_dynbuffer buf;
            basic_to_string(buf.base(), value_.dbl, fmt_state(fmt_flags::kAlternate));
            out.write(as_span(buf.data(), buf.size()));
        } break;
        case value::dtype::kString: print_quoted_text(out, str_view()); break;
        case value::dtype::kArray: break;
        case value::dtype::kRecord: break;
        default: UNREACHABLE_CODE;
    }
}

// --------------------------

/*static*/ value::flexarray<char>* value::copy_string(span<const char> s) {
    if (s.size() == 0) { return nullptr; }
    flexarray<char>* str = flexarray<char>::alloc(s.size());
    str->size = s.size();
    std::memcpy(str->data, s.data(), s.size());
    return str;
}

/*static*/ value::flexarray<char>* value::assign_string(flexarray<char>* str, span<const char> s) {
    if (!str || s.size() > str->capacity) {
        if (s.size() == 0) { return nullptr; }
        flexarray<char>* new_str = flexarray<char>::alloc(s.size());
        if (str) { flexarray<char>::dealloc(str); }
        str = new_str;
    }
    str->size = s.size();
    std::memcpy(str->data, s.data(), s.size());
    return str;
}

/*static*/ value::flexarray<value>* value::copy_array(span<const value> v) {
    if (v.size() == 0) { return nullptr; }
    flexarray<value>* arr = flexarray<value>::alloc(v.size());
    try {
        uxs::uninitialized_copy(v, static_cast<value*>(arr->data));
    } catch (...) {
        flexarray<value>::dealloc(arr);
        throw;
    }
    arr->size = v.size();
    return arr;
}

/*static*/ value::flexarray<value>* value::assign_array(flexarray<value>* arr, span<const value> v) {
    if (arr && v.size() <= arr->capacity) {
        span<value> v_tgt = arr->view();
        if (v.size() > v_tgt.size()) {
            value* p = uxs::copy(v.subspan(0, v_tgt.size()), v_tgt.data());
            uxs::uninitialized_copy(v.subspan(v_tgt.size(), v.size() - v_tgt.size()), p);
        } else {
            uxs::copy(v, v_tgt.data());
            uxs::for_each(v_tgt.subspan(v.size(), v_tgt.size() - v.size()), [](value& v) { v.destroy(); });
        }
    } else if (v.size() != 0) {
        flexarray<value>* new_arr = flexarray<value>::alloc(v.size());
        try {
            uxs::uninitialized_copy(v, static_cast<value*>(new_arr->data));
        } catch (...) {
            flexarray<value>::dealloc(new_arr);
            throw;
        }
        if (arr) { flexarray<value>::dealloc(arr); }
        arr = new_arr;
    }
    arr->size = v.size();
    return arr;
}

// --------------------------

void value::init_from(const value& other) {
    switch (other.type_) {
        case dtype::kString: value_.str = copy_string(other.str_view()); break;
        case dtype::kArray: value_.arr = copy_array(other.array_view()); break;
        case dtype::kRecord: value_.rec = other.value_.rec->copy(); break;
        default: value_ = other.value_; break;
    }
}

void value::copy_from(const value& other) {
    if (type_ != other.type_) {
        if (type_ != dtype::kNull) { destroy(); }
        init_from(other);
        type_ = other.type_;
    } else {
        switch (other.type_) {
            case dtype::kString: value_.str = assign_string(value_.str, other.str_view()); break;
            case dtype::kArray: value_.arr = assign_array(value_.arr, other.array_view()); break;
            case dtype::kRecord: value_.rec = record::assign(value_.rec, other.value_.rec); break;
            default: value_ = other.value_; break;
        }
    }
}

void value::destroy() {
    switch (type_) {
        case dtype::kString: {
            if (value_.str) { flexarray<char>::dealloc(value_.str); }
        } break;
        case dtype::kArray: {
            if (value_.arr) {
                uxs::for_each(value_.arr->view(), [](value& v) { v.~value(); });
                flexarray<value>::dealloc(value_.arr);
            }
        } break;
        case dtype::kRecord: {
            value_.rec->destroy(value_.rec->head.next);
            record::dealloc(value_.rec);
        } break;
        default: break;
    }
    type_ = dtype::kNull;
}

void value::reserve_back() {
    if (type_ != dtype::kArray || !value_.arr) {
        if (type_ != dtype::kArray && type_ != dtype::kNull) { throw exception("not an array"); }
        value_.arr = flexarray<value>::alloc(kMinCapacity);
        value_.arr->size = 0;
        type_ = dtype::kArray;
    } else {
        value_.arr = flexarray<value>::grow(value_.arr, 1);
    }
}

void value::rotate_back(size_t pos) {
    assert(pos != value_.arr->size - 1);
    value* v = &value_.arr->data[value_.arr->size];
    value t(std::move(*(v - 1)));
    while (--v != &value_.arr->data[pos]) { *v = std::move(*(v - 1)); }
    *v = std::move(t);
}

template struct value::flexarray<char>;
template struct value::flexarray<value>;
