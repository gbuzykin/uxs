#pragma once

#include "util/map.h"
#include "util/span.h"
#include "util/string_view.h"

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace util {
namespace db {

class exception : public std::runtime_error {
 public:
    explicit exception(const char* message) : std::runtime_error(message) {}
    explicit exception(const std::string& message) : std::runtime_error(message) {}
};

class UTIL_EXPORT value {
 public:
    using record = util::map<std::string, value, util::less<>>;

    enum class dtype {
        kNull = 0,
        kBoolean,
        kInteger,
        kUInteger,
        kInteger64,
        kUInteger64,
        kDouble,
        kString,
        kArray,
        kRecord,
    };

    value() : type_(dtype::kNull) { value_.i64 = 0; }
    explicit value(bool b) : type_(dtype::kBoolean) { value_.b = b; }
    explicit value(int32_t i) : type_(dtype::kInteger) { value_.i = i; }
    explicit value(uint32_t u) : type_(dtype::kUInteger) { value_.u = u; }
    explicit value(int64_t i) : type_(dtype::kInteger64) { value_.i64 = i; }
    explicit value(uint64_t u) : type_(dtype::kUInteger64) { value_.u64 = u; }
    explicit value(double d) : type_(dtype::kDouble) { value_.dbl = d; }
    explicit value(std::string_view s) : type_(dtype::kString) { init_string(s); }

    ~value() {
        if (type_ != dtype::kNull) { destroy(); }
    }
    value(value&& other) : type_(other.type_), value_(other.value_) { other.type_ = dtype::kNull; }
    value& operator=(value&& other) {
        if (&other == this) { return *this; }
        if (type_ != dtype::kNull) { destroy(); }
        type_ = other.type_, value_ = other.value_;
        other.type_ = dtype::kNull;
        return *this;
    }
    value(const value& other) : type_(other.type_) { init_from(other); }
    value& operator=(const value& other) {
        if (&other == this) { return *this; }
        copy_from(other);
        type_ = other.type_;
        return *this;
    }

#define PARSER_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(ty, id, field) \
    value& operator=(ty v) { \
        if (type_ != dtype::kNull) { destroy(); } \
        type_ = dtype::id, value_.field = v; \
        return *this; \
    }
    PARSER_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(bool, kBoolean, b)
    PARSER_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(int32_t, kInteger, i)
    PARSER_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(uint32_t, kUInteger, u)
    PARSER_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(int64_t, kInteger64, i64)
    PARSER_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(uint64_t, kUInteger64, u64)
    PARSER_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(double, kDouble, dbl)
#undef PARSER_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT

    value& operator=(std::string_view s) {
        assign_string(s);
        type_ = dtype::kString;
        return *this;
    }

    dtype type() const { return type_; }

    template<typename Ty>
    bool is() const = delete;

    template<typename Ty>
    Ty as() const = delete;

    template<typename Ty>
    Ty get(Ty def) const = delete;

    template<typename Ty>
    Ty get(std::string_view name, Ty def) const = delete;

    bool is_null() const { return type_ == dtype::kNull; }
    bool is_bool() const { return type_ == dtype::kBoolean; }
    bool is_int() const;
    bool is_uint() const;
    bool is_int64() const;
    bool is_uint64() const;
    bool is_integral() const;
    bool is_float() const { return is_numeric(); }
    bool is_double() const { return is_numeric(); }
    bool is_numeric() const { return type_ >= dtype::kInteger && type_ <= dtype::kDouble; }
    bool is_string() const { return type_ == dtype::kString; }
    bool is_array() const { return type_ == dtype::kArray; }
    bool is_record() const { return type_ == dtype::kRecord; }

    bool as_bool(bool& res) const;
    bool as_int(int32_t& res) const;
    bool as_uint(uint32_t& res) const;
    bool as_int64(int64_t& res) const;
    bool as_uint64(uint64_t& res) const;
    bool as_float(float& res) const;
    bool as_double(double& res) const;
    bool as_string(std::string& res) const;

    bool convert(dtype type);

    bool as_bool() const;
    int32_t as_int() const;
    uint32_t as_uint() const;
    int64_t as_int64() const;
    uint64_t as_uint64() const;
    float as_float() const;
    double as_double() const;
    std::string as_string() const;

    size_t size() const;
    bool empty() const;
    bool contains(std::string_view name) const;
    std::vector<std::string_view> members() const;
    explicit operator bool() const { return !is_null(); }

    span<const value> view() const;
    span<value> view();

    iterator_range<record::const_iterator> map() const;
    iterator_range<record::iterator> map();

    const value& operator[](size_t i) const;
    value& operator[](size_t i);

    const value& operator[](std::string_view name) const;
    value& operator[](std::string_view name);

    const value* find(std::string_view name) const;
    value* find(std::string_view name) { return const_cast<value*>(std::as_const(*this).find(name)); }

    void clear();
    void resize(size_t sz);

    template<typename... Args>
    value& emplace_back(Args&&... args) {
        return *::new (add_placeholder()) value(std::forward<Args>(args)...);
    }
    void push_back(const value& v) { emplace_back(v); }
    void push_back(value&& v) { emplace_back(std::move(v)); }

    template<typename... Args>
    value& emplace(size_t pos, Args&&... args) {
        return *::new (insert_placeholder(pos)) value(std::forward<Args>(args)...);
    }
    void insert(size_t pos, const value& v) { emplace(pos, v); }
    void insert(size_t pos, value&& v) { emplace(pos, std::move(v)); }

    void remove(size_t pos);
    void remove(size_t pos, value& removed);
    bool remove(std::string_view name);
    bool remove(std::string_view name, value& removed);

 private:
    enum : unsigned { kMinCapacity = 8 };

    template<typename Ty>
    struct dynarray {
        size_t size;
        size_t capacity;
        Ty data[1];
        span<const Ty> view() const { return as_span(data, size); }
        span<Ty> view() { return as_span(data, size); }
        static size_t get_alloc_sz(size_t cap) {
            return (offsetof(dynarray, data[cap]) + sizeof(dynarray) - 1) / sizeof(dynarray);
        }
        static dynarray* alloc(size_t cap);
        dynarray* grow(size_t extra);
        void dealloc();
    };

    dtype type_;

    union {
        bool b;
        int32_t i;
        uint32_t u;
        int64_t i64;
        uint64_t u64;
        double dbl;
        dynarray<char>* str;
        dynarray<value>* arr;
        record* rec;
    } value_;

    std::string_view str_view() const {
        return value_.str ? std::string_view(value_.str->data, value_.str->size) : std::string_view();
    }
    span<const value> arr_view() const { return value_.arr ? value_.arr->view() : span<value>(); }
    span<value> arr_view() { return value_.arr ? value_.arr->view() : span<value>(); }

    void init_string(span<const char> s);
    void assign_string(span<const char> s);

    void init_array(span<const value> v);
    void assign_array(span<const value> v);

    void init_from(const value& other);
    void copy_from(const value& other);
    void destroy();

    value* add_placeholder();
    value* insert_placeholder(size_t pos);
};

#define PARSER_VALUE_IMPLEMENT_SCALAR_IS_AS_GET(ty, is_func, as_func) \
    template<> \
    inline bool value::is<ty>() const { \
        return this->is_func(); \
    } \
    inline ty value::as_func() const { \
        ty res; \
        if (!this->as_func(res)) { throw exception("not convertible to " #ty); } \
        return res; \
    } \
    template<> \
    inline ty value::as<ty>() const { \
        return this->as_func(); \
    } \
    template<> \
    inline ty value::get(ty def) const { \
        ty res(std::move(def)); \
        this->as_func(res); \
        return res; \
    } \
    template<> \
    inline ty value::get(std::string_view name, ty def) const { \
        ty res(std::move(def)); \
        if (const value* v = this->find(name)) { v->as_func(res); } \
        return res; \
    }

PARSER_VALUE_IMPLEMENT_SCALAR_IS_AS_GET(bool, is_bool, as_bool)
PARSER_VALUE_IMPLEMENT_SCALAR_IS_AS_GET(int32_t, is_int, as_int)
PARSER_VALUE_IMPLEMENT_SCALAR_IS_AS_GET(uint32_t, is_uint, as_uint)
PARSER_VALUE_IMPLEMENT_SCALAR_IS_AS_GET(int64_t, is_int64, as_int64)
PARSER_VALUE_IMPLEMENT_SCALAR_IS_AS_GET(uint64_t, is_uint64, as_uint64)
PARSER_VALUE_IMPLEMENT_SCALAR_IS_AS_GET(float, is_float, as_float)
PARSER_VALUE_IMPLEMENT_SCALAR_IS_AS_GET(double, is_double, as_double)
PARSER_VALUE_IMPLEMENT_SCALAR_IS_AS_GET(std::string, is_string, as_string)
#undef PARSER_VALUE_IMPLEMENT_SCALAR_IS_AS_GET

}  // namespace db
}  // namespace util
