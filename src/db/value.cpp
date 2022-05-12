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
/*static*/ value::dynarray<Ty>* value::dynarray<Ty>::grow(dynarray* arr, size_t extra) {
    size_t delta_sz = std::max(extra, arr->size >> 1);
    using alloc_traits = std::allocator_traits<std::allocator<dynarray>>;
    size_t max_size = (alloc_traits::max_size(std::allocator<dynarray>()) * sizeof(dynarray) -
                       offsetof(dynarray, data[0])) /
                      sizeof(Ty);
    if (delta_sz > max_size - arr->size) {
        if (extra > max_size - arr->size) { throw std::length_error("too much to reserve"); }
        delta_sz = std::max(extra, (max_size - arr->size) >> 1);
    }
    dynarray* new_arr = alloc(arr->size + delta_sz);
    if (arr->size != 0) { std::memcpy(new_arr->data, arr->data, arr->size * sizeof(Ty)); }
    new_arr->size = arr->size;
    dealloc(arr);
    return new_arr;
}

template<typename Ty>
/*static*/ void value::dynarray<Ty>::dealloc(dynarray* arr) {
    std::allocator<dynarray>().deallocate(arr, get_alloc_sz(arr->capacity));
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
        case dtype::kBoolean: res = value_.b ? "true" : "false"; return true;
        case dtype::kInteger: res = to_string(value_.i); return true;
        case dtype::kUInteger: res = to_string(value_.u); return true;
        case dtype::kInteger64: res = to_string(value_.i64); return true;
        case dtype::kUInteger64: res = to_string(value_.u64); return true;
        case dtype::kDouble: res = to_string(value_.dbl, fmt_flags::kAlternate); return true;
        case dtype::kString: res = std::string(str_view()); return true;
        default: break;
    }
    return false;
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
    return util::make_range(value_.rec->cbegin(), value_.rec->cend());
}

iterator_range<value::record_iterator> value::as_map() {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    return util::make_range(value_.rec->begin(), value_.rec->end());
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
    return value_.rec->try_emplace(name).first->second;
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
        case dtype::kRecord: value_.rec->clear(); break;
        default: break;
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
        value_.arr = dynarray<value>::alloc(sz);
        value_.arr->size = 0;
        type_ = dtype::kArray;
    } else if (sz > value_.arr->capacity) {
        value_.arr = dynarray<value>::grow(value_.arr, sz - value_.arr->size);
    } else if (sz < value_.arr->size) {
        std::for_each(value_.arr->data + sz, value_.arr->data + value_.arr->size, [](value& v) { v.destroy(); });
        value_.arr->size = sz;
        return;
    }
    std::for_each(value_.arr->data + value_.arr->size, value_.arr->data + sz, [](value& v) { new (&v) value(); });
    value_.arr->size = sz;
}

void value::remove(size_t pos) {
    if (type_ != dtype::kArray) { throw exception("not an array"); }
    if (!value_.arr || pos >= value_.arr->size) { throw exception("index out of range"); }
    value* v = value_.arr->data + pos;
    value* v_end = value_.arr->data + --value_.arr->size;
    for (; v != v_end; ++v) { *v = std::move(*(v + 1)); }
    v->destroy();
}

void value::remove(size_t pos, value& removed) {
    if (type_ != dtype::kArray) { throw exception("not an array"); }
    if (!value_.arr || pos >= value_.arr->size) { throw exception("index out of range"); }
    value* v = value_.arr->data + pos;
    value* v_end = value_.arr->data + --value_.arr->size;
    removed = std::move(*v);
    for (; v != v_end; ++v) { *v = std::move(*(v + 1)); }
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

void value::print_scalar(iobuf& out) const {
    switch (type_) {
        case value::dtype::kNull: out.write("null"); break;
        case value::dtype::kBoolean: out.write(value_.b ? "true" : "false"); break;
        case value::dtype::kInteger: {
            dynbuffer buf;
            basic_to_string(buf, value_.i, fmt_state());
            out.write(as_span(buf.data(), buf.size()));
        } break;
        case value::dtype::kUInteger: {
            dynbuffer buf;
            basic_to_string(buf, value_.u, fmt_state());
            out.write(as_span(buf.data(), buf.size()));
        } break;
        case value::dtype::kInteger64: {
            dynbuffer buf;
            basic_to_string(buf, value_.i64, fmt_state());
            out.write(as_span(buf.data(), buf.size()));
        } break;
        case value::dtype::kUInteger64: {
            dynbuffer buf;
            basic_to_string(buf, value_.u64, fmt_state());
            out.write(as_span(buf.data(), buf.size()));
        } break;
        case value::dtype::kDouble: {
            dynbuffer buf;
            basic_to_string(buf, value_.dbl, fmt_state(fmt_flags::kAlternate));
            out.write(as_span(buf.data(), buf.size()));
        } break;
        case value::dtype::kString: print_quoted_text(out, str_view()); break;
        default: break;
    }
}

// --------------------------

/*static*/ value::dynarray<char>* value::copy_string(span<const char> s) {
    if (s.size() == 0) { return nullptr; }
    dynarray<char>* str = dynarray<char>::alloc(s.size());
    str->size = s.size();
    std::memcpy(str->data, s.data(), s.size());
    return str;
}

/*static*/ value::dynarray<char>* value::assign_string(dynarray<char>* str, span<const char> s) {
    if (!str || s.size() > str->capacity) {
        if (s.size() == 0) { return nullptr; }
        dynarray<char>* new_str = dynarray<char>::alloc(s.size());
        if (str) { dynarray<char>::dealloc(str); }
        str = new_str;
    }
    str->size = s.size();
    std::memcpy(str->data, s.data(), s.size());
    return str;
}

/*static*/ value::dynarray<value>* value::copy_array(span<const value> v) {
    if (v.size() == 0) { return nullptr; }
    dynarray<value>* arr = dynarray<value>::alloc(v.size());
    try {
        util::uninitialized_copy(v, arr->data);
    } catch (...) {
        dynarray<value>::dealloc(arr);
        throw;
    }
    arr->size = v.size();
    return arr;
}

/*static*/ value::dynarray<value>* value::assign_array(dynarray<value>* arr, span<const value> v) {
    if (arr && v.size() <= arr->capacity) {
        span<value> v_tgt = arr->view();
        if (v.size() > v_tgt.size()) {
            value* p = util::copy(v.subspan(0, v_tgt.size()), v_tgt.data());
            util::uninitialized_copy(v.subspan(v_tgt.size(), v.size() - v_tgt.size()), p);
        } else {
            util::copy(v, v_tgt.data());
            util::for_each(v_tgt.subspan(v.size(), v_tgt.size() - v.size()), [](value& v) { v.destroy(); });
        }
    } else if (v.size() != 0) {
        dynarray<value>* new_arr = dynarray<value>::alloc(v.size());
        try {
            util::uninitialized_copy(v, new_arr->data);
        } catch (...) {
            dynarray<value>::dealloc(new_arr);
            throw;
        }
        if (arr) { dynarray<value>::dealloc(arr); }
        arr = new_arr;
    }
    return arr;
}

// --------------------------

void value::init_from(const value& other) {
    switch (other.type_) {
        case dtype::kString: value_.str = copy_string(other.str_view()); break;
        case dtype::kArray: value_.arr = copy_array(other.array_view()); break;
        case dtype::kRecord: value_.rec = new record(*other.value_.rec); break;
        default: value_ = other.value_; break;
    }
}

void value::copy_from(const value& other) {
    if (type_ != other.type_) {
        if (type_ != dtype::kNull) { destroy(); }
        init_from(other);
    } else {
        switch (other.type_) {
            case dtype::kString: value_.str = assign_string(value_.str, other.str_view()); break;
            case dtype::kArray: value_.arr = assign_array(value_.arr, other.array_view()); break;
            case dtype::kRecord: *value_.rec = *other.value_.rec; break;
            default: value_ = other.value_; break;
        }
    }
    type_ = other.type_;
}

void value::destroy() {
    switch (type_) {
        case dtype::kString: {
            if (value_.str) { dynarray<char>::dealloc(value_.str); }
        } break;
        case dtype::kArray: {
            if (value_.arr) {
                util::for_each(value_.arr->view(), [](value& v) { v.destroy(); });
                dynarray<value>::dealloc(value_.arr);
            }
        } break;
        case dtype::kRecord: delete value_.rec; break;
        default: break;
    }
}

// --------------------------

void value::reserve_back() {
    if (type_ != dtype::kArray || !value_.arr) {
        if (type_ != dtype::kArray && type_ != dtype::kNull) { throw exception("not an array"); }
        value_.arr = dynarray<value>::alloc(kMinCapacity);
        value_.arr->size = 0;
        type_ = dtype::kArray;
    } else if (value_.arr->size == value_.arr->capacity) {
        value_.arr = dynarray<value>::grow(value_.arr, 1);
    }
}

void value::rotate_back(size_t pos) {
    assert(pos != value_.arr->size - 1);
    value* v = value_.arr->data + value_.arr->size;
    value t(std::move(*(v - 1)));
    while (--v != value_.arr->data + pos) { *v = std::move(*(v - 1)); }
    *v = std::move(t);
}

template struct value::dynarray<char>;
template struct value::dynarray<value>;
