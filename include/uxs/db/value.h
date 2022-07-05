#pragma once

#include "uxs/dllist.h"
#include "uxs/span.h"
#include "uxs/string_view.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace uxs {
namespace db {
namespace json {
class writer;
}

class value;

namespace detail {
template<typename Ty, typename = void>
struct is_record_value : std::false_type {};
template<typename FirstTy, typename SecondTy>
struct is_record_value<std::pair<FirstTy, SecondTy>,
                       std::enable_if_t<std::is_convertible<FirstTy, std::string_view>::value &&
                                        std::is_convertible<SecondTy, value>::value>> : std::true_type {};
}  // namespace detail

class exception : public std::runtime_error {
 public:
    explicit exception(const char* message) : std::runtime_error(message) {}
    explicit exception(const std::string& message) : std::runtime_error(message) {}
};

template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
value make_array(InputIt first, InputIt last);

template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>,
         typename = std::enable_if_t<detail::is_record_value<typename std::iterator_traits<InputIt>::value_type>::value>>
value make_record(InputIt first, InputIt last);

class UXS_EXPORT value {
 public:
    template<typename Ty>
    struct rec_value_proxy;

 private:
    template<typename Ty>
    struct flexarray_t {
        size_t size;
        size_t capacity;
        Ty data[1];

        flexarray_t(const flexarray_t&) = delete;
        flexarray_t& operator=(const flexarray_t&) = delete;

        span<const Ty> view() const { return as_span(data, size); }
        span<Ty> view() { return as_span(data, size); }

        static size_t max_size();
        static size_t get_alloc_sz(size_t cap);
        static flexarray_t* alloc(size_t cap);
        static flexarray_t* alloc_checked(size_t cap);
        static flexarray_t* grow(flexarray_t* arr, size_t extra);
        static void dealloc(flexarray_t* arr);
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
    struct rec_value;

    struct record_t {
        using value_type = std::pair<std::string_view, value>;
        using size_type = size_t;
        using difference_type = std::ptrdiff_t;
        using reference = rec_value_proxy<value&>;
        using const_reference = rec_value_proxy<const value&>;
        using pointer = reference;
        using const_pointer = const_reference;

        mutable list_links_type head;
        size_t size;
        size_t bucket_count;
        list_links_type* hashtbl[1];

        record_t(const record_t&) = delete;
        record_t& operator=(const record_t&) = delete;

        void init();
        void destroy(list_links_type* node);

        list_links_type* find(std::string_view name, size_t hash_code) const;
        size_t count(std::string_view name) const;

        static record_t* alloc() {
            record_t* rec = alloc(1);
            rec->init();
            return rec;
        }

        void clear() {
            destroy(head.next);
            init();
        }

        template<typename InputIt>
        static record_t* alloc(InputIt first, InputIt last);
        static record_t* alloc(const std::initializer_list<value>& init);
        static record_t* alloc(const std::initializer_list<std::pair<std::string_view, value>>& init);
        static record_t* alloc(const record_t& src);

        static record_t* assign(record_t* rec, const record_t& src);

        template<typename... Args>
        list_links_type* new_node(std::string_view name, Args&&... args);
        static void delete_node(list_links_type* node);
        static size_t calc_hash_code(std::string_view name);
        void add_to_hash(list_links_type* node, size_t hash_code);
        static record_t* insert(record_t* rec, size_t hash_code, list_links_type* node);
        list_links_type* erase(list_links_type* node);
        size_t erase(std::string_view name);

        static size_t max_size();
        static size_t get_alloc_sz(size_t bckt_cnt);
        static size_t next_bucket_count(size_t sz);
        static record_t* alloc(size_t bckt_cnt);
        static record_t* rehash(record_t* rec, size_t bckt_cnt);
        static void dealloc(record_t* rec);
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

    using record_iterator = list_iterator<record_t, list_node_traits, false>;
    using const_record_iterator = list_iterator<record_t, list_node_traits, true>;

    value() NOEXCEPT : type_(dtype::kNull) {}
    value(bool b) : type_(dtype::kBoolean) { value_.b = b; }
    value(int32_t i) : type_(dtype::kInteger) { value_.i = i; }
    value(uint32_t u) : type_(dtype::kUInteger) { value_.u = u; }
    value(int64_t i) : type_(dtype::kInteger64) { value_.i64 = i; }
    value(uint64_t u) : type_(dtype::kUInteger64) { value_.u64 = u; }
    value(double d) : type_(dtype::kDouble) { value_.dbl = d; }
    value(std::string_view s) : type_(dtype::kString) { value_.str = alloc_string(s); }
    value(const char* cstr) : type_(dtype::kString) { value_.str = alloc_string(std::string_view(cstr)); }
    value(std::nullptr_t) : type_(dtype::kNull) {}

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    value(InputIt first, InputIt last) {
        construct_impl(first, last, detail::is_record_value<typename std::iterator_traits<InputIt>::value_type>());
    }

    value(std::initializer_list<value> init);

    ~value() {
        if (type_ != dtype::kNull) { destroy(); }
    }

    value(const value& other) : type_(other.type_) { init_from(other); }
    value& operator=(const value& other) {
        if (&other == this) { return *this; }
        assign_impl(other);
        return *this;
    }

    value(value&& other) NOEXCEPT : type_(other.type_), value_(other.value_) { other.type_ = dtype::kNull; }
    value& operator=(value&& other) NOEXCEPT {
        if (&other == this) { return *this; }
        if (type_ != dtype::kNull) { destroy(); }
        value_ = other.value_, type_ = other.type_;
        other.type_ = dtype::kNull;
        return *this;
    }

    value& operator=(std::initializer_list<value> init) {
        assign(init);
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    void assign(InputIt first, InputIt last) {
        assign_impl(first, last, detail::is_record_value<typename std::iterator_traits<InputIt>::value_type>());
    }

    void assign(std::initializer_list<value> init);

#define UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(ty, id, field) \
    value& operator=(ty v) { \
        if (type_ != dtype::kNull) { destroy(); } \
        value_.field = v, type_ = dtype::id; \
        return *this; \
    }
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(bool, kBoolean, b)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(int32_t, kInteger, i)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(uint32_t, kUInteger, u)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(int64_t, kInteger64, i64)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(uint64_t, kUInteger64, u64)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(double, kDouble, dbl)
#undef UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT

    value& operator=(std::string_view s);
    value& operator=(const char* cstr) { return (*this = std::string_view(cstr)); }

    value& operator=(std::nullptr_t) {
        if (type_ == dtype::kNull) { return *this; }
        destroy();
        return *this;
    }

    void swap(value& other) NOEXCEPT {
        if (&other == this) { return; }
        std::swap(value_, other.value_);
        std::swap(type_, other.type_);
    }

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

    bool empty() const NOEXCEPT;
    size_t size() const NOEXCEPT;
    std::vector<std::string_view> members() const;
    explicit operator bool() const { return !is_null(); }

    span<const value> as_array() const NOEXCEPT;
    span<value> as_array() NOEXCEPT;

    iterator_range<const_record_iterator> as_record() const;
    iterator_range<record_iterator> as_record();

    const value& operator[](size_t i) const { return as_array()[i]; }
    value& operator[](size_t i) { return as_array()[i]; }

    const value& at(size_t i) const {
        auto range = as_array();
        if (i >= range.size()) { throw exception("index out of range"); }
        return range[i];
    }

    value& at(size_t i) {
        auto range = as_array();
        if (i >= range.size()) { throw exception("index out of range"); }
        return range[i];
    }

    const value& operator[](std::string_view name) const;
    value& operator[](std::string_view name);

    const_record_iterator find(std::string_view name) const;
    record_iterator find(std::string_view name);
    const_record_iterator nil() const NOEXCEPT;
    record_iterator nil() NOEXCEPT;
    bool contains(std::string_view name) const;
    size_t count(std::string_view name) const;

    void clear() NOEXCEPT;
    void reserve(size_t sz);
    void resize(size_t sz);

    template<typename... Args>
    value& emplace_back(Args&&... args);
    void push_back(const value& v) { emplace_back(v); }
    void push_back(value&& v) { emplace_back(std::move(v)); }
    void pop_back();

    template<typename... Args>
    value& emplace(size_t pos, Args&&... args);
    void insert(size_t pos, const value& v) { emplace(pos, v); }
    void insert(size_t pos, value&& v) { emplace(pos, std::move(v)); }

    template<typename... Args>
    value& emplace(std::string_view name, Args&&... args);
    void insert(std::string_view name, const value& v) { emplace(name, v); }
    void insert(std::string_view name, value&& v) { emplace(name, std::move(v)); }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    void insert(size_t pos, InputIt first, InputIt last);
    void insert(size_t pos, std::initializer_list<value> init);

    template<
        typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>,
        typename = std::enable_if_t<detail::is_record_value<typename std::iterator_traits<InputIt>::value_type>::value>>
    void insert(InputIt first, InputIt last);
    void insert(std::initializer_list<std::pair<std::string_view, value>> init);

    void erase(size_t pos);
    record_iterator erase(const_record_iterator it);
    size_t erase(std::string_view name);

 private:
    friend class json::writer;
    friend value make_array();
    template<typename InputIt, typename>
    friend value make_array(InputIt, InputIt);
    friend value make_array(std::initializer_list<value>);
    friend value make_record();
    template<typename InputIt, typename, typename>
    friend value make_record(InputIt, InputIt);
    friend value make_record(std::initializer_list<std::pair<std::string_view, value>>);

    enum : unsigned { kStartCapacity = 8, kMinBucketCountInc = 12 };

    dtype type_;

    union {
        bool b;
        int32_t i;
        uint32_t u;
        int64_t i64;
        uint64_t u64;
        double dbl;
        flexarray_t<char>* str;
        flexarray_t<value>* arr;
        record_t* rec;
    } value_;

    std::string_view str_view() const {
        return value_.str ? std::string_view(value_.str->data, value_.str->size) : std::string_view();
    }
    span<const value> array_view() const { return value_.arr ? value_.arr->view() : span<value>(); }
    span<value> array_view() { return value_.arr ? value_.arr->view() : span<value>(); }

    static flexarray_t<char>* alloc_string(std::string_view s);
    void assign_string(std::string_view s);

    template<typename RandIt>
    static flexarray_t<value>* alloc_array(size_t sz, RandIt src);
    template<typename RandIt>
    static flexarray_t<value>* alloc_array(RandIt first, RandIt last, std::true_type /* random access iterator */) {
        return alloc_array(static_cast<size_t>(last - first), first);
    }
    template<typename InputIt>
    static flexarray_t<value>* alloc_array(InputIt first, InputIt last, std::false_type);
    static flexarray_t<value>* alloc_array(span<const value> v) { return alloc_array(v.size(), v.data()); }

    template<typename RandIt>
    void assign_array(size_t sz, RandIt src);
    template<typename RandIt>
    void assign_array(RandIt first, RandIt last, std::true_type /* random access iterator */) {
        assign_array(static_cast<size_t>(last - first), first);
    }
    template<typename InputIt>
    void assign_array(InputIt first, InputIt last, std::false_type /* random access iterator */);
    void assign_array(span<const value> v) { assign_array(v.size(), v.data()); }

    template<typename RandIt>
    void append_array(size_t sz, RandIt src);
    template<typename RandIt>
    size_t append_array(RandIt first, RandIt last, std::true_type /* random access iterator */) {
        size_t count = static_cast<size_t>(last - first);
        append_array(count, first);
        return count;
    }
    template<typename InputIt>
    size_t append_array(InputIt first, InputIt last, std::false_type /* random access iterator */);

    void init_from(const value& other);
    void assign_impl(const value& other);
    void destroy();

    template<typename InputIt>
    void construct_impl(InputIt first, InputIt last, std::true_type /* range of pairs */) {
        type_ = dtype::kRecord;
        value_.rec = record_t::alloc(first, last);
    }

    template<typename InputIt>
    void construct_impl(InputIt first, InputIt last, std::false_type /* range of pairs */) {
        type_ = dtype::kArray;
        value_.arr = alloc_array(first, last, is_random_access_iterator<InputIt>());
    }

    template<typename InputIt>
    void assign_impl(InputIt first, InputIt last, std::true_type /* range of pairs */);

    template<typename InputIt>
    void assign_impl(InputIt first, InputIt last, std::false_type /* range of pairs */);

    void reserve_back();
    void rotate_back(size_t pos);
};

struct value::rec_value {
    value v;
    size_t name_sz;
    char name_chars[1];
    std::string_view name() const { return std::string_view(name_chars, name_sz); }
    const value& val() const { return v; }
    value& val() { return v; }
};

template<typename Ty>
struct value::rec_value_proxy : std::pair<std::string_view, Ty&> {
    rec_value_proxy(rec_value& v) : std::pair<std::string_view, Ty&>(v.name(), v.val()) {}
    static rec_value_proxy addressof(const rec_value_proxy& proxy) { return proxy; }
    const rec_value_proxy* operator->() const { return this; }
    template<typename Ty1, typename Ty2>
    friend bool operator==(const rec_value_proxy& lhs, const std::pair<Ty1, Ty2>& rhs) {
        return lhs.first == rhs.first && lhs.second == rhs.second;
    }
    template<typename Ty1, typename Ty2>
    friend bool operator==(const std::pair<Ty1, Ty2>& lhs, const rec_value_proxy& rhs) {
        return lhs.first == rhs.first && lhs.second == rhs.second;
    }
    template<typename Ty1, typename Ty2>
    friend bool operator!=(const rec_value_proxy& lhs, const std::pair<Ty1, Ty2>& rhs) {
        return !(lhs == rhs);
    }
    template<typename Ty1, typename Ty2>
    friend bool operator!=(const std::pair<Ty1, Ty2>& lhs, const rec_value_proxy& rhs) {
        return !(lhs == rhs);
    }
    friend bool operator==(const rec_value_proxy& lhs, const rec_value_proxy& rhs) {
        return lhs.first == rhs.first && lhs.second == rhs.second;
    }
    friend bool operator!=(const rec_value_proxy& lhs, const rec_value_proxy& rhs) { return !(lhs == rhs); }
};

struct value::list_node_type {
    list_links_type links;
    list_links_type* bucket_next;
    size_t hash_code;
    rec_value v;
    static list_node_type* from_links(list_links_type* links) {
        return reinterpret_cast<list_node_type*>(reinterpret_cast<uint8_t*>(links) - offsetof(list_node_type, links));
    }
    static size_t max_name_size();
    static size_t get_alloc_sz(size_t name_sz);
    static list_node_type* alloc_checked(std::string_view name);
    static void dealloc(list_node_type* node);
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
    static rec_value& get_value(list_links_type* node) { return node_t::from_links(node)->v; }
};

// --------------------------

inline value make_array() {
    value v;
    v.value_.arr = nullptr;
    v.type_ = value::dtype::kArray;
    return v;
}

template<typename InputIt, typename>
value make_array(InputIt first, InputIt last) {
    value v;
    v.value_.arr = value::alloc_array(first, last, is_random_access_iterator<InputIt>());
    v.type_ = value::dtype::kArray;
    return v;
}

inline value make_array(std::initializer_list<value> init) {
    value v;
    v.value_.arr = value::alloc_array(init.size(), init.begin());
    v.type_ = value::dtype::kArray;
    return v;
}

inline value make_record() {
    value v;
    v.value_.rec = value::record_t::alloc();
    v.type_ = value::dtype::kRecord;
    return v;
}

template<typename InputIt, typename, typename>
value make_record(InputIt first, InputIt last) {
    value v;
    v.value_.rec = value::record_t::alloc(first, last);
    v.type_ = value::dtype::kRecord;
    return v;
}

inline value make_record(std::initializer_list<std::pair<std::string_view, value>> init) {
    value v;
    v.value_.rec = value::record_t::alloc(init);
    v.type_ = value::dtype::kRecord;
    return v;
}

inline value& value::operator=(std::string_view s) {
    if (type_ != dtype::kString) {
        if (type_ != dtype::kNull) { destroy(); }
        value_.str = alloc_string(s);
        type_ = dtype::kString;
    } else {
        assign_string(s);
    }
    return *this;
}

// --------------------------

template<typename... Args>
value& value::emplace_back(Args&&... args) {
    if (type_ != dtype::kArray || !value_.arr || value_.arr->size == value_.arr->capacity) { reserve_back(); }
    value& v = *new (&value_.arr->data[value_.arr->size]) value(std::forward<Args>(args)...);
    ++value_.arr->size;
    return v;
}

inline void value::pop_back() {
    if (type_ != dtype::kArray) { throw exception("not an array"); }
    assert(value_.arr && value_.arr->size);
    value_.arr->data[--value_.arr->size].~value();
}

template<typename... Args>
value& value::emplace(size_t pos, Args&&... args) {
    if (type_ != dtype::kArray || !value_.arr || value_.arr->size == value_.arr->capacity) { reserve_back(); }
    new (&value_.arr->data[value_.arr->size]) value(std::forward<Args>(args)...);
    ++value_.arr->size;
    if (pos >= value_.arr->size - 1) { return value_.arr->data[value_.arr->size - 1]; }
    rotate_back(pos);
    return value_.arr->data[pos];
}

template<typename... Args>
value& value::emplace(std::string_view name, Args&&... args) {
    if (type_ != dtype::kRecord) {
        if (type_ != dtype::kNull) { throw exception("not a record"); }
        value_.rec = record_t::alloc();
        type_ = dtype::kRecord;
    }
    list_links_type* node = value_.rec->new_node(name, std::forward<Args>(args)...);
    value_.rec = record_t::insert(value_.rec, record_t::calc_hash_code(name), node);
    return list_node_traits::get_value(node).val();
}

template<typename InputIt, typename>
void value::insert(size_t pos, InputIt first, InputIt last) {
    if (type_ != dtype::kArray) {
        if (type_ != dtype::kNull) { throw exception("not an array"); }
        value_.arr = alloc_array(first, last, is_random_access_iterator<InputIt>());
        type_ = dtype::kArray;
    } else if (first != last) {
        size_t count = append_array(first, last, is_random_access_iterator<InputIt>());
        if (pos < value_.arr->size - count) {
            std::rotate(&value_.arr->data[pos], &value_.arr->data[value_.arr->size - count],
                        &value_.arr->data[value_.arr->size]);
        }
    }
}

template<typename InputIt, typename, typename>
void value::insert(InputIt first, InputIt last) {
    if (type_ != dtype::kRecord) {
        if (type_ != dtype::kNull) { throw exception("not a record"); }
        value_.rec = record_t::alloc(first, last);
        type_ = dtype::kRecord;
    } else {
        for (; first != last; ++first) {
            list_links_type* node = value_.rec->new_node(first->first, first->second);
            value_.rec = record_t::insert(value_.rec, record_t::calc_hash_code(first->first), node);
        }
    }
}

inline span<const value> value::as_array() const NOEXCEPT {
    if (type_ == dtype::kArray) { return array_view(); }
    return type_ != dtype::kNull ? as_span(this, 1) : span<value>();
}

inline span<value> value::as_array() NOEXCEPT {
    if (type_ == dtype::kArray) { return array_view(); }
    return type_ != dtype::kNull ? as_span(this, 1) : span<value>();
}

inline iterator_range<value::const_record_iterator> value::as_record() const {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    return uxs::make_range(const_record_iterator(value_.rec->head.next), const_record_iterator(&value_.rec->head));
}

inline iterator_range<value::record_iterator> value::as_record() {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    return uxs::make_range(record_iterator(value_.rec->head.next), record_iterator(&value_.rec->head));
}

inline value::const_record_iterator value::find(std::string_view name) const {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    return const_record_iterator(value_.rec->find(name, record_t::calc_hash_code(name)));
}

inline value::record_iterator value::find(std::string_view name) {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    return record_iterator(value_.rec->find(name, record_t::calc_hash_code(name)));
}

inline value::const_record_iterator value::nil() const NOEXCEPT {
    return const_record_iterator(type_ == dtype::kRecord ? &value_.rec->head : nullptr);
}

inline value::record_iterator value::nil() NOEXCEPT {
    return record_iterator(type_ == dtype::kRecord ? &value_.rec->head : nullptr);
}

inline bool value::contains(std::string_view name) const { return find(name) != nil(); }

inline size_t value::count(std::string_view name) const {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    return value_.rec->count(name);
}

// --------------------------

template<typename RandIt>
/*static*/ value::flexarray_t<value>* value::alloc_array(size_t sz, RandIt src) {
    if (!sz) { return nullptr; }
    flexarray_t<value>* arr = flexarray_t<value>::alloc_checked(sz);
    try {
        std::uninitialized_copy_n(src, sz, static_cast<value*>(arr->data));
        arr->size = sz;
        return arr;
    } catch (...) {
        flexarray_t<value>::dealloc(arr);
        throw;
    }
}

template<typename InputIt>
/*static*/ value::flexarray_t<value>* value::alloc_array(InputIt first, InputIt last,
                                                         std::false_type /* random access iterator */) {
    if (first == last) { return nullptr; }
    flexarray_t<value>* arr = flexarray_t<value>::alloc(kStartCapacity);
    arr->size = 0;
    try {
        do {
            if (arr->size == arr->capacity) { arr = flexarray_t<value>::grow(arr, 1); }
            new (&arr->data[arr->size]) value(*first);
            ++arr->size;
            ++first;
        } while (first != last);
        return arr;
    } catch (...) {
        std::for_each(static_cast<value*>(arr->data), static_cast<value*>(arr->data) + arr->size,
                      [](value& v) { v.~value(); });
        flexarray_t<value>::dealloc(arr);
        throw;
    }
}

template<typename RandIt>
void value::assign_array(size_t sz, RandIt src) {
    if (value_.arr && sz <= value_.arr->capacity) {
        if (sz <= value_.arr->size) {
            value* p = sz ? std::copy_n(src, sz, static_cast<value*>(value_.arr->data)) : value_.arr->data;
            std::for_each(p, p + static_cast<size_t>(value_.arr->size - sz), [](value& v) { v.~value(); });
        } else {
            value* p = std::copy_n(src, value_.arr->size, static_cast<value*>(value_.arr->data));
            std::uninitialized_copy_n(src + value_.arr->size, sz - value_.arr->size, p);
        }
        value_.arr->size = sz;
    } else {
        flexarray_t<value>* new_arr = alloc_array(sz, src);
        if (value_.arr) { flexarray_t<value>::dealloc(value_.arr); }
        value_.arr = new_arr;
    }
}

template<typename InputIt>
void value::assign_array(InputIt first, InputIt last, std::false_type /* random access iterator */) {
    if (value_.arr) {
        value *p0 = value_.arr->data, *p = p0;
        value* p_end = &value_.arr->data[value_.arr->size];
        for (; p != p_end && first != last; ++first) { *p++ = *first; }
        value_.arr->size = p - p0;
        if (p != p_end) {
            std::for_each(p, p_end, [](value& v) { v.~value(); });
        } else {
            for (; first != last; ++first) {
                if (value_.arr->size == value_.arr->capacity) { value_.arr = flexarray_t<value>::grow(value_.arr, 1); }
                new (&value_.arr->data[value_.arr->size]) value(*first);
                ++value_.arr->size;
            }
        }
    } else {
        value_.arr = alloc_array(first, last, std::false_type());
    }
}

template<typename RandIt>
void value::append_array(size_t sz, RandIt src) {
    if (value_.arr) {
        if (sz > value_.arr->capacity - value_.arr->size) { value_.arr = flexarray_t<value>::grow(value_.arr, sz); }
        std::uninitialized_copy_n(src, sz, &value_.arr->data[value_.arr->size]);
        value_.arr->size += sz;
    } else {
        value_.arr = alloc_array(sz, src);
    }
}

template<typename InputIt>
size_t value::append_array(InputIt first, InputIt last, std::false_type /* random access iterator */) {
    if (first == last) { return 0; }
    if (value_.arr) {
        size_t old_sz = value_.arr->size;
        for (; first != last; ++first) {
            if (value_.arr->size == value_.arr->capacity) { value_.arr = flexarray_t<value>::grow(value_.arr, 1); }
            new (&value_.arr->data[value_.arr->size]) value(*first);
            ++value_.arr->size;
        }
        return value_.arr->size - old_sz;
    }
    value_.arr = alloc_array(first, last, std::false_type());
    return value_.arr->size;
}

template<typename InputIt>
void value::assign_impl(InputIt first, InputIt last, std::true_type /* range of pairs */) {
    if (type_ != dtype::kRecord) {
        if (type_ != dtype::kNull) { destroy(); }
        value_.rec = record_t::alloc(first, last);
        type_ = dtype::kRecord;
    } else {
        value_.rec->clear();
        for (; first != last; ++first) {
            list_links_type* node = value_.rec->new_node(first->first, first->second);
            value_.rec = record_t::insert(value_.rec, record_t::calc_hash_code(first->first), node);
        }
    }
}

template<typename InputIt>
void value::assign_impl(InputIt first, InputIt last, std::false_type /* range of pairs */) {
    if (type_ != dtype::kArray) {
        if (type_ != dtype::kNull) { destroy(); }
        value_.arr = alloc_array(first, last, is_random_access_iterator<InputIt>());
        type_ = dtype::kArray;
    } else {
        assign_array(first, last, is_random_access_iterator<InputIt>());
    }
}

template<typename InputIt>
/*static*/ value::record_t* value::record_t::alloc(InputIt first, InputIt last) {
    record_t* rec = alloc();
    try {
        for (; first != last; ++first) {
            list_links_type* node = rec->new_node(first->first, first->second);
            rec = insert(rec, calc_hash_code(first->first), node);
        }
        return rec;
    } catch (...) {
        rec->destroy(rec->head.next);
        dealloc(rec);
        throw;
    }
}

template<typename... Args>
value::list_links_type* value::record_t::new_node(std::string_view name, Args&&... args) {
    list_node_type* node = list_node_type::alloc_checked(name);
    try {
        new (&node->v.val()) value(std::forward<Args>(args)...);
        return &node->links;
    } catch (...) {
        list_node_type::dealloc(node);
        throw;
    }
}

/*static*/ inline void value::record_t::delete_node(list_links_type* links) {
    list_node_type* node = list_node_type::from_links(links);
    node->v.val().~value();
    list_node_type::dealloc(node);
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
        auto it = this->find(name); \
        if (it != this->nil()) { it->second.as_func(res); } \
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

namespace std {
inline void swap(uxs::db::value& v1, uxs::db::value& v2) NOEXCEPT { v1.swap(v2); }
}  // namespace std
