#pragma once

#include "uxs/dllist.h"
#include "uxs/io/iobuf.h"
#include "uxs/span.h"
#include "uxs/string_view.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace uxs {
namespace db {

class exception : public std::runtime_error {
 public:
    explicit exception(const char* message) : std::runtime_error(message) {}
    explicit exception(const std::string& message) : std::runtime_error(message) {}
};

struct empty_array_t {};
struct empty_record_t {};

#if !defined(_MSC_VER) || _MSC_VER >= 1920
constexpr empty_array_t empty_array{};
constexpr empty_record_t empty_record{};
#else   // !defined(_MSC_VER) || _MSC_VER >= 1920
const empty_array_t empty_array{};
const empty_record_t empty_record{};
#endif  // !defined(_MSC_VER) || _MSC_VER >= 1920

class UXS_EXPORT value {
 private:
    template<typename Ty>
    struct dynarray {
        size_t size;
        size_t capacity;
        Ty data[1];

        dynarray(const dynarray&) = delete;
        dynarray& operator=(const dynarray&) = delete;

        span<const Ty> view() const { return as_span(data, size); }
        span<Ty> view() { return as_span(data, size); }

        static size_t get_alloc_sz(size_t cap) {
            return (offsetof(dynarray, data[cap]) + sizeof(dynarray) - 1) / sizeof(dynarray);
        }

        static dynarray* alloc(size_t cap);
        static dynarray* grow(dynarray* arr, size_t extra);
        static void dealloc(dynarray* arr);
    };

#if _ITERATOR_DEBUG_LEVEL != 0
    struct list_links_type {
        list_links_type* next;
        list_links_type* prev;
        list_links_type* head;
    };
#else   // _ITERATOR_DEBUG_LEVEL != 0
    using list_links_type = dllist_node_t;
#endif  // _ITERATOR_DEBUG_LEVEL != 0

    struct list_node_type;
    struct list_node_traits;
    class map_value;

    struct record {
        using value_type = map_value;
        using size_type = size_t;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;

        list_links_type head;
        size_t size;
        size_t bucket_count;
        list_links_type* hashtbl[1];

        record(const record&) = delete;
        record& operator=(const record&) = delete;

        void init();
        void destroy(list_links_type* node);

        list_links_type* find(std::string_view name, size_t hash_code) const;

        record* copy() const;
        static record* assign(record* rec, const record* src);

        void clear() {
            destroy(head.next);
            init();
        }

        template<typename... Args>
        list_links_type* new_node(std::string_view name, Args&&... args);
        static void delete_node(list_links_type* node);
        static size_t calc_hash_code(std::string_view name);
        void insert(size_t hash_code, list_links_type* node);
        void add_to_hash(list_links_type* node, size_t hash_code);
        list_links_type* erase(std::string_view name);

        static size_t get_alloc_sz(size_t bckt_cnt) {
            return (offsetof(record, hashtbl[bckt_cnt]) + sizeof(record) - 1) / sizeof(record);
        }
        static size_t next_bucket_count(size_t sz);
        static record* alloc(size_t bckt_cnt);
        static record* rehash(record* rec, size_t bckt_cnt);
        static void dealloc(record* rec);
    };

 public:
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

    using record_iterator = list_iterator<record, list_node_traits, false>;
    using const_record_iterator = list_iterator<record, list_node_traits, true>;

    value() : type_(dtype::kNull) { value_.i64 = 0; }
    value(empty_array_t) : type_(dtype::kArray) { value_.arr = nullptr; }
    value(empty_record_t) : type_(dtype::kRecord) {
        value_.rec = record::alloc(1);
        value_.rec->init();
    }
    value(bool b) : type_(dtype::kBoolean) { value_.b = b; }
    value(int32_t i) : type_(dtype::kInteger) { value_.i = i; }
    value(uint32_t u) : type_(dtype::kUInteger) { value_.u = u; }
    value(int64_t i) : type_(dtype::kInteger64) { value_.i64 = i; }
    value(uint64_t u) : type_(dtype::kUInteger64) { value_.u64 = u; }
    value(double d) : type_(dtype::kDouble) { value_.dbl = d; }
    value(std::string_view s) : type_(dtype::kString) { value_.str = copy_string(s); }
    value(const char* cstr) : type_(dtype::kString) { value_.str = copy_string(std::string_view(cstr)); }

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
        return *this;
    }

#define UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(ty, id, field) \
    value& operator=(ty v) { \
        if (type_ != dtype::kNull) { destroy(); } \
        type_ = dtype::id, value_.field = v; \
        return *this; \
    }
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(bool, kBoolean, b)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(int32_t, kInteger, i)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(uint32_t, kUInteger, u)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(int64_t, kInteger64, i64)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(uint64_t, kUInteger64, u64)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(double, kDouble, dbl)
#undef UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT

    value& operator=(std::string_view s) {
        if (type_ != dtype::kString) {
            if (type_ != dtype::kNull) { destroy(); }
            value_.str = copy_string(s);
        } else {
            value_.str = assign_string(value_.str, s);
        }
        type_ = dtype::kString;
        return *this;
    }

    value& operator=(const char* cstr) { return (*this = std::string_view(cstr)); }

    friend bool operator==(const value& lhs, const value& rhs);
    friend bool operator!=(const value& lhs, const value& rhs) { return !(lhs == rhs); }

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
    bool as_string_view(std::string_view& res) const;

    bool as_bool() const;
    int32_t as_int() const;
    uint32_t as_uint() const;
    int64_t as_int64() const;
    uint64_t as_uint64() const;
    float as_float() const;
    double as_double() const;
    std::string as_string() const;
    std::string_view as_string_view() const;

    bool convert(dtype type);
    void print_scalar(iobuf& out) const;

    size_t size() const;
    bool empty() const;
    bool contains(std::string_view name) const;
    std::vector<std::string_view> members() const;
    explicit operator bool() const { return !is_null(); }

    span<const value> as_array() const;
    span<value> as_array();

    iterator_range<const_record_iterator> as_map() const;
    iterator_range<record_iterator> as_map();

    const value& operator[](size_t i) const;
    value& operator[](size_t i);

    const value& operator[](std::string_view name) const;
    value& operator[](std::string_view name);

    const value* find(std::string_view name) const;
    value* find(std::string_view name) { return const_cast<value*>(std::as_const(*this).find(name)); }

    void clear();
    void resize(size_t sz);
    void nullify() {
        if (type_ == dtype::kNull) { return; }
        destroy();
        type_ = dtype::kNull;
    }

    void rehash() {
        if (type_ == dtype::kRecord && value_.rec->size > value_.rec->bucket_count) {
            value_.rec = record::rehash(value_.rec, value_.rec->size);
        }
    }

    value& append(std::string_view s);
    void push_back(char ch);

    template<typename... Args>
    value& emplace_back(Args&&... args);
    void push_back(const value& v) { emplace_back(v); }
    void push_back(value&& v) { emplace_back(std::move(v)); }

    template<typename... Args>
    value& emplace(size_t pos, Args&&... args);
    void insert(size_t pos, const value& v) { emplace(pos, v); }
    void insert(size_t pos, value&& v) { emplace(pos, std::move(v)); }

    template<typename... Args>
    value& emplace(std::string_view name, Args&&... args);
    void insert(std::string_view name, const value& v) { emplace(name, v); }
    void insert(std::string_view name, value&& v) { emplace(name, std::move(v)); }

    void remove(size_t pos);
    void remove(size_t pos, value& removed);
    bool remove(std::string_view name);
    bool remove(std::string_view name, value& removed);

 private:
    enum : unsigned { kMinCapacity = 8, kMinBucketCountInc = 12 };

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
    span<const value> array_view() const { return value_.arr ? value_.arr->view() : span<value>(); }
    span<value> array_view() { return value_.arr ? value_.arr->view() : span<value>(); }

    static dynarray<char>* copy_string(span<const char> s);
    static dynarray<char>* assign_string(dynarray<char>* str, span<const char> s);

    static dynarray<value>* copy_array(span<const value> v);
    static dynarray<value>* assign_array(dynarray<value>* arr, span<const value> v);

    void init_from(const value& other);
    void copy_from(const value& other);
    void destroy();

    void reserve_back();
    void rotate_back(size_t pos);
};

class value::map_value {
 public:
    template<typename... Args>
    explicit map_value(std::string_view name, Args&&... args) : name_sz_(name.size()), v_(std::forward<Args>(args)...) {
        std::copy_n(name.data(), name.size(), static_cast<char*>(name_));
    }
    map_value(const map_value&) = delete;
    map_value& operator=(const map_value&) = delete;
    std::string_view name() const { return std::string_view(name_, name_sz_); }
    const value& val() const { return v_; }
    value& val() { return v_; }

    friend bool operator==(const map_value& lhs, const map_value& rhs) {
        return lhs.name() == rhs.name() && lhs.val() == rhs.val();
    }
    friend bool operator!=(const map_value& lhs, const map_value& rhs) { return !(lhs == rhs); }

 private:
    friend struct value::record;
    size_t name_sz_;
    value v_;
    char name_[1];

    static size_t get_alloc_sz(size_t name_sz) {
        return (offsetof(map_value, name_[name_sz]) + sizeof(map_value) - 1) / sizeof(map_value);
    }
};

struct value::list_node_type : list_links_type {
    list_links_type* bucket_next;
    size_t hash_code;
    map_value v;
};

struct value::list_node_traits {
    using iterator_node_t = list_links_type;
    using node_t = list_node_type;
    static list_links_type* get_next(list_links_type* node) { return node->next; }
    static list_links_type* get_prev(list_links_type* node) { return node->prev; }
#if _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(list_links_type* node, list_links_type* head) { node->head = head; }
    static list_links_type* get_head(list_links_type* node) { return node->head; }
    static list_links_type* get_front(list_links_type* head) { return head->next; }
#else   // _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(list_links_type* node, list_links_type* head) {}
#endif  // _ITERATOR_DEBUG_LEVEL != 0
    static map_value& get_value(list_links_type* node) { return static_cast<node_t*>(node)->v; }
};

template<typename... Args>
value& value::emplace_back(Args&&... args) {
    if (type_ != dtype::kArray || !value_.arr || value_.arr->size == value_.arr->capacity) { reserve_back(); }
    value& v = *new (&value_.arr->data[value_.arr->size]) value(std::forward<Args>(args)...);
    ++value_.arr->size;
    return v;
}

template<typename... Args>
value& value::emplace(size_t pos, Args&&... args) {
    if (type_ != dtype::kArray || !value_.arr || value_.arr->size == value_.arr->capacity) { reserve_back(); }
    if (pos > value_.arr->size) { throw exception("index out of range"); }
    value& v = *new (&value_.arr->data[value_.arr->size]) value(std::forward<Args>(args)...);
    ++value_.arr->size;
    if (pos != value_.arr->size - 1) { rotate_back(pos); }
    return v;
}

template<typename... Args>
value::list_links_type* value::record::new_node(std::string_view name, Args&&... args) {
    size_t alloc_sz = map_value::get_alloc_sz(name.size());
    list_links_type* node = std::allocator<list_node_type>().allocate(alloc_sz);
    list_node_traits::set_head(node, &head);
    try {
        new (&list_node_traits::get_value(node)) map_value(name, std::forward<Args>(args)...);
    } catch (...) {
        std::allocator<list_node_type>().deallocate(static_cast<list_node_type*>(node), alloc_sz);
        throw;
    }
    return node;
}

/*static*/ inline void value::record::delete_node(list_links_type* node) {
    map_value& v = list_node_traits::get_value(node);
    v.val().~value();
    std::allocator<list_node_type>().deallocate(static_cast<list_node_type*>(node), map_value::get_alloc_sz(v.name_sz_));
}

inline void value::record::add_to_hash(list_links_type* node, size_t hash_code) {
    list_links_type** slot = &hashtbl[hash_code % bucket_count];
    static_cast<list_node_type*>(node)->bucket_next = *slot;
    *slot = node;
}

template<typename... Args>
value& value::emplace(std::string_view name, Args&&... args) {
    if (type_ != dtype::kRecord) {
        if (type_ != dtype::kNull) { throw exception("not a record"); }
        value_.rec = record::alloc(1);
        value_.rec->init();
        type_ = dtype::kRecord;
    }
    list_links_type* node = value_.rec->new_node(name, std::forward<Args>(args)...);
    value_.rec->insert(record::calc_hash_code(name), node);
    return list_node_traits::get_value(node).val();
}

#define UXS_DB_VALUE_IMPLEMENT_SCALAR_IS_GET(ty, is_func) \
    template<> \
    inline bool value::is<ty>() const { \
        return this->is_func(); \
    }
UXS_DB_VALUE_IMPLEMENT_SCALAR_IS_GET(bool, is_bool)
UXS_DB_VALUE_IMPLEMENT_SCALAR_IS_GET(int32_t, is_int)
UXS_DB_VALUE_IMPLEMENT_SCALAR_IS_GET(uint32_t, is_uint)
UXS_DB_VALUE_IMPLEMENT_SCALAR_IS_GET(int64_t, is_int64)
UXS_DB_VALUE_IMPLEMENT_SCALAR_IS_GET(uint64_t, is_uint64)
UXS_DB_VALUE_IMPLEMENT_SCALAR_IS_GET(float, is_float)
UXS_DB_VALUE_IMPLEMENT_SCALAR_IS_GET(double, is_double)
UXS_DB_VALUE_IMPLEMENT_SCALAR_IS_GET(std::string, is_string)
#undef UXS_DB_VALUE_IMPLEMENT_SCALAR_IS_GET

#define UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(ty, as_func) \
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

UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(bool, as_bool)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(int32_t, as_int)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(uint32_t, as_uint)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(int64_t, as_int64)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(uint64_t, as_uint64)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(float, as_float)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(double, as_double)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(std::string, as_string)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(std::string_view, as_string_view)
#undef UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET

}  // namespace db
}  // namespace uxs
