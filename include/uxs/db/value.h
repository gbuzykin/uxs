#pragma once

#include "exception.h"

#include "uxs/allocator.h"
#include "uxs/dllist.h"
#include "uxs/span.h"
#include "uxs/string_view.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

namespace uxs {
namespace db {
namespace json {
class writer;
}
namespace xml {
class writer;
}

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

template<typename CharT, typename Alloc>
class basic_value;

namespace detail {
template<typename CharT, typename Alloc, typename Ty, typename = void>
struct is_record_value : std::false_type {};
template<typename CharT, typename Alloc, typename FirstTy, typename SecondTy>
struct is_record_value<CharT, Alloc, std::pair<FirstTy, SecondTy>,
                       std::enable_if_t<std::is_convertible<FirstTy, std::basic_string_view<CharT>>::value &&
                                        std::is_convertible<SecondTy, basic_value<CharT, Alloc>>::value>>
    : std::true_type {};
}  // namespace detail

template<typename CharT = char, typename Alloc = std::allocator<CharT>, typename InputIt,
         typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
basic_value<CharT, Alloc> make_array(InputIt first, InputIt last, const Alloc& al = Alloc());

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
basic_value<CharT, Alloc> make_array(std::initializer_list<basic_value<CharT, Alloc>>, const Alloc& al = Alloc());

template<typename CharT = char, typename Alloc = std::allocator<CharT>, typename InputIt,
         typename = std::enable_if_t<is_input_iterator<InputIt>::value>,
         typename = std::enable_if_t<
             detail::is_record_value<CharT, Alloc, typename std::iterator_traits<InputIt>::value_type>::value>>
basic_value<CharT, Alloc> make_record(InputIt first, InputIt last, const Alloc& al = Alloc());

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
basic_value<CharT, Alloc> make_record(
    std::initializer_list<std::pair<std::basic_string_view<CharT>, basic_value<CharT, Alloc>>>,
    const Alloc& al = Alloc());

template<typename CharT, typename Alloc = std::allocator<CharT>>
class UXS_EXPORT basic_value : protected std::allocator_traits<Alloc>::template rebind_alloc<CharT> {
 public:
    using char_type = CharT;
    using allocator_type = Alloc;
    template<typename Ty>
    struct rec_value_proxy;

 private:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<CharT>;

    template<typename Ty>
    struct UXS_EXPORT flexarray_t {
        size_t size;
        size_t capacity;
        typename std::aligned_storage<sizeof(Ty), std::alignment_of<Ty>::value>::type buf[1];

        flexarray_t(const flexarray_t&) = delete;
        flexarray_t& operator=(const flexarray_t&) = delete;

        using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<flexarray_t>;

        span<const Ty> view() const { return as_span(reinterpret_cast<const Ty*>(&buf), size); }
        span<Ty> view() { return as_span(reinterpret_cast<Ty*>(&buf), size); }
        const Ty& operator[](size_t i) const { return reinterpret_cast<const Ty*>(&buf)[i]; }
        Ty& operator[](size_t i) { return reinterpret_cast<Ty*>(&buf)[i]; }

        static size_t max_size(const alloc_type& arr_al);
        static size_t get_alloc_sz(size_t cap);
        static flexarray_t* alloc(alloc_type& arr_al, size_t cap);
        static flexarray_t* alloc_checked(alloc_type& arr_al, size_t cap);
        static flexarray_t* grow(alloc_type& arr_al, flexarray_t* arr, size_t extra);
        static void dealloc(alloc_type& arr_al, flexarray_t* arr);
    };

    using char_flexarray_t = flexarray_t<char_type>;
    using value_flexarray_t = flexarray_t<basic_value>;

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
        using value_type = std::pair<std::basic_string_view<char_type>, basic_value>;
        using size_type = size_t;
        using difference_type = std::ptrdiff_t;
        using reference = rec_value_proxy<basic_value&>;
        using const_reference = rec_value_proxy<const basic_value&>;
        using pointer = reference;
        using const_pointer = const_reference;

        mutable list_links_type head;
        size_t size;
        size_t bucket_count;
        list_links_type* hashtbl[1];

        record_t(const record_t&) = delete;
        record_t& operator=(const record_t&) = delete;

        using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<record_t>;

        void init();
        void destroy(alloc_type& rec_al, list_links_type* node);

        list_links_type* find(std::basic_string_view<char_type> name, size_t hash_code) const;
        size_t count(std::basic_string_view<char_type> name) const;

        static record_t* create(alloc_type& rec_al) {
            record_t* rec = alloc(rec_al, 1);
            rec->init();
            return rec;
        }

        void clear(alloc_type& rec_al) {
            destroy(rec_al, head.next);
            init();
        }

        template<typename InputIt>
        static record_t* create(alloc_type& rec_al, InputIt first, InputIt last);
        static record_t* create(alloc_type& rec_al, const std::initializer_list<basic_value>& init);
        static record_t* create(
            alloc_type& rec_al,
            const std::initializer_list<std::pair<std::basic_string_view<char_type>, basic_value>>& init);
        static record_t* create(alloc_type& rec_al, const record_t& src);

        static record_t* assign(alloc_type& rec_al, record_t* rec, const record_t& src);

        template<typename... Args>
        list_links_type* new_node(alloc_type& rec_al, std::basic_string_view<char_type> name, Args&&... args);
        static void delete_node(alloc_type& rec_al, list_links_type* node);
        static size_t calc_hash_code(std::basic_string_view<char_type> name);
        void add_to_hash(list_links_type* node, size_t hash_code);
        static record_t* insert(alloc_type& rec_al, record_t* rec, size_t hash_code, list_links_type* node);
        list_links_type* erase(alloc_type& rec_al, list_links_type* node);
        size_t erase(alloc_type& rec_al, std::basic_string_view<char_type> name);

        static size_t max_size(const alloc_type& rec_al);
        static size_t get_alloc_sz(size_t bckt_cnt);
        static size_t next_bucket_count(const alloc_type& rec_al, size_t sz);
        static record_t* alloc(alloc_type& rec_al, size_t bckt_cnt);
        static record_t* rehash(alloc_type& rec_al, record_t* rec, size_t bckt_cnt);
        static void dealloc(alloc_type& rec_al, record_t* rec);
    };

 public:
    using record_iterator = list_iterator<record_t, list_node_traits, false>;
    using const_record_iterator = list_iterator<record_t, list_node_traits, true>;

    basic_value() NOEXCEPT_IF(std::is_nothrow_default_constructible<alloc_type>::value)
        : alloc_type(), type_(dtype::kNull) {}
    basic_value(bool b) : alloc_type(), type_(dtype::kBoolean) { value_.b = b; }
    basic_value(int32_t i) : alloc_type(), type_(dtype::kInteger) { value_.i = i; }
    basic_value(uint32_t u) : alloc_type(), type_(dtype::kUInteger) { value_.u = u; }
    basic_value(int64_t i) : alloc_type(), type_(dtype::kInteger64) { value_.i64 = i; }
    basic_value(uint64_t u) : alloc_type(), type_(dtype::kUInteger64) { value_.u64 = u; }
    basic_value(double d) : alloc_type(), type_(dtype::kDouble) { value_.dbl = d; }
    basic_value(std::basic_string_view<char_type> s) : alloc_type(), type_(dtype::kString) {
        value_.str = alloc_string(s);
    }
    basic_value(const char_type* cstr) : alloc_type(), type_(dtype::kString) {
        value_.str = alloc_string(std::basic_string_view<char_type>(cstr));
    }
    basic_value(std::nullptr_t) : alloc_type(), type_(dtype::kNull) {}

    explicit basic_value(const Alloc& al) NOEXCEPT : alloc_type(al), type_(dtype::kNull) {}
    basic_value(bool b, const Alloc& al) : alloc_type(al), type_(dtype::kBoolean) { value_.b = b; }
    basic_value(int32_t i, const Alloc& al) : alloc_type(al), type_(dtype::kInteger) { value_.i = i; }
    basic_value(uint32_t u, const Alloc& al) : alloc_type(al), type_(dtype::kUInteger) { value_.u = u; }
    basic_value(int64_t i, const Alloc& al) : alloc_type(al), type_(dtype::kInteger64) { value_.i64 = i; }
    basic_value(uint64_t u, const Alloc& al) : alloc_type(al), type_(dtype::kUInteger64) { value_.u64 = u; }
    basic_value(double d, const Alloc& al) : alloc_type(al), type_(dtype::kDouble) { value_.dbl = d; }
    basic_value(std::basic_string_view<char_type> s, const Alloc& al) : alloc_type(al), type_(dtype::kString) {
        value_.str = alloc_string(s);
    }
    basic_value(const char_type* cstr, const Alloc& al) : alloc_type(al), type_(dtype::kString) {
        value_.str = alloc_string(std::basic_string_view<char_type>(cstr));
    }
    basic_value(std::nullptr_t, const Alloc& al) : alloc_type(al), type_(dtype::kNull) {}

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    basic_value(InputIt first, InputIt last, const Alloc& al = Alloc()) : alloc_type(al) {
        construct_impl(first, last,
                       detail::is_record_value<CharT, Alloc, typename std::iterator_traits<InputIt>::value_type>());
    }

    basic_value(std::initializer_list<basic_value> init, const Alloc& al = Alloc());

    ~basic_value() {
        if (type_ != dtype::kNull) { destroy(); }
    }

    basic_value(const basic_value& other) : alloc_type(), type_(other.type_) { init_from(other); }
    basic_value(const basic_value& other, const Alloc& al) : alloc_type(al), type_(other.type_) { init_from(other); }
    basic_value& operator=(const basic_value& other) {
        if (&other == this) { return *this; }
        assign_impl(other);
        return *this;
    }

    basic_value(basic_value&& other) NOEXCEPT : alloc_type(std::move(other)), type_(other.type_), value_(other.value_) {
        other.type_ = dtype::kNull;
    }
    basic_value(basic_value&& other, const Alloc& al) NOEXCEPT_IF(is_alloc_always_equal<alloc_type>::value)
        : alloc_type(al), type_(other.type_) {
        construct_impl(std::move(other), is_alloc_always_equal<alloc_type>());
    }
    basic_value& operator=(basic_value&& other) NOEXCEPT {
        if (&other == this) { return *this; }
        if (type_ != dtype::kNull) { destroy(); }
        alloc_type::operator=(std::move(other));
        type_ = other.type_, value_ = other.value_;
        other.type_ = dtype::kNull;
        return *this;
    }

    basic_value& operator=(std::initializer_list<basic_value> init) {
        assign(init);
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    void assign(InputIt first, InputIt last) {
        assign_impl(first, last,
                    detail::is_record_value<CharT, Alloc, typename std::iterator_traits<InputIt>::value_type>());
    }

    void assign(std::initializer_list<basic_value> init);

#define UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(ty, id, field) \
    basic_value& operator=(ty v) { \
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

    basic_value& operator=(std::basic_string_view<char_type> s);
    basic_value& operator=(const char_type* cstr) { return (*this = std::basic_string_view<char_type>(cstr)); }

    basic_value& operator=(std::nullptr_t) {
        if (type_ == dtype::kNull) { return *this; }
        destroy();
        return *this;
    }

    void swap(basic_value& other) NOEXCEPT {
        if (&other == this) { return; }
        std::swap(static_cast<alloc_type&>(*this), static_cast<alloc_type&>(other));
        std::swap(value_, other.value_);
        std::swap(type_, other.type_);
    }

    template<typename CharT_, typename Alloc_>
    friend bool operator==(const basic_value<CharT_, Alloc_>& lhs, const basic_value<CharT_, Alloc_>& rhs);
    template<typename CharT_, typename Alloc_>
    friend bool operator!=(const basic_value<CharT_, Alloc_>& lhs, const basic_value<CharT_, Alloc_>& rhs);

    dtype type() const NOEXCEPT { return type_; }
    allocator_type get_allocator() const NOEXCEPT { return allocator_type(*this); }

    template<typename Ty>
    bool is() const;

    template<typename Ty>
    Ty as() const;

    template<typename Ty>
    Ty get(Ty def) const;

    template<typename Ty>
    Ty get(std::basic_string_view<char_type> name, Ty def) const;

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
    bool as_string(std::basic_string<char_type>& res) const;
    bool as_string_view(std::basic_string_view<char_type>& res) const;

    bool as_bool() const;
    int32_t as_int() const;
    uint32_t as_uint() const;
    int64_t as_int64() const;
    uint64_t as_uint64() const;
    float as_float() const;
    double as_double() const;
    std::basic_string<char_type> as_string() const;
    std::basic_string_view<char_type> as_string_view() const;

    bool convert(dtype type);

    bool empty() const NOEXCEPT;
    size_t size() const NOEXCEPT;
    explicit operator bool() const { return !is_null(); }

    basic_value& append_string(std::basic_string_view<char_type> s);
    basic_value& append_string(const char_type* cstr) { return append_string(std::basic_string_view<char_type>(cstr)); }

    span<const basic_value> as_array() const NOEXCEPT;
    span<basic_value> as_array() NOEXCEPT;

    iterator_range<const_record_iterator> as_record() const;
    iterator_range<record_iterator> as_record();

    const basic_value& operator[](size_t i) const { return as_array()[i]; }
    basic_value& operator[](size_t i) { return as_array()[i]; }

    const basic_value& at(size_t i) const {
        auto range = as_array();
        if (i >= range.size()) { throw exception("index out of range"); }
        return range[i];
    }

    basic_value& at(size_t i) {
        auto range = as_array();
        if (i >= range.size()) { throw exception("index out of range"); }
        return range[i];
    }

    const basic_value& operator[](std::basic_string_view<char_type> name) const;
    basic_value& operator[](std::basic_string_view<char_type> name);

    const_record_iterator find(std::basic_string_view<char_type> name) const;
    record_iterator find(std::basic_string_view<char_type> name);
    const_record_iterator nil() const NOEXCEPT;
    record_iterator nil() NOEXCEPT;
    bool contains(std::basic_string_view<char_type> name) const;
    size_t count(std::basic_string_view<char_type> name) const;

    void clear() NOEXCEPT;
    void reserve(size_t sz);
    void resize(size_t sz);

    template<typename... Args>
    basic_value& emplace_back(Args&&... args);
    void push_back(const basic_value& v) { emplace_back(v); }
    void push_back(basic_value&& v) { emplace_back(std::move(v)); }
    void pop_back();

    template<typename... Args>
    basic_value* emplace(size_t pos, Args&&... args);
    basic_value* insert(size_t pos, const basic_value& v) { return emplace(pos, v); }
    basic_value* insert(size_t pos, basic_value&& v) { return emplace(pos, std::move(v)); }

    template<typename... Args>
    record_iterator emplace(std::basic_string_view<char_type> name, Args&&... args);
    record_iterator insert(std::basic_string_view<char_type> name, const basic_value& v) { return emplace(name, v); }
    record_iterator insert(std::basic_string_view<char_type> name, basic_value&& v) {
        return emplace(name, std::move(v));
    }

    template<typename... Args>
    std::pair<record_iterator, bool> emplace_unique(std::basic_string_view<char_type> name, Args&&... args);
    std::pair<record_iterator, bool> insert_unique(std::basic_string_view<char_type> name, const basic_value& v) {
        return emplace_unique(name, v);
    }
    std::pair<record_iterator, bool> insert_unique(std::basic_string_view<char_type> name, basic_value&& v) {
        return emplace_unique(name, std::move(v));
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    void insert(size_t pos, InputIt first, InputIt last);
    void insert(size_t pos, std::initializer_list<basic_value> init);

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>,
             typename = std::enable_if_t<
                 detail::is_record_value<CharT, Alloc, typename std::iterator_traits<InputIt>::value_type>::value>>
    void insert(InputIt first, InputIt last);
    void insert(std::initializer_list<std::pair<std::basic_string_view<char_type>, basic_value>> init);

    void erase(size_t pos);
    record_iterator erase(const_record_iterator it);
    size_t erase(std::basic_string_view<char_type> name);

 private:
    friend class json::writer;
    friend class xml::writer;
    template<typename CharT_, typename Alloc_>
    friend basic_value<CharT_, Alloc_> make_array();
    template<typename CharT_, typename Alloc_>
    friend basic_value<CharT_, Alloc_> make_array(const Alloc_&);
    template<typename CharT_, typename Alloc_, typename InputIt, typename>
    friend basic_value<CharT_, Alloc_> make_array(InputIt, InputIt, const Alloc_&);
    template<typename CharT_, typename Alloc_>
    friend basic_value<CharT_, Alloc_> make_array(std::initializer_list<basic_value<CharT_, Alloc_>>, const Alloc_&);
    template<typename CharT_, typename Alloc_>
    friend basic_value<CharT_, Alloc_> make_record();
    template<typename CharT_, typename Alloc_>
    friend basic_value<CharT_, Alloc_> make_record(const Alloc_&);
    template<typename CharT_, typename Alloc_, typename InputIt, typename, typename>
    friend basic_value<CharT_, Alloc_> make_record(InputIt, InputIt, const Alloc_&);
    template<typename CharT_, typename Alloc_>
    friend basic_value<CharT_, Alloc_> make_record(
        std::initializer_list<std::pair<std::basic_string_view<CharT_>, basic_value<CharT_, Alloc_>>>, const Alloc_&);

    enum : unsigned { kStartCapacity = 8, kMinBucketCountInc = 12 };

    dtype type_;

    union {
        bool b;
        int32_t i;
        uint32_t u;
        int64_t i64;
        uint64_t u64;
        double dbl;
        char_flexarray_t* str;
        value_flexarray_t* arr;
        record_t* rec;
    } value_;

    std::basic_string_view<char_type> str_view() const {
        return value_.str ? std::basic_string_view<char_type>(&(*value_.str)[0], value_.str->size) :
                            std::basic_string_view<char_type>();
    }
    span<const basic_value> array_view() const { return value_.arr ? value_.arr->view() : span<basic_value>(); }
    span<basic_value> array_view() { return value_.arr ? value_.arr->view() : span<basic_value>(); }

    char_flexarray_t* alloc_string(std::basic_string_view<char_type> s);
    void assign_string(std::basic_string_view<char_type> s);

    template<typename RandIt>
    value_flexarray_t* alloc_array(size_t sz, RandIt src);
    template<typename RandIt>
    value_flexarray_t* alloc_array(RandIt first, RandIt last, std::true_type /* random access iterator */) {
        return alloc_array(static_cast<size_t>(last - first), first);
    }
    template<typename InputIt>
    value_flexarray_t* alloc_array(InputIt first, InputIt last, std::false_type);
    value_flexarray_t* alloc_array(span<const basic_value> v) { return alloc_array(v.size(), v.data()); }

    template<typename RandIt>
    void assign_array(size_t sz, RandIt src);
    template<typename RandIt>
    void assign_array(RandIt first, RandIt last, std::true_type /* random access iterator */) {
        assign_array(static_cast<size_t>(last - first), first);
    }
    template<typename InputIt>
    void assign_array(InputIt first, InputIt last, std::false_type /* random access iterator */);
    void assign_array(span<const basic_value> v) { assign_array(v.size(), v.data()); }

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

    void init_from(const basic_value& other);
    void assign_impl(const basic_value& other);
    void destroy();

    void construct_impl(basic_value&& other, std::true_type) NOEXCEPT {
        type_ = other.type_, value_ = other.value_;
        other.type_ = dtype::kNull;
    }

    void construct_impl(basic_value&& other, std::false_type) {
        if (static_cast<alloc_type&>(*this) == static_cast<alloc_type&>(other)) {
            type_ = other.type_, value_ = other.value_;
            other.type_ = dtype::kNull;
        } else {
            init_from(other);
        }
    }

    template<typename InputIt>
    void construct_impl(InputIt first, InputIt last, std::true_type /* range of pairs */) {
        typename record_t::alloc_type rec_al(*this);
        type_ = dtype::kRecord, value_.rec = record_t::create(rec_al, first, last);
    }

    template<typename InputIt>
    void construct_impl(InputIt first, InputIt last, std::false_type /* range of pairs */) {
        type_ = dtype::kArray, value_.arr = alloc_array(first, last, is_random_access_iterator<InputIt>());
    }

    template<typename InputIt>
    void assign_impl(InputIt first, InputIt last, std::true_type /* range of pairs */);

    template<typename InputIt>
    void assign_impl(InputIt first, InputIt last, std::false_type /* range of pairs */);

    void reserve_back();
    void reserve_string(size_t extra);
    void rotate_back(size_t pos);

    static void move_values(const basic_value* first, const basic_value* last, basic_value* out) {
        for (; first != last; ++first, ++out) {
            new (out) basic_value(static_cast<const Alloc&>(*first));
            out->type_ = first->type_, out->value_ = first->value_;
        }
    }
    template<typename Alloc_, typename = std::enable_if_t<!std::is_same<
                                  Alloc_, std::allocator<typename std::allocator_traits<Alloc_>::value_type>>::value>>
    static void destruct_moved_values(const Alloc_& al, basic_value* first, basic_value* last) {
        std::for_each(first, last, [](decltype(*first) v) { static_cast<Alloc&>(v).~Alloc(); });
    }

    template<typename Ty>
    static void move_values(const Ty* first, const Ty* last, Ty* out) {
        std::memcpy(out, first, (last - first) * sizeof(Ty));
    }
    template<typename Alloc_, typename Ty>
    static void destruct_moved_values(const Alloc_& al, Ty* first, Ty* last) {}
};

template<typename CharT, typename Alloc>
struct basic_value<CharT, Alloc>::rec_value {
    typename std::aligned_storage<sizeof(basic_value), std::alignment_of<basic_value>::value>::type v;
    size_t name_sz;
    char_type name_chars[1];
    std::basic_string_view<char_type> name() const { return std::basic_string_view<char_type>(name_chars, name_sz); }
    const basic_value& val() const { return *reinterpret_cast<const basic_value*>(&v); }
    basic_value& val() { return *reinterpret_cast<basic_value*>(&v); }
};

template<typename CharT, typename Alloc>
template<typename Ty>
struct basic_value<CharT, Alloc>::rec_value_proxy : std::pair<std::basic_string_view<char_type>, Ty&> {
    rec_value_proxy(rec_value& v) : std::pair<std::basic_string_view<char_type>, Ty&>(v.name(), v.val()) {}
    static rec_value_proxy addressof(const rec_value_proxy& proxy) { return proxy; }
    const rec_value_proxy* operator->() const { return this; }
};

template<typename CharT, typename Alloc>
struct basic_value<CharT, Alloc>::list_node_type {
    list_links_type links;
    list_links_type* bucket_next;
    size_t hash_code;
    rec_value v;

    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<list_node_type>;

    static list_node_type* from_links(list_links_type* links) {
        return reinterpret_cast<list_node_type*>(reinterpret_cast<uint8_t*>(links) - offsetof(list_node_type, links));
    }
    static size_t max_name_size(const alloc_type& node_al);
    static size_t get_alloc_sz(size_t name_sz);
    static list_node_type* alloc_checked(alloc_type& node_al, std::basic_string_view<char_type> name);
    static void dealloc(alloc_type& node_al, list_node_type* node);
};

template<typename CharT, typename Alloc>
struct basic_value<CharT, Alloc>::list_node_traits {
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

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
basic_value<CharT, Alloc> make_array() {
    basic_value<CharT, Alloc> v;
    v.type_ = dtype::kArray, v.value_.arr = nullptr;
    return v;
}

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
basic_value<CharT, Alloc> make_array(const Alloc& al) {
    basic_value<CharT, Alloc> v(al);
    v.type_ = dtype::kArray, v.value_.arr = nullptr;
    return v;
}

template<typename CharT, typename Alloc, typename InputIt, typename>
basic_value<CharT, Alloc> make_array(InputIt first, InputIt last, const Alloc& al) {
    basic_value<CharT, Alloc> v(al);
    v.value_.arr = v.alloc_array(first, last, is_random_access_iterator<InputIt>());
    v.type_ = dtype::kArray;
    return v;
}

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc> make_array(std::initializer_list<basic_value<CharT, Alloc>> init, const Alloc& al) {
    basic_value<CharT, Alloc> v(al);
    v.value_.arr = v.alloc_array(init.size(), init.begin());
    v.type_ = dtype::kArray;
    return v;
}

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
basic_value<CharT, Alloc> make_record() {
    basic_value<CharT, Alloc> v;
    typename basic_value<CharT, Alloc>::record_t::alloc_type rec_al(v);
    v.value_.rec = basic_value<CharT, Alloc>::record_t::create(rec_al);
    v.type_ = dtype::kRecord;
    return v;
}

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
basic_value<CharT, Alloc> make_record(const Alloc& al) {
    basic_value<CharT, Alloc> v(al);
    typename basic_value<CharT, Alloc>::record_t::alloc_type rec_al(v);
    v.value_.rec = basic_value<CharT, Alloc>::record_t::create(rec_al);
    v.type_ = dtype::kRecord;
    return v;
}

template<typename CharT, typename Alloc, typename InputIt, typename, typename>
basic_value<CharT, Alloc> make_record(InputIt first, InputIt last, const Alloc& al) {
    basic_value<CharT, Alloc> v(al);
    typename basic_value<CharT, Alloc>::record_t::alloc_type rec_al(v);
    v.value_.rec = basic_value<CharT, Alloc>::record_t::create(rec_al, first, last);
    v.type_ = dtype::kRecord;
    return v;
}

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc> make_record(
    std::initializer_list<std::pair<std::basic_string_view<CharT>, basic_value<CharT, Alloc>>> init, const Alloc& al) {
    basic_value<CharT, Alloc> v(al);
    typename basic_value<CharT, Alloc>::record_t::alloc_type rec_al(v);
    v.value_.rec = basic_value<CharT, Alloc>::record_t::create(rec_al, init);
    v.type_ = dtype::kRecord;
    return v;
}

// --------------------------

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc>& basic_value<CharT, Alloc>::operator=(std::basic_string_view<char_type> s) {
    if (type_ != dtype::kString) {
        if (type_ != dtype::kNull) { destroy(); }
        value_.str = alloc_string(s);
        type_ = dtype::kString;
    } else {
        assign_string(s);
    }
    return *this;
}

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc>& basic_value<CharT, Alloc>::append_string(std::basic_string_view<char_type> s) {
    if (type_ != dtype::kString || !value_.str || value_.str->capacity - value_.str->size < s.size()) {
        reserve_string(s.size());
    }
    std::copy_n(s.data(), s.size(), &(*value_.str)[value_.str->size]);
    value_.str->size += s.size();
    return *this;
}

// --------------------------

template<typename CharT, typename Alloc>
template<typename... Args>
basic_value<CharT, Alloc>& basic_value<CharT, Alloc>::emplace_back(Args&&... args) {
    if (type_ != dtype::kArray || !value_.arr || value_.arr->size == value_.arr->capacity) { reserve_back(); }
    basic_value& v = *new (&(*value_.arr)[value_.arr->size]) basic_value(std::forward<Args>(args)...);
    ++value_.arr->size;
    return v;
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::pop_back() {
    if (type_ != dtype::kArray) { throw exception("not an array"); }
    assert(value_.arr && value_.arr->size);
    (*value_.arr)[--value_.arr->size].~basic_value();
}

template<typename CharT, typename Alloc>
template<typename... Args>
basic_value<CharT, Alloc>* basic_value<CharT, Alloc>::emplace(size_t pos, Args&&... args) {
    if (type_ != dtype::kArray || !value_.arr || value_.arr->size == value_.arr->capacity) { reserve_back(); }
    new (&(*value_.arr)[value_.arr->size]) basic_value(std::forward<Args>(args)...);
    ++value_.arr->size;
    if (pos >= value_.arr->size - 1) { return &(*value_.arr)[value_.arr->size - 1]; }
    rotate_back(pos);
    return &(*value_.arr)[pos];
}

template<typename CharT, typename Alloc>
template<typename... Args>
auto basic_value<CharT, Alloc>::emplace(std::basic_string_view<char_type> name, Args&&... args) -> record_iterator {
    typename record_t::alloc_type rec_al(*this);
    if (type_ != dtype::kRecord) {
        if (type_ != dtype::kNull) { throw exception("not a record"); }
        value_.rec = record_t::create(rec_al);
        type_ = dtype::kRecord;
    }
    list_links_type* node = value_.rec->new_node(rec_al, name, std::forward<Args>(args)...);
    value_.rec = record_t::insert(rec_al, value_.rec, record_t::calc_hash_code(name), node);
    return record_iterator(node);
}

template<typename CharT, typename Alloc>
template<typename... Args>
auto basic_value<CharT, Alloc>::emplace_unique(std::basic_string_view<char_type> name, Args&&... args)
    -> std::pair<record_iterator, bool> {
    typename record_t::alloc_type rec_al(*this);
    if (type_ != dtype::kRecord) {
        if (type_ != dtype::kNull) { throw exception("not a record"); }
        value_.rec = record_t::create(rec_al);
        type_ = dtype::kRecord;
    }
    const size_t hash_code = record_t::calc_hash_code(name);
    list_links_type* node = value_.rec->find(name, hash_code);
    if (node == &value_.rec->head) {
        node = value_.rec->new_node(rec_al, name, std::forward<Args>(args)...);
        value_.rec = record_t::insert(rec_al, value_.rec, hash_code, node);
        return std::make_pair(record_iterator(node), true);
    }
    return std::make_pair(record_iterator(node), false);
}

template<typename CharT, typename Alloc>
template<typename InputIt, typename>
void basic_value<CharT, Alloc>::insert(size_t pos, InputIt first, InputIt last) {
    if (type_ != dtype::kArray) {
        if (type_ != dtype::kNull) { throw exception("not an array"); }
        value_.arr = alloc_array(first, last, is_random_access_iterator<InputIt>());
        type_ = dtype::kArray;
    } else if (first != last) {
        size_t count = append_array(first, last, is_random_access_iterator<InputIt>());
        if (pos < value_.arr->size - count) {
            std::rotate(&(*value_.arr)[pos], &(*value_.arr)[value_.arr->size - count], &(*value_.arr)[value_.arr->size]);
        }
    }
}

template<typename CharT, typename Alloc>
template<typename InputIt, typename, typename>
void basic_value<CharT, Alloc>::insert(InputIt first, InputIt last) {
    typename record_t::alloc_type rec_al(*this);
    if (type_ != dtype::kRecord) {
        if (type_ != dtype::kNull) { throw exception("not a record"); }
        value_.rec = record_t::create(rec_al, first, last);
        type_ = dtype::kRecord;
    } else {
        for (; first != last; ++first) {
            list_links_type* node = value_.rec->new_node(rec_al, first->first, first->second);
            value_.rec = record_t::insert(rec_al, value_.rec, record_t::calc_hash_code(first->first), node);
        }
    }
}

template<typename CharT, typename Alloc>
span<const basic_value<CharT, Alloc>> basic_value<CharT, Alloc>::as_array() const NOEXCEPT {
    if (type_ == dtype::kArray) { return array_view(); }
    return type_ != dtype::kNull ? as_span(this, 1) : span<basic_value>();
}

template<typename CharT, typename Alloc>
span<basic_value<CharT, Alloc>> basic_value<CharT, Alloc>::as_array() NOEXCEPT {
    if (type_ == dtype::kArray) { return array_view(); }
    return type_ != dtype::kNull ? as_span(this, 1) : span<basic_value>();
}

template<typename CharT, typename Alloc>
iterator_range<typename basic_value<CharT, Alloc>::const_record_iterator> basic_value<CharT, Alloc>::as_record() const {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    return uxs::make_range(const_record_iterator(value_.rec->head.next), const_record_iterator(&value_.rec->head));
}

template<typename CharT, typename Alloc>
iterator_range<typename basic_value<CharT, Alloc>::record_iterator> basic_value<CharT, Alloc>::as_record() {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    return uxs::make_range(record_iterator(value_.rec->head.next), record_iterator(&value_.rec->head));
}

template<typename CharT, typename Alloc>
typename basic_value<CharT, Alloc>::const_record_iterator basic_value<CharT, Alloc>::find(
    std::basic_string_view<char_type> name) const {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    return const_record_iterator(value_.rec->find(name, record_t::calc_hash_code(name)));
}

template<typename CharT, typename Alloc>
typename basic_value<CharT, Alloc>::record_iterator basic_value<CharT, Alloc>::find(
    std::basic_string_view<char_type> name) {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    return record_iterator(value_.rec->find(name, record_t::calc_hash_code(name)));
}

template<typename CharT, typename Alloc>
typename basic_value<CharT, Alloc>::const_record_iterator basic_value<CharT, Alloc>::nil() const NOEXCEPT {
    return const_record_iterator(type_ == dtype::kRecord ? &value_.rec->head : nullptr);
}

template<typename CharT, typename Alloc>
typename basic_value<CharT, Alloc>::record_iterator basic_value<CharT, Alloc>::nil() NOEXCEPT {
    return record_iterator(type_ == dtype::kRecord ? &value_.rec->head : nullptr);
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::contains(std::basic_string_view<char_type> name) const {
    return find(name) != nil();
}

template<typename CharT, typename Alloc>
size_t basic_value<CharT, Alloc>::count(std::basic_string_view<char_type> name) const {
    if (type_ != dtype::kRecord) { throw exception("not a record"); }
    return value_.rec->count(name);
}

// --------------------------

template<typename CharT, typename Alloc>
template<typename RandIt>
auto basic_value<CharT, Alloc>::alloc_array(size_t sz, RandIt src) -> value_flexarray_t* {
    if (!sz) { return nullptr; }
    typename value_flexarray_t::alloc_type arr_al(*this);
    value_flexarray_t* arr = value_flexarray_t::alloc_checked(arr_al, sz);
    try {
        std::uninitialized_copy_n(src, sz, &(*arr)[0]);
        arr->size = sz;
        return arr;
    } catch (...) {
        value_flexarray_t::dealloc(arr_al, arr);
        throw;
    }
}

template<typename CharT, typename Alloc>
template<typename InputIt>
auto basic_value<CharT, Alloc>::alloc_array(InputIt first, InputIt last, std::false_type /* random access iterator */)
    -> value_flexarray_t* {
    if (first == last) { return nullptr; }
    typename value_flexarray_t::alloc_type arr_al(*this);
    value_flexarray_t* arr = value_flexarray_t::alloc(arr_al, kStartCapacity);
    arr->size = 0;
    try {
        do {
            if (arr->size == arr->capacity) { arr = value_flexarray_t::grow(arr_al, arr, 1); }
            new (&(*arr)[arr->size]) basic_value(*first);
            ++arr->size;
            ++first;
        } while (first != last);
        return arr;
    } catch (...) {
        std::for_each(&(*arr)[0], &(*arr)[arr->size], [](basic_value& v) { v.~basic_value(); });
        value_flexarray_t::dealloc(arr_al, arr);
        throw;
    }
}

template<typename CharT, typename Alloc>
template<typename RandIt>
void basic_value<CharT, Alloc>::assign_array(size_t sz, RandIt src) {
    if (value_.arr && sz <= value_.arr->capacity) {
        if (sz <= value_.arr->size) {
            basic_value* p = sz ? std::copy_n(src, sz, &(*value_.arr)[0]) : &(*value_.arr)[0];
            std::for_each(p, &p[value_.arr->size - sz], [](basic_value& v) { v.~basic_value(); });
        } else {
            basic_value* p = std::copy_n(src, value_.arr->size, &(*value_.arr)[0]);
            std::uninitialized_copy_n(src + value_.arr->size, sz - value_.arr->size, p);
        }
        value_.arr->size = sz;
    } else {
        value_flexarray_t* new_arr = alloc_array(sz, src);
        if (value_.arr) {
            typename value_flexarray_t::alloc_type arr_al(*this);
            std::for_each(&(*value_.arr)[0], &(*value_.arr)[value_.arr->size], [](basic_value& v) { v.~basic_value(); });
            value_flexarray_t::dealloc(arr_al, value_.arr);
        }
        value_.arr = new_arr;
    }
}

template<typename CharT, typename Alloc>
template<typename InputIt>
void basic_value<CharT, Alloc>::assign_array(InputIt first, InputIt last,
                                             std::false_type /* random access iterator */) {
    if (value_.arr) {
        basic_value *p0 = &(*value_.arr)[0], *p = p0;
        basic_value* p_end = &(*value_.arr)[value_.arr->size];
        for (; p != p_end && first != last; ++first) { *p++ = *first; }
        value_.arr->size = p - p0;
        if (p != p_end) {
            std::for_each(p, p_end, [](basic_value& v) { v.~basic_value(); });
        } else {
            typename value_flexarray_t::alloc_type arr_al(*this);
            for (; first != last; ++first) {
                if (value_.arr->size == value_.arr->capacity) {
                    value_.arr = value_flexarray_t::grow(arr_al, value_.arr, 1);
                }
                new (&(*value_.arr)[value_.arr->size]) basic_value(*first);
                ++value_.arr->size;
            }
        }
    } else {
        value_.arr = alloc_array(first, last, std::false_type());
    }
}

template<typename CharT, typename Alloc>
template<typename RandIt>
void basic_value<CharT, Alloc>::append_array(size_t sz, RandIt src) {
    if (value_.arr) {
        if (sz > value_.arr->capacity - value_.arr->size) {
            typename value_flexarray_t::alloc_type arr_al(*this);
            value_.arr = value_flexarray_t::grow(arr_al, value_.arr, sz);
        }
        std::uninitialized_copy_n(src, sz, &(*value_.arr)[value_.arr->size]);
        value_.arr->size += sz;
    } else {
        value_.arr = alloc_array(sz, src);
    }
}

template<typename CharT, typename Alloc>
template<typename InputIt>
size_t basic_value<CharT, Alloc>::append_array(InputIt first, InputIt last,
                                               std::false_type /* random access iterator */) {
    if (first == last) { return 0; }
    if (value_.arr) {
        typename value_flexarray_t::alloc_type arr_al(*this);
        size_t old_sz = value_.arr->size;
        for (; first != last; ++first) {
            if (value_.arr->size == value_.arr->capacity) {
                value_.arr = value_flexarray_t::grow(arr_al, value_.arr, 1);
            }
            new (&(*value_.arr)[value_.arr->size]) basic_value(*first);
            ++value_.arr->size;
        }
        return value_.arr->size - old_sz;
    }
    value_.arr = alloc_array(first, last, std::false_type());
    return value_.arr->size;
}

template<typename CharT, typename Alloc>
template<typename InputIt>
void basic_value<CharT, Alloc>::assign_impl(InputIt first, InputIt last, std::true_type /* range of pairs */) {
    typename record_t::alloc_type rec_al(*this);
    if (type_ != dtype::kRecord) {
        if (type_ != dtype::kNull) { destroy(); }
        value_.rec = record_t::create(rec_al, first, last);
        type_ = dtype::kRecord;
    } else {
        value_.rec->clear(rec_al);
        for (; first != last; ++first) {
            list_links_type* node = value_.rec->new_node(rec_al, first->first, first->second);
            value_.rec = record_t::insert(rec_al, value_.rec, record_t::calc_hash_code(first->first), node);
        }
    }
}

template<typename CharT, typename Alloc>
template<typename InputIt>
void basic_value<CharT, Alloc>::assign_impl(InputIt first, InputIt last, std::false_type /* range of pairs */) {
    if (type_ != dtype::kArray) {
        if (type_ != dtype::kNull) { destroy(); }
        value_.arr = alloc_array(first, last, is_random_access_iterator<InputIt>());
        type_ = dtype::kArray;
    } else {
        assign_array(first, last, is_random_access_iterator<InputIt>());
    }
}

template<typename CharT, typename Alloc>
template<typename InputIt>
/*static*/ typename basic_value<CharT, Alloc>::record_t*  //
basic_value<CharT, Alloc>::record_t::create(alloc_type& rec_al, InputIt first, InputIt last) {
    record_t* rec = create(rec_al);
    try {
        for (; first != last; ++first) {
            list_links_type* node = rec->new_node(rec_al, first->first, first->second);
            rec = insert(rec_al, rec, calc_hash_code(first->first), node);
        }
        return rec;
    } catch (...) {
        rec->destroy(rec_al, rec->head.next);
        dealloc(rec_al, rec);
        throw;
    }
}

template<typename CharT, typename Alloc>
template<typename... Args>
typename basic_value<CharT, Alloc>::list_links_type* basic_value<CharT, Alloc>::record_t::new_node(
    alloc_type& rec_al, std::basic_string_view<char_type> name, Args&&... args) {
    typename list_node_type::alloc_type node_al(rec_al);
    list_node_type* node = list_node_type::alloc_checked(node_al, name);
    try {
        new (&node->v.val()) basic_value(std::forward<Args>(args)...);
        return &node->links;
    } catch (...) {
        list_node_type::dealloc(node_al, node);
        throw;
    }
}

template<typename CharT, typename Alloc>
/*static*/ void basic_value<CharT, Alloc>::record_t::delete_node(alloc_type& rec_al, list_links_type* links) {
    list_node_type* node = list_node_type::from_links(links);
    node->v.val().~basic_value();
    typename list_node_type::alloc_type node_al(rec_al);
    list_node_type::dealloc(node_al, node);
}

namespace detail {
template<typename ValTy, typename Ty>
struct value_getters_specializer;
#define UXS_DB_VALUE_IMPLEMENT_GETTER_SPECIALIZER(ty, is_func, as_func) \
    template<typename ValTy> \
    struct value_getters_specializer<ValTy, ty> { \
        static bool is(const ValTy& v) { return v.is_func(); } \
        static ty as(const ValTy& v) { return v.as_func(); } \
        static ty get(const ValTy& v, ty& def) { \
            ty res(std::move(def)); \
            v.as_func(res); \
            return res; \
        } \
        static ty get(const ValTy& v, std::basic_string_view<typename ValTy::char_type> name, ty& def) { \
            ty res(std::move(def)); \
            auto it = v.find(name); \
            if (it != v.nil()) { it->second.as_func(res); } \
            return res; \
        } \
    };
UXS_DB_VALUE_IMPLEMENT_GETTER_SPECIALIZER(bool, is_bool, as_bool)
UXS_DB_VALUE_IMPLEMENT_GETTER_SPECIALIZER(int32_t, is_int, as_int)
UXS_DB_VALUE_IMPLEMENT_GETTER_SPECIALIZER(uint32_t, is_uint, as_uint)
UXS_DB_VALUE_IMPLEMENT_GETTER_SPECIALIZER(int64_t, is_int64, as_int64)
UXS_DB_VALUE_IMPLEMENT_GETTER_SPECIALIZER(uint64_t, is_uint64, as_uint64)
UXS_DB_VALUE_IMPLEMENT_GETTER_SPECIALIZER(float, is_float, as_float)
UXS_DB_VALUE_IMPLEMENT_GETTER_SPECIALIZER(double, is_double, as_double)
UXS_DB_VALUE_IMPLEMENT_GETTER_SPECIALIZER(std::basic_string<typename ValTy::char_type>, is_string, as_string)
UXS_DB_VALUE_IMPLEMENT_GETTER_SPECIALIZER(std::basic_string_view<typename ValTy::char_type>, is_string, as_string_view)
#undef UXS_DB_VALUE_IMPLEMENT_SCALAR_IS_GET
}  // namespace detail

template<typename CharT, typename Alloc>
template<typename Ty>
bool basic_value<CharT, Alloc>::is() const {
    return detail::value_getters_specializer<basic_value, Ty>::is();
}

template<typename CharT, typename Alloc>
template<typename Ty>
Ty basic_value<CharT, Alloc>::as() const {
    return detail::value_getters_specializer<basic_value, Ty>::as();
}

template<typename CharT, typename Alloc>
template<typename Ty>
Ty basic_value<CharT, Alloc>::get(Ty def) const {
    return detail::value_getters_specializer<basic_value, Ty>::get(def);
}

template<typename CharT, typename Alloc>
template<typename Ty>
Ty basic_value<CharT, Alloc>::get(std::basic_string_view<char_type> name, Ty def) const {
    return detail::value_getters_specializer<basic_value, Ty>::get(name, def);
}

#define UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(ty, as_func) \
    template<typename CharT, typename Alloc> \
    ty basic_value<CharT, Alloc>::as_func() const { \
        ty res; \
        if (!this->as_func(res)) { throw exception("not convertible to " #ty); } \
        return res; \
    }
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(bool, as_bool)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(int32_t, as_int)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(uint32_t, as_uint)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(int64_t, as_int64)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(uint64_t, as_uint64)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(float, as_float)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(double, as_double)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(std::basic_string<CharT>, as_string)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET(std::basic_string_view<CharT>, as_string_view)
#undef UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_GET

template<typename CharT, typename Alloc>
bool operator!=(const basic_value<CharT, Alloc>& lhs, const basic_value<CharT, Alloc>& rhs) {
    return !(lhs == rhs);
}

using value = basic_value<char>;

}  // namespace db
}  // namespace uxs

namespace std {
template<typename CharT, typename Alloc>
void swap(uxs::db::basic_value<CharT, Alloc>& v1, uxs::db::basic_value<CharT, Alloc>& v2) NOEXCEPT {
    v1.swap(v2);
}
}  // namespace std
