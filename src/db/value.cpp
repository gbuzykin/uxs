#include "util/db/value.h"

#include "util/algorithm.h"
#include "util/stringcvt.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>

using namespace util;
using namespace util::db;

// --------------------------

template<typename Ty>
/*static*/ value::dynarray<Ty>* value::dynarray<Ty>::alloc(size_t cap) {
    dynarray* new_arr = std::allocator<dynarray>().allocate(get_alloc_sz(cap));
    new_arr->capacity = cap;
    return new_arr;
}

template<typename Ty>
value::dynarray<Ty>* value::dynarray<Ty>::grow(size_t extra) {
    dynarray* new_arr = alloc(size + std::max(extra, size >> 1));
    if (size != 0) { std::memcpy(new_arr->data, data, size * sizeof(Ty)); }
    new_arr->size = size;
    dealloc();
    return new_arr;
}

template<typename Ty>
void value::dynarray<Ty>::dealloc() {
    std::allocator<dynarray>().deallocate(this, get_alloc_sz(capacity));
}

// --------------------------

inline bool is_integral(double d) {
    double integral_part;
    return std::modf(d, &integral_part) == 0.;
}

bool value::as_bool(bool& res) const {
    switch (type_) {
        case dtype::kBoolean: res = value_.b; return true;
        case dtype::kInteger: res = !!value_.i; return true;
        case dtype::kUInteger: res = !!value_.u; return true;
        case dtype::kInteger64: res = !!value_.i64; return true;
        case dtype::kUInteger64: res = !!value_.u64; return true;
        case dtype::kDouble: res = value_.dbl != 0.; return true;
        case dtype::kString: {
            if (stoval(str_view(), res) != 0) { return true; }
        } break;
        default: break;
    }
    return false;
}

bool value::as_int(int32_t& res) const {
    switch (type_) {
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
        case dtype::kString: {
            if (stoval(str_view(), res) != 0) { return true; }
        } break;
        default: break;
    }
    return false;
}

bool value::as_uint(uint32_t& res) const {
    switch (type_) {
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
        case dtype::kString: {
            if (stoval(str_view(), res) != 0) { return true; }
        } break;
        default: break;
    }
    return false;
}

bool value::as_int64(int64_t& res) const {
    switch (type_) {
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
        case dtype::kString: {
            if (stoval(str_view(), res) != 0) { return true; }
        } break;
        default: break;
    }
    return false;
}

bool value::as_uint64(uint64_t& res) const {
    switch (type_) {
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
        case dtype::kString: {
            if (stoval(str_view(), res) != 0) { return true; }
        } break;
        default: break;
    }
    return false;
}

bool value::as_float(float& res) const {
    switch (type_) {
        case dtype::kBoolean: res = value_.b ? 1.f : 0.f; return true;
        case dtype::kInteger: res = static_cast<float>(value_.i); return true;
        case dtype::kUInteger: res = static_cast<float>(value_.u); return true;
        case dtype::kInteger64: res = static_cast<float>(value_.i64); return true;
        case dtype::kUInteger64: res = static_cast<float>(value_.u64); return true;
        case dtype::kDouble: res = static_cast<float>(value_.dbl); return true;
        case dtype::kString: {
            if (stoval(str_view(), res) != 0) { return true; }
        } break;
        default: break;
    }
    return false;
}

bool value::as_double(double& res) const {
    switch (type_) {
        case dtype::kBoolean: res = value_.b ? 1. : 0.; return true;
        case dtype::kInteger: res = static_cast<double>(value_.i); return true;
        case dtype::kUInteger: res = static_cast<double>(value_.u); return true;
        case dtype::kInteger64: res = static_cast<double>(value_.i64); return true;
        case dtype::kUInteger64: res = static_cast<double>(value_.u64); return true;
        case dtype::kDouble: res = value_.dbl; return true;
        case dtype::kString: {
            if (stoval(str_view(), res) != 0) { return true; }
        } break;
        default: break;
    }
    return false;
}

bool value::as_string(std::string& res) const {
    switch (type_) {
        case dtype::kNull: res = "null"; return true;
        case dtype::kBoolean: res = to_string(value_.b); return true;
        case dtype::kInteger: res = to_string(value_.i); return true;
        case dtype::kUInteger: res = to_string(value_.u); return true;
        case dtype::kInteger64: res = to_string(value_.i64); return true;
        case dtype::kUInteger64: res = to_string(value_.u64); return true;
        case dtype::kDouble: res = to_string(value_.dbl); return true;
        case dtype::kString: res = std::string(str_view()); return true;
        default: break;
    }
    return false;
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
                dynarray<value>* arr = dynarray<value>::alloc(kMinCapacity);
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
            value_.rec = new record();
            type_ = dtype::kRecord;
        } break;
        default: break;
    }
    return true;
}

size_t value::size() const {
    switch (type_) {
        case dtype::kNull: return 0;
        case dtype::kArray: return value_.arr ? value_.arr->size : 0;
        case dtype::kRecord: return value_.rec->size();
        default: break;
    }
    return 1;
}

bool value::empty() const {
    switch (type_) {
        case dtype::kNull: return true;
        case dtype::kArray: return !value_.arr || value_.arr->size == 0;
        case dtype::kRecord: return value_.rec->empty();
        default: break;
    }
    return false;
}

bool value::contains(std::string_view name) const {
    if (type_ != dtype::kRecord) { return false; }
    return value_.rec->contains(name);
}

std::vector<std::string_view> value::members() const {
    if (type_ != dtype::kRecord) { return {}; }
    std::vector<std::string_view> names(value_.rec->size());
    auto p = names.begin();
    for (const auto& item : *value_.rec) { *p++ = item.first; }
    return names;
}

span<const value> value::view() const {
    if (type_ == dtype::kArray) { return arr_view(); }
    return type_ != dtype::kNull ? as_span(this, 1) : span<value>();
}

span<value> value::view() {
    if (type_ == dtype::kArray) { return arr_view(); }
    return type_ != dtype::kNull ? as_span(this, 1) : span<value>();
}

iterator_range<value::record::const_iterator> value::map() const {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    return util::make_range(value_.rec->cbegin(), value_.rec->cend());
}

iterator_range<value::record::iterator> value::map() {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    return util::make_range(value_.rec->begin(), value_.rec->end());
}

const value& value::operator[](size_t i) const {
    auto range = view();
    if (i >= range.size()) { throw exception("index out of range"); }
    return range[i];
}

value& value::operator[](size_t i) {
    auto range = view();
    if (i >= range.size()) { throw exception("index out of range"); }
    return range[i];
}

const value& value::operator[](std::string_view name) const {
    static const value null;
    if (type_ != dtype::kRecord) { return null; }
    auto it = value_.rec->find(name);
    if (it == value_.rec->end()) { return null; }
    return it->second;
}

value& value::operator[](std::string_view name) {
    if (type_ != dtype::kRecord) {
        if (type_ != dtype::kNull) { throw exception("not a record"); }
        value_.rec = new record();
        type_ = dtype::kRecord;
    }
    auto it = value_.rec->lower_bound(name);
    if (it == value_.rec->end() || name < it->first) {
        it = value_.rec->emplace_hint(it, std::piecewise_construct, std::make_tuple(name), std::tuple<>());
    }
    return it->second;
}

const value* value::find(std::string_view name) const {
    if (type_ != dtype::kRecord) { return nullptr; }
    auto it = value_.rec->find(name);
    if (it == value_.rec->end()) { return nullptr; }
    return &it->second;
}

void value::clear() {
    switch (type_) {
        case dtype::kArray: {
            if (value_.arr) {
                util::for_each(value_.arr->view(), [](value& v) { v.destroy(); });
                value_.arr->size = 0;
            }
        } break;
        case dtype::kRecord: value_.rec->size(); break;
        default: break;
    }
}

void value::resize(size_t sz) {
    if (type_ != dtype::kArray || !value_.arr) {
        if (value_.arr && type_ != dtype::kNull) { throw exception("not an array"); }
        value_.arr = dynarray<value>::alloc(sz);
        value_.arr->size = 0;
        type_ = dtype::kArray;
    } else if (sz > value_.arr->capacity) {
        value_.arr->grow(sz - value_.arr->size);
    } else if (sz < value_.arr->size) {
        std::for_each(value_.arr->data + sz, value_.arr->data + value_.arr->size, [](value& v) { v.destroy(); });
        value_.arr->size = sz;
        return;
    }
    std::for_each(value_.arr->data + value_.arr->size, value_.arr->data + sz, [](value& v) { ::new (&v) value(); });
    value_.arr->size = sz;
}

void value::remove(size_t pos) {
    if (type_ != dtype::kArray) { throw exception("not an array"); }
    if (!value_.arr || pos >= value_.arr->size) { throw exception("index out of range"); }
    value* v = value_.arr->data + pos;
    for (; v != value_.arr->data + pos - 1; ++v) { *v = std::move(*(v + 1)); }
    v->destroy();
}

void value::remove(size_t pos, value& removed) {
    if (type_ != dtype::kArray) { throw exception("not an array"); }
    if (!value_.arr || pos >= value_.arr->size) { throw exception("index out of range"); }
    value* v = value_.arr->data + pos;
    removed = std::move(*v);
    for (; v != value_.arr->data + pos - 1; ++v) { *v = std::move(*(v + 1)); }
    v->destroy();
}

bool value::remove(std::string_view name) {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    auto it = value_.rec->find(name);
    if (it == value_.rec->end()) { return false; }
    value_.rec->erase(it);
    return true;
}

bool value::remove(std::string_view name, value& removed) {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    auto it = value_.rec->find(name);
    if (it == value_.rec->end()) { return false; }
    removed = std::move(it->second);
    value_.rec->erase(it);
    return true;
}

// --------------------------

void value::init_string(span<const char> s) {
    if (s.size() != 0) {
        value_.str = dynarray<char>::alloc(s.size());
        value_.str->size = s.size();
        std::memcpy(value_.str->data, s.data(), s.size());
    } else {
        value_.str = nullptr;
    }
}

void value::assign_string(span<const char> s) {
    if (s.size() != 0) {
        if (type_ != dtype::kString || !value_.str || s.size() > value_.str->capacity) {
            dynarray<char>* new_str = dynarray<char>::alloc(s.size());
            if (type_ != dtype::kNull) { destroy(); }
            value_.str = new_str;
        }
        value_.str->size = s.size();
        std::memcpy(value_.str->data, s.data(), s.size());
    } else if (type_ != dtype::kNull) {
        destroy();
    }
}

void value::init_array(span<const value> v) {
    if (v.size() != 0) {
        value_.arr = dynarray<value>::alloc(v.size());
        try {
            util::uninitialized_copy(v, value_.arr->data);
        } catch (...) {
            value_.arr->dealloc();
            throw;
        }
        value_.arr->size = v.size();
    } else {
        value_.arr = nullptr;
    }
}

void value::assign_array(span<const value> v) {
    if (v.size() != 0) {
        if (type_ != dtype::kArray || !value_.arr || v.size() > value_.arr->capacity) {
            dynarray<value>* new_arr = dynarray<value>::alloc(v.size());
            try {
                util::uninitialized_copy(v, new_arr->data);
            } catch (...) {
                new_arr->dealloc();
                throw;
            }
            if (type_ != dtype::kNull) { destroy(); }
            value_.arr = new_arr;
        } else {
            span<value> v_tgt = value_.arr->view();
            if (v.size() > v_tgt.size()) {
                value* p = util::copy(v.subspan(0, v_tgt.size()), v_tgt.data());
                util::uninitialized_copy(v.subspan(v_tgt.size(), v.size() - v_tgt.size()), p);
            } else {
                util::copy(v, v_tgt.data());
                util::for_each(v_tgt.subspan(v.size(), v_tgt.size() - v.size()), [](value& v) { v.destroy(); });
            }
        }
        value_.arr->size = v.size();
    } else if (type_ != dtype::kNull) {
        destroy();
    }
}

// --------------------------

void value::init_from(const value& other) {
    switch (type_) {
        case dtype::kString: init_string(other.str_view()); break;
        case dtype::kArray: init_array(other.arr_view()); break;
        case dtype::kRecord: value_.rec = new record(*other.value_.rec); break;
        default: value_ = other.value_; break;
    }
}

void value::copy_from(const value& other) {
    switch (other.type_) {
        case dtype::kString: assign_string(other.str_view()); break;
        case dtype::kArray: assign_array(other.arr_view()); break;
        case dtype::kRecord: *value_.rec = *other.value_.rec; break;
        default: value_ = other.value_; break;
    }
}

void value::destroy() {
    switch (type_) {
        case dtype::kString: {
            if (value_.str) { value_.str->dealloc(); }
        } break;
        case dtype::kArray: {
            if (value_.arr) {
                util::for_each(value_.arr->view(), [](value& v) { v.destroy(); });
                value_.arr->dealloc();
            }
        } break;
        case dtype::kRecord: delete value_.rec; break;
        default: break;
    }
}

// --------------------------

value* value::add_placeholder() {
    if (type_ != dtype::kArray || !value_.arr) {
        if (value_.arr && type_ != dtype::kNull) { throw exception("not an array"); }
        value_.arr = dynarray<value>::alloc(kMinCapacity);
        value_.arr->size = 0;
        type_ = dtype::kArray;
    } else if (value_.arr->size == value_.arr->capacity) {
        value_.arr = value_.arr->grow(1);
    }
    return &value_.arr->data[value_.arr->size++];
}

value* value::insert_placeholder(size_t pos) {
    value* v = add_placeholder();
    if (pos >= value_.arr->size) {
        --value_.arr->size;
        throw exception("index out of range");
    }
    if (pos != value_.arr->size - 1) {
        ::new (v) value(std::move(*(v - 1)));
        while (--v != value_.arr->data + pos) { *v = std::move(*(v - 1)); }
        v->destroy();
    }
    return v;
}

template struct value::dynarray<char>;
template struct value::dynarray<value>;
