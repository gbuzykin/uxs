#pragma once

#include "database_error.h"

#include "uxs/dllist.h"
#include "uxs/iterator.h"
#include "uxs/memory.h"
#include "uxs/optional.h"
#include "uxs/span.h"
#include "uxs/string_view.h"

#include <algorithm>
#include <cstring>

namespace uxs {
namespace db {
namespace json {
class writer;
}
namespace xml {
class writer;
}

enum class dtype {
    null = 0,
    boolean,
    integer,
    unsigned_integer,
    long_integer,
    unsigned_long_integer,
    double_precision,
    string,
    array,
    record,
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

template<typename CharT, typename Alloc, bool store_values>
struct flexarray_t {
    using array_value_t = std::conditional_t<store_values, basic_value<CharT, Alloc>, CharT>;
    std::size_t size;
    std::size_t capacity;
    alignas(std::alignment_of<array_value_t>::value) std::uint8_t x[sizeof(array_value_t)];

    enum : unsigned { start_capacity = 8 };

    flexarray_t(const flexarray_t&) = delete;
    flexarray_t& operator=(const flexarray_t&) = delete;

    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<flexarray_t>;

    uxs::span<const array_value_t> view() const {
        return uxs::as_span(reinterpret_cast<const array_value_t*>(&x), size);
    }
    uxs::span<array_value_t> view() { return uxs::as_span(reinterpret_cast<array_value_t*>(&x), size); }
    const array_value_t& operator[](std::size_t i) const { return reinterpret_cast<const array_value_t*>(&x)[i]; }
    array_value_t& operator[](std::size_t i) { return reinterpret_cast<array_value_t*>(&x)[i]; }

    static std::size_t max_size(const alloc_type& arr_al) {
        return (std::allocator_traits<alloc_type>::max_size(arr_al) * sizeof(flexarray_t) - offsetof(flexarray_t, x)) /
               sizeof(array_value_t);
    }

    static std::size_t get_alloc_sz(std::size_t cap) {
        return (offsetof(flexarray_t, x) + cap * sizeof(array_value_t) + sizeof(flexarray_t) - 1) / sizeof(flexarray_t);
    }

    UXS_EXPORT static flexarray_t* alloc(alloc_type& arr_al, std::size_t cap);
    UXS_EXPORT static flexarray_t* grow(alloc_type& arr_al, flexarray_t* arr, std::size_t extra);

    static void dealloc(alloc_type& arr_al, flexarray_t* arr) { arr_al.deallocate(arr, get_alloc_sz(arr->capacity)); }

    static flexarray_t* alloc_checked(alloc_type& arr_al, std::size_t cap) {
        if (cap > max_size(arr_al)) { throw std::length_error("too much to reserve"); }
        return alloc(arr_al, cap);
    }
};

template<typename Ty>
struct record_value {
    using char_type = typename Ty::char_type;
    alignas(std::alignment_of<Ty>::value) std::uint8_t x[sizeof(Ty)];
    std::size_t name_sz;
    char_type name_chars[1];
    std::basic_string_view<char_type> name() const { return std::basic_string_view<char_type>(name_chars, name_sz); }
    const Ty& val() const { return *reinterpret_cast<const Ty*>(&x); }
    Ty& val() { return *reinterpret_cast<Ty*>(&x); }
};

template<typename Ty>
struct record_value_proxy : std::pair<std::basic_string_view<typename Ty::char_type>, Ty&> {
    record_value_proxy(record_value<std::remove_const_t<Ty>>& v)
        : std::pair<std::basic_string_view<typename Ty::char_type>, Ty&>(v.name(), v.val()) {}
    static record_value_proxy addressof(const record_value_proxy& proxy) { return proxy; }
    const record_value_proxy* operator->() const { return this; }
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

template<typename CharT, typename Alloc>
struct record_node_type {
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<record_node_type>;

    list_links_type links;
    list_links_type* next_bucket;
    std::size_t hash_code;
    record_value<basic_value<CharT, Alloc>> v;

    static record_node_type* from_links(list_links_type* links) {
        return get_containing_record<record_node_type, offsetof(record_node_type, links)>(links);
    }

    static std::size_t max_name_size(const alloc_type& node_al) {
        return (std::allocator_traits<alloc_type>::max_size(node_al) * sizeof(record_node_type) -
                offsetof(record_node_type, v.name_chars)) /
               sizeof(CharT);
    }

    static std::size_t get_alloc_sz(std::size_t name_sz) {
        return (offsetof(record_node_type, v.name_chars) + name_sz * sizeof(CharT) + sizeof(record_node_type) - 1) /
               sizeof(record_node_type);
    }

    UXS_EXPORT static record_node_type* alloc_checked(alloc_type& node_al, std::basic_string_view<CharT> name);

    static void dealloc(alloc_type& node_al, record_node_type* node) {
        node_al.deallocate(node, get_alloc_sz(node->v.name_sz));
    }
};

template<typename CharT, typename Alloc>
struct record_node_traits {
    using iterator_node_t = list_links_type;
    using node_t = record_node_type<CharT, Alloc>;
    static list_links_type* get_next(list_links_type* node) { return node->next; }
    static list_links_type* get_prev(list_links_type* node) { return node->prev; }
#if _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(list_links_type* node, list_links_type* head) { node->head = head; }
    static list_links_type* get_head(list_links_type* node) { return node->head; }
    static list_links_type* get_front(list_links_type* head) { return head->next; }
#else   // _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(list_links_type* node, list_links_type* head) {}
#endif  // _ITERATOR_DEBUG_LEVEL != 0
    static record_value<basic_value<CharT, Alloc>>& get_value(list_links_type* node) {
        return node_t::from_links(node)->v;
    }
};

template<typename CharT, typename Alloc>
struct record_t {
    using value_type = std::pair<std::basic_string_view<CharT>, basic_value<CharT, Alloc>>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = record_value_proxy<basic_value<CharT, Alloc>>;
    using const_reference = record_value_proxy<const basic_value<CharT, Alloc>>;
    using pointer = reference;
    using const_pointer = const_reference;
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<record_t>;
    using node_traits = record_node_traits<CharT, Alloc>;
    using node_t = typename node_traits::node_t;
    using hasher = std::hash<std::basic_string_view<CharT>>;

    mutable list_links_type head;
    std::size_t size;
    std::size_t bucket_count;
    list_links_type* hashtbl[1];

    enum : unsigned { min_bucket_count_inc = 12 };

    record_t(const record_t&) = delete;
    record_t& operator=(const record_t&) = delete;

    UXS_EXPORT void init();
    UXS_EXPORT void destroy(alloc_type& rec_al, list_links_type* node);

    UXS_EXPORT list_links_type* find(std::basic_string_view<CharT> name, std::size_t hash_code) const;
    UXS_EXPORT std::size_t count(std::basic_string_view<CharT> name) const;

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
    UXS_EXPORT static record_t* create(alloc_type& rec_al, const std::initializer_list<basic_value<CharT, Alloc>>& init);
    UXS_EXPORT static record_t* create(
        alloc_type& rec_al,
        const std::initializer_list<std::pair<std::basic_string_view<CharT>, basic_value<CharT, Alloc>>>& init);

    static record_t* create(alloc_type& rec_al, const record_t& src);
    static record_t* assign(alloc_type& rec_al, record_t* rec, const record_t& src);

    template<typename... Args>
    list_links_type* new_node(alloc_type& rec_al, std::basic_string_view<CharT> name, Args&&... args);
    static void delete_node(alloc_type& rec_al, list_links_type* node);
    void add_to_hash(list_links_type* node, std::size_t hash_code);
    UXS_EXPORT static record_t* insert(alloc_type& rec_al, record_t* rec, std::size_t hash_code, list_links_type* node);
    list_links_type* erase(alloc_type& rec_al, list_links_type* node);
    std::size_t erase(alloc_type& rec_al, std::basic_string_view<CharT> name);

    static std::size_t max_size(const alloc_type& rec_al) {
        return (std::allocator_traits<alloc_type>::max_size(rec_al) * sizeof(record_t) - offsetof(record_t, hashtbl)) /
               sizeof(list_links_type*);
    }

    static std::size_t get_alloc_sz(std::size_t bckt_cnt) {
        return (offsetof(record_t, hashtbl) + bckt_cnt * sizeof(list_links_type*) + sizeof(record_t) - 1) /
               sizeof(record_t);
    }

    static std::size_t next_bucket_count(const alloc_type& rec_al, std::size_t sz);
    UXS_EXPORT static record_t* alloc(alloc_type& rec_al, std::size_t bckt_cnt);
    static record_t* rehash(alloc_type& rec_al, record_t* rec, std::size_t bckt_cnt);
    UXS_EXPORT static void dealloc(alloc_type& rec_al, record_t* rec) {
        rec_al.deallocate(rec, get_alloc_sz(rec->bucket_count));
    }
};

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
class basic_value : protected std::allocator_traits<Alloc>::template rebind_alloc<CharT> {
 public:
    using char_type = CharT;
    using allocator_type = Alloc;

 private:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<CharT>;
    using char_flexarray_t = detail::flexarray_t<char_type, Alloc, false>;
    using value_flexarray_t = detail::flexarray_t<char_type, Alloc, true>;
    using record_t = detail::record_t<CharT, Alloc>;

 public:
    using record_iterator = list_iterator<record_t, typename record_t::node_traits, false>;
    using const_record_iterator = list_iterator<record_t, typename record_t::node_traits, true>;

    basic_value() noexcept(std::is_nothrow_default_constructible<alloc_type>::value)
        : alloc_type(), type_(dtype::null) {}
    basic_value(bool b) : alloc_type(), type_(dtype::boolean) { value_.b = b; }
    basic_value(std::int32_t i) : alloc_type(), type_(dtype::integer) { value_.i = i; }
    basic_value(std::uint32_t u) : alloc_type(), type_(dtype::unsigned_integer) { value_.u = u; }
    basic_value(std::int64_t i) : alloc_type(), type_(dtype::long_integer) { value_.i64 = i; }
    basic_value(std::uint64_t u) : alloc_type(), type_(dtype::unsigned_long_integer) { value_.u64 = u; }
    basic_value(float f) : alloc_type(), type_(dtype::double_precision) { value_.dbl = f; }
    basic_value(double d) : alloc_type(), type_(dtype::double_precision) { value_.dbl = d; }
    basic_value(std::basic_string_view<char_type> s) : alloc_type(), type_(dtype::string) {
        value_.str = alloc_string(s);
    }
    basic_value(const char_type* cstr) : alloc_type(), type_(dtype::string) {
        value_.str = alloc_string(std::basic_string_view<char_type>(cstr));
    }
    basic_value(std::nullptr_t) noexcept(std::is_nothrow_default_constructible<alloc_type>::value)
        : alloc_type(), type_(dtype::null) {}

    explicit basic_value(const Alloc& al) noexcept : alloc_type(al), type_(dtype::null) {}
    basic_value(bool b, const Alloc& al) : alloc_type(al), type_(dtype::boolean) { value_.b = b; }
    basic_value(std::int32_t i, const Alloc& al) : alloc_type(al), type_(dtype::integer) { value_.i = i; }
    basic_value(std::uint32_t u, const Alloc& al) : alloc_type(al), type_(dtype::unsigned_integer) { value_.u = u; }
    basic_value(std::int64_t i, const Alloc& al) : alloc_type(al), type_(dtype::long_integer) { value_.i64 = i; }
    basic_value(std::uint64_t u, const Alloc& al) : alloc_type(al), type_(dtype::unsigned_long_integer) {
        value_.u64 = u;
    }
    basic_value(float f, const Alloc& al) : alloc_type(al), type_(dtype::double_precision) { value_.dbl = f; }
    basic_value(double d, const Alloc& al) : alloc_type(al), type_(dtype::double_precision) { value_.dbl = d; }
    basic_value(std::basic_string_view<char_type> s, const Alloc& al) : alloc_type(al), type_(dtype::string) {
        value_.str = alloc_string(s);
    }
    basic_value(const char_type* cstr, const Alloc& al) : alloc_type(al), type_(dtype::string) {
        value_.str = alloc_string(std::basic_string_view<char_type>(cstr));
    }
    basic_value(std::nullptr_t, const Alloc& al) noexcept : alloc_type(al), type_(dtype::null) {}

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    basic_value(InputIt first, InputIt last, const Alloc& al = Alloc()) : alloc_type(al) {
        construct_impl(first, last,
                       detail::is_record_value<CharT, Alloc, typename std::iterator_traits<InputIt>::value_type>());
    }

    UXS_EXPORT basic_value(std::initializer_list<basic_value> init, const Alloc& al = Alloc());

    ~basic_value() {
        if (type_ != dtype::null) { destroy(); }
    }

    basic_value(const basic_value& other) : alloc_type(), type_(other.type_) { init_from(other); }
    basic_value(const basic_value& other, const Alloc& al) : alloc_type(al), type_(other.type_) { init_from(other); }
    basic_value& operator=(const basic_value& other) {
        if (&other == this) { return *this; }
        assign_impl(other);
        return *this;
    }

    basic_value(basic_value&& other) noexcept : alloc_type(std::move(other)), type_(other.type_), value_(other.value_) {
        other.type_ = dtype::null;
    }
    basic_value(basic_value&& other, const Alloc& al) noexcept(is_alloc_always_equal<alloc_type>::value)
        : alloc_type(al), type_(other.type_) {
        construct_impl(std::move(other), is_alloc_always_equal<alloc_type>());
    }
    basic_value& operator=(basic_value&& other) noexcept {
        if (&other == this) { return *this; }
        if (type_ != dtype::null) { destroy(); }
        alloc_type::operator=(std::move(other));
        type_ = other.type_, value_ = other.value_;
        other.type_ = dtype::null;
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

    UXS_EXPORT void assign(std::initializer_list<basic_value> init);

#define UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(ty, id, field) \
    basic_value& operator=(ty v) { \
        if (type_ != dtype::null) { destroy(); } \
        type_ = id, value_.field = v; \
        return *this; \
    }
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(bool, dtype::boolean, b)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(std::int32_t, dtype::integer, i)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(std::uint32_t, dtype::unsigned_integer, u)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(std::int64_t, dtype::long_integer, i64)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(std::uint64_t, dtype::unsigned_long_integer, u64)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(float, dtype::double_precision, dbl)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT(double, dtype::double_precision, dbl)
#undef UXS_DB_VALUE_IMPLEMENT_SCALAR_ASSIGNMENT

    basic_value& operator=(std::basic_string_view<char_type> s);
    basic_value& operator=(const char_type* cstr) { return (*this = std::basic_string_view<char_type>(cstr)); }

    basic_value& operator=(std::nullptr_t) {
        if (type_ == dtype::null) { return *this; }
        destroy();
        return *this;
    }

    void swap(basic_value& other) noexcept {
        if (&other == this) { return; }
        std::swap(static_cast<alloc_type&>(*this), static_cast<alloc_type&>(other));
        std::swap(value_, other.value_);
        std::swap(type_, other.type_);
    }

    template<typename CharT_, typename Alloc_>
    friend UXS_EXPORT bool operator==(const basic_value<CharT_, Alloc_>& lhs,
                                      const basic_value<CharT_, Alloc_>& rhs) noexcept;
    template<typename CharT_, typename Alloc_>
    friend bool operator!=(const basic_value<CharT_, Alloc_>& lhs, const basic_value<CharT_, Alloc_>& rhs) noexcept;

    dtype type() const noexcept { return type_; }
    allocator_type get_allocator() const noexcept { return allocator_type(*this); }

    template<typename Ty>
    bool is() const noexcept;

    template<typename Ty>
    Ty as() const;

    template<typename Ty>
    uxs::optional<Ty> get() const;

    template<typename Ty, typename U>
    Ty value_or(U&& default_value) const {
        auto result = get<Ty>();
        return result ? *result : Ty(std::forward<U>(default_value));
    }

    template<typename Ty, typename U>
    Ty value_or(std::basic_string_view<char_type> name, U&& default_value) const {
        auto it = find(name);
        if (it != nil()) {
            auto result = it->second.template get<Ty>();
            if (result) { return *result; }
        }
        return Ty(std::forward<U>(default_value));
    }

    template<typename Ty>
    Ty value() const {
        return value_or<Ty>(Ty());
    }

    template<typename Ty>
    Ty value(std::basic_string_view<char_type> name) const {
        return value_or<Ty>(name, Ty());
    }

    bool is_null() const noexcept { return type_ == dtype::null; }
    bool is_bool() const noexcept { return type_ == dtype::boolean; }
    UXS_EXPORT bool is_int() const noexcept;
    UXS_EXPORT bool is_uint() const noexcept;
    UXS_EXPORT bool is_int64() const noexcept;
    UXS_EXPORT bool is_uint64() const noexcept;
    UXS_EXPORT bool is_integral() const noexcept;
    bool is_float() const noexcept { return is_numeric(); }
    bool is_double() const noexcept { return is_numeric(); }
    bool is_numeric() const noexcept { return type_ >= dtype::integer && type_ <= dtype::double_precision; }
    bool is_string() const noexcept { return type_ == dtype::string; }
    bool is_array() const noexcept { return type_ == dtype::array; }
    bool is_record() const noexcept { return type_ == dtype::record; }

    UXS_EXPORT uxs::optional<bool> get_bool() const;
    UXS_EXPORT uxs::optional<std::int32_t> get_int() const;
    UXS_EXPORT uxs::optional<std::uint32_t> get_uint() const;
    UXS_EXPORT uxs::optional<std::int64_t> get_int64() const;
    UXS_EXPORT uxs::optional<std::uint64_t> get_uint64() const;
    UXS_EXPORT uxs::optional<float> get_float() const;
    UXS_EXPORT uxs::optional<double> get_double() const;
    UXS_EXPORT uxs::optional<std::basic_string<char_type>> get_string() const;
    UXS_EXPORT uxs::optional<std::basic_string_view<char_type>> get_string_view() const;

    bool as_bool() const;
    std::int32_t as_int() const;
    std::uint32_t as_uint() const;
    std::int64_t as_int64() const;
    std::uint64_t as_uint64() const;
    float as_float() const;
    double as_double() const;
    std::basic_string<char_type> as_string() const;
    std::basic_string_view<char_type> as_string_view() const;

    UXS_EXPORT bool convert(dtype type);

    UXS_EXPORT bool empty() const noexcept;
    UXS_EXPORT std::size_t size() const noexcept;
    explicit operator bool() const { return !is_null(); }

    basic_value& append_string(std::basic_string_view<char_type> s);
    basic_value& append_string(const char_type* cstr) { return append_string(std::basic_string_view<char_type>(cstr)); }

    uxs::span<const basic_value> as_array() const noexcept;
    uxs::span<basic_value> as_array() noexcept;

    iterator_range<const_record_iterator> as_record() const;
    iterator_range<record_iterator> as_record();

    const basic_value& operator[](std::size_t i) const { return as_array()[i]; }
    basic_value& operator[](std::size_t i) { return as_array()[i]; }

    const basic_value& at(std::size_t i) const {
        auto range = as_array();
        if (i < range.size()) { return range[i]; }
        throw database_error("index out of range");
    }

    basic_value& at(std::size_t i) {
        auto range = as_array();
        if (i < range.size()) { return range[i]; }
        throw database_error("index out of range");
    }

    UXS_EXPORT const basic_value& operator[](std::basic_string_view<char_type> name) const;
    UXS_EXPORT basic_value& operator[](std::basic_string_view<char_type> name);

    const_record_iterator find(std::basic_string_view<char_type> name) const;
    record_iterator find(std::basic_string_view<char_type> name);
    const_record_iterator nil() const noexcept;
    record_iterator nil() noexcept;
    bool contains(std::basic_string_view<char_type> name) const;
    std::size_t count(std::basic_string_view<char_type> name) const;

    UXS_EXPORT void clear() noexcept;
    UXS_EXPORT void reserve(std::size_t sz);
    UXS_EXPORT void resize(std::size_t sz);

    template<typename... Args>
    basic_value& emplace_back(Args&&... args);
    void push_back(const basic_value& v) { emplace_back(v); }
    void push_back(basic_value&& v) { emplace_back(std::move(v)); }
    void pop_back();

    template<typename... Args>
    basic_value* emplace(std::size_t pos, Args&&... args);
    basic_value* insert(std::size_t pos, const basic_value& v) { return emplace(pos, v); }
    basic_value* insert(std::size_t pos, basic_value&& v) { return emplace(pos, std::move(v)); }

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
    void insert(std::size_t pos, InputIt first, InputIt last);
    UXS_EXPORT void insert(std::size_t pos, std::initializer_list<basic_value> init);

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>,
             typename = std::enable_if_t<
                 detail::is_record_value<CharT, Alloc, typename std::iterator_traits<InputIt>::value_type>::value>>
    void insert(InputIt first, InputIt last);
    UXS_EXPORT void insert(std::initializer_list<std::pair<std::basic_string_view<char_type>, basic_value>> init);

    UXS_EXPORT void erase(std::size_t pos);
    UXS_EXPORT record_iterator erase(const_record_iterator it);
    UXS_EXPORT std::size_t erase(std::basic_string_view<char_type> name);

 private:
    friend class json::writer;
    friend class xml::writer;
    friend struct detail::record_t<CharT, Alloc>;
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

    dtype type_;

    union {
        bool b;
        std::int32_t i;
        std::uint32_t u;
        std::int64_t i64;
        std::uint64_t u64;
        double dbl;
        char_flexarray_t* str;
        value_flexarray_t* arr;
        record_t* rec;
    } value_;

    std::basic_string_view<char_type> str_view() const {
        return value_.str ? std::basic_string_view<char_type>(&(*value_.str)[0], value_.str->size) :
                            std::basic_string_view<char_type>();
    }
    uxs::span<const basic_value> array_view() const {
        return value_.arr ? value_.arr->view() : uxs::span<basic_value>();
    }
    uxs::span<basic_value> array_view() { return value_.arr ? value_.arr->view() : uxs::span<basic_value>(); }

    UXS_EXPORT char_flexarray_t* alloc_string(std::basic_string_view<char_type> s);
    UXS_EXPORT void assign_string(std::basic_string_view<char_type> s);

    template<typename RandIt>
    value_flexarray_t* alloc_array(std::size_t sz, RandIt src);
    template<typename RandIt>
    value_flexarray_t* alloc_array(RandIt first, RandIt last, std::true_type /* random access iterator */) {
        return alloc_array(static_cast<std::size_t>(last - first), first);
    }
    template<typename InputIt>
    value_flexarray_t* alloc_array(InputIt first, InputIt last, std::false_type);
    value_flexarray_t* alloc_array(uxs::span<const basic_value> v) { return alloc_array(v.size(), v.data()); }

    template<typename RandIt>
    void assign_array(std::size_t sz, RandIt src);
    template<typename RandIt>
    void assign_array(RandIt first, RandIt last, std::true_type /* random access iterator */) {
        assign_array(static_cast<std::size_t>(last - first), first);
    }
    template<typename InputIt>
    void assign_array(InputIt first, InputIt last, std::false_type /* random access iterator */);
    void assign_array(uxs::span<const basic_value> v) { assign_array(v.size(), v.data()); }

    template<typename RandIt>
    void append_array(std::size_t sz, RandIt src);
    template<typename RandIt>
    std::size_t append_array(RandIt first, RandIt last, std::true_type /* random access iterator */) {
        std::size_t count = static_cast<std::size_t>(last - first);
        append_array(count, first);
        return count;
    }
    template<typename InputIt>
    std::size_t append_array(InputIt first, InputIt last, std::false_type /* random access iterator */);

    UXS_EXPORT void init_from(const basic_value& other);
    UXS_EXPORT void assign_impl(const basic_value& other);
    UXS_EXPORT void destroy();

    void construct_impl(basic_value&& other, std::true_type) noexcept {
        type_ = other.type_, value_ = other.value_;
        other.type_ = dtype::null;
    }

    void construct_impl(basic_value&& other, std::false_type) {
        if (static_cast<alloc_type&>(*this) == static_cast<alloc_type&>(other)) {
            type_ = other.type_, value_ = other.value_;
            other.type_ = dtype::null;
        } else {
            init_from(other);
        }
    }

    template<typename InputIt>
    void construct_impl(InputIt first, InputIt last, std::true_type /* range of pairs */) {
        typename record_t::alloc_type rec_al(*this);
        type_ = dtype::record, value_.rec = record_t::create(rec_al, first, last);
    }

    template<typename InputIt>
    void construct_impl(InputIt first, InputIt last, std::false_type /* range of pairs */) {
        type_ = dtype::array, value_.arr = alloc_array(first, last, is_random_access_iterator<InputIt>());
    }

    template<typename InputIt>
    void assign_impl(InputIt first, InputIt last, std::true_type /* range of pairs */);

    template<typename InputIt>
    void assign_impl(InputIt first, InputIt last, std::false_type /* range of pairs */);

    UXS_EXPORT void reserve_back();
    UXS_EXPORT void reserve_string(std::size_t extra);
    UXS_EXPORT void rotate_back(std::size_t pos);
};

// --------------------------

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
basic_value<CharT, Alloc> make_array() {
    basic_value<CharT, Alloc> v;
    v.type_ = dtype::array, v.value_.arr = nullptr;
    return v;
}

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
basic_value<CharT, Alloc> make_array(const Alloc& al) {
    basic_value<CharT, Alloc> v(al);
    v.type_ = dtype::array, v.value_.arr = nullptr;
    return v;
}

template<typename CharT, typename Alloc, typename InputIt, typename>
basic_value<CharT, Alloc> make_array(InputIt first, InputIt last, const Alloc& al) {
    basic_value<CharT, Alloc> v(al);
    v.value_.arr = v.alloc_array(first, last, is_random_access_iterator<InputIt>());
    v.type_ = dtype::array;
    return v;
}

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc> make_array(std::initializer_list<basic_value<CharT, Alloc>> init, const Alloc& al) {
    basic_value<CharT, Alloc> v(al);
    v.value_.arr = v.alloc_array(init.size(), init.begin());
    v.type_ = dtype::array;
    return v;
}

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
basic_value<CharT, Alloc> make_record() {
    basic_value<CharT, Alloc> v;
    typename basic_value<CharT, Alloc>::record_t::alloc_type rec_al(v);
    v.value_.rec = basic_value<CharT, Alloc>::record_t::create(rec_al);
    v.type_ = dtype::record;
    return v;
}

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
basic_value<CharT, Alloc> make_record(const Alloc& al) {
    basic_value<CharT, Alloc> v(al);
    typename basic_value<CharT, Alloc>::record_t::alloc_type rec_al(v);
    v.value_.rec = basic_value<CharT, Alloc>::record_t::create(rec_al);
    v.type_ = dtype::record;
    return v;
}

template<typename CharT, typename Alloc, typename InputIt, typename, typename>
basic_value<CharT, Alloc> make_record(InputIt first, InputIt last, const Alloc& al) {
    basic_value<CharT, Alloc> v(al);
    typename basic_value<CharT, Alloc>::record_t::alloc_type rec_al(v);
    v.value_.rec = basic_value<CharT, Alloc>::record_t::create(rec_al, first, last);
    v.type_ = dtype::record;
    return v;
}

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc> make_record(
    std::initializer_list<std::pair<std::basic_string_view<CharT>, basic_value<CharT, Alloc>>> init, const Alloc& al) {
    basic_value<CharT, Alloc> v(al);
    typename basic_value<CharT, Alloc>::record_t::alloc_type rec_al(v);
    v.value_.rec = basic_value<CharT, Alloc>::record_t::create(rec_al, init);
    v.type_ = dtype::record;
    return v;
}

// --------------------------

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc>& basic_value<CharT, Alloc>::operator=(std::basic_string_view<char_type> s) {
    if (type_ != dtype::string) {
        if (type_ != dtype::null) { destroy(); }
        value_.str = alloc_string(s);
        type_ = dtype::string;
    } else {
        assign_string(s);
    }
    return *this;
}

template<typename CharT, typename Alloc>
basic_value<CharT, Alloc>& basic_value<CharT, Alloc>::append_string(std::basic_string_view<char_type> s) {
    if (type_ != dtype::string || !value_.str || value_.str->capacity - value_.str->size < s.size()) {
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
    if (type_ != dtype::array || !value_.arr || value_.arr->size == value_.arr->capacity) { reserve_back(); }
    basic_value& v = *new (&(*value_.arr)[value_.arr->size]) basic_value(std::forward<Args>(args)...);
    ++value_.arr->size;
    return v;
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::pop_back() {
    if (type_ != dtype::array) { throw database_error("not an array"); }
    assert(value_.arr && value_.arr->size);
    (*value_.arr)[--value_.arr->size].~basic_value();
}

template<typename CharT, typename Alloc>
template<typename... Args>
basic_value<CharT, Alloc>* basic_value<CharT, Alloc>::emplace(std::size_t pos, Args&&... args) {
    if (type_ != dtype::array || !value_.arr || value_.arr->size == value_.arr->capacity) { reserve_back(); }
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
    if (type_ != dtype::record) {
        if (type_ != dtype::null) { throw database_error("not a record"); }
        value_.rec = record_t::create(rec_al);
        type_ = dtype::record;
    }
    detail::list_links_type* node = value_.rec->new_node(rec_al, name, std::forward<Args>(args)...);
    value_.rec = record_t::insert(rec_al, value_.rec, typename record_t::hasher{}(name), node);
    return record_iterator(node);
}

template<typename CharT, typename Alloc>
template<typename... Args>
auto basic_value<CharT, Alloc>::emplace_unique(std::basic_string_view<char_type> name, Args&&... args)
    -> std::pair<record_iterator, bool> {
    typename record_t::alloc_type rec_al(*this);
    if (type_ != dtype::record) {
        if (type_ != dtype::null) { throw database_error("not a record"); }
        value_.rec = record_t::create(rec_al);
        type_ = dtype::record;
    }
    const std::size_t hash_code = typename record_t::hasher{}(name);
    detail::list_links_type* node = value_.rec->find(name, hash_code);
    if (node == &value_.rec->head) {
        node = value_.rec->new_node(rec_al, name, std::forward<Args>(args)...);
        value_.rec = record_t::insert(rec_al, value_.rec, hash_code, node);
        return std::make_pair(record_iterator(node), true);
    }
    return std::make_pair(record_iterator(node), false);
}

template<typename CharT, typename Alloc>
template<typename InputIt, typename>
void basic_value<CharT, Alloc>::insert(std::size_t pos, InputIt first, InputIt last) {
    if (type_ != dtype::array) {
        if (type_ != dtype::null) { throw database_error("not an array"); }
        value_.arr = alloc_array(first, last, is_random_access_iterator<InputIt>());
        type_ = dtype::array;
    } else if (first != last) {
        std::size_t count = append_array(first, last, is_random_access_iterator<InputIt>());
        if (pos < value_.arr->size - count) {
            std::rotate(&(*value_.arr)[pos], &(*value_.arr)[value_.arr->size - count], &(*value_.arr)[value_.arr->size]);
        }
    }
}

template<typename CharT, typename Alloc>
template<typename InputIt, typename, typename>
void basic_value<CharT, Alloc>::insert(InputIt first, InputIt last) {
    typename record_t::alloc_type rec_al(*this);
    if (type_ != dtype::record) {
        if (type_ != dtype::null) { throw database_error("not a record"); }
        value_.rec = record_t::create(rec_al, first, last);
        type_ = dtype::record;
    } else {
        for (; first != last; ++first) {
            detail::list_links_type* node = value_.rec->new_node(rec_al, first->first, first->second);
            value_.rec = record_t::insert(rec_al, value_.rec, typename record_t::hasher{}(first->first), node);
        }
    }
}

template<typename CharT, typename Alloc>
uxs::span<const basic_value<CharT, Alloc>> basic_value<CharT, Alloc>::as_array() const noexcept {
    if (type_ == dtype::array) { return array_view(); }
    return type_ != dtype::null ? uxs::as_span(this, 1) : uxs::span<basic_value>();
}

template<typename CharT, typename Alloc>
uxs::span<basic_value<CharT, Alloc>> basic_value<CharT, Alloc>::as_array() noexcept {
    if (type_ == dtype::array) { return array_view(); }
    return type_ != dtype::null ? uxs::as_span(this, 1) : uxs::span<basic_value>();
}

template<typename CharT, typename Alloc>
iterator_range<typename basic_value<CharT, Alloc>::const_record_iterator> basic_value<CharT, Alloc>::as_record() const {
    if (type_ != dtype::record) { throw database_error("not a record"); }
    return uxs::make_range(const_record_iterator(value_.rec->head.next), const_record_iterator(&value_.rec->head));
}

template<typename CharT, typename Alloc>
iterator_range<typename basic_value<CharT, Alloc>::record_iterator> basic_value<CharT, Alloc>::as_record() {
    if (type_ != dtype::record) { throw database_error("not a record"); }
    return uxs::make_range(record_iterator(value_.rec->head.next), record_iterator(&value_.rec->head));
}

template<typename CharT, typename Alloc>
typename basic_value<CharT, Alloc>::const_record_iterator basic_value<CharT, Alloc>::find(
    std::basic_string_view<char_type> name) const {
    if (type_ != dtype::record) { throw database_error("not a record"); }
    return const_record_iterator(value_.rec->find(name, typename record_t::hasher{}(name)));
}

template<typename CharT, typename Alloc>
typename basic_value<CharT, Alloc>::record_iterator basic_value<CharT, Alloc>::find(
    std::basic_string_view<char_type> name) {
    if (type_ != dtype::record) { throw database_error("not a record"); }
    return record_iterator(value_.rec->find(name, typename record_t::hasher{}(name)));
}

template<typename CharT, typename Alloc>
typename basic_value<CharT, Alloc>::const_record_iterator basic_value<CharT, Alloc>::nil() const noexcept {
    return const_record_iterator(type_ == dtype::record ? &value_.rec->head : nullptr);
}

template<typename CharT, typename Alloc>
typename basic_value<CharT, Alloc>::record_iterator basic_value<CharT, Alloc>::nil() noexcept {
    return record_iterator(type_ == dtype::record ? &value_.rec->head : nullptr);
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::contains(std::basic_string_view<char_type> name) const {
    return find(name) != nil();
}

template<typename CharT, typename Alloc>
std::size_t basic_value<CharT, Alloc>::count(std::basic_string_view<char_type> name) const {
    if (type_ != dtype::record) { throw database_error("not a record"); }
    return value_.rec->count(name);
}

// --------------------------

template<typename CharT, typename Alloc>
template<typename RandIt>
auto basic_value<CharT, Alloc>::alloc_array(std::size_t sz, RandIt src) -> value_flexarray_t* {
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
    value_flexarray_t* arr = value_flexarray_t::alloc(arr_al, value_flexarray_t::start_capacity);
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
void basic_value<CharT, Alloc>::assign_array(std::size_t sz, RandIt src) {
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
void basic_value<CharT, Alloc>::append_array(std::size_t sz, RandIt src) {
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
std::size_t basic_value<CharT, Alloc>::append_array(InputIt first, InputIt last,
                                                    std::false_type /* random access iterator */) {
    if (first == last) { return 0; }
    if (value_.arr) {
        typename value_flexarray_t::alloc_type arr_al(*this);
        std::size_t old_sz = value_.arr->size;
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
    if (type_ != dtype::record) {
        if (type_ != dtype::null) { destroy(); }
        value_.rec = record_t::create(rec_al, first, last);
        type_ = dtype::record;
    } else {
        value_.rec->clear(rec_al);
        for (; first != last; ++first) {
            detail::list_links_type* node = value_.rec->new_node(rec_al, first->first, first->second);
            value_.rec = record_t::insert(rec_al, value_.rec, typename record_t::hasher{}(first->first), node);
        }
    }
}

template<typename CharT, typename Alloc>
template<typename InputIt>
void basic_value<CharT, Alloc>::assign_impl(InputIt first, InputIt last, std::false_type /* range of pairs */) {
    if (type_ != dtype::array) {
        if (type_ != dtype::null) { destroy(); }
        value_.arr = alloc_array(first, last, is_random_access_iterator<InputIt>());
        type_ = dtype::array;
    } else {
        assign_array(first, last, is_random_access_iterator<InputIt>());
    }
}

namespace detail {

template<typename, typename, typename>
struct value_getters_specializer;

template<typename CharT, typename Alloc>
template<typename InputIt>
/*static*/ record_t<CharT, Alloc>* record_t<CharT, Alloc>::create(alloc_type& rec_al, InputIt first, InputIt last) {
    record_t* rec = create(rec_al);
    try {
        for (; first != last; ++first) {
            list_links_type* node = rec->new_node(rec_al, first->first, first->second);
            rec = insert(rec_al, rec, hasher{}(first->first), node);
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
list_links_type* record_t<CharT, Alloc>::new_node(alloc_type& rec_al, std::basic_string_view<CharT> name,
                                                  Args&&... args) {
    typename node_t::alloc_type node_al(rec_al);
    node_t* node = node_t::alloc_checked(node_al, name);
    try {
        new (&node->v.val()) basic_value<CharT, Alloc>(std::forward<Args>(args)...);
        return &node->links;
    } catch (...) {
        node_t::dealloc(node_al, node);
        throw;
    }
}

template<typename CharT, typename Alloc>
/*static*/ void record_t<CharT, Alloc>::delete_node(alloc_type& rec_al, list_links_type* links) {
    node_t* node = node_t::from_links(links);
    node->v.val().~basic_value<CharT, Alloc>();
    typename node_t::alloc_type node_al(rec_al);
    node_t::dealloc(node_al, node);
}

}  // namespace detail

#define UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(ty, func) \
    template<typename CharT, typename Alloc> \
    ty basic_value<CharT, Alloc>::as##func() const { \
        auto result = this->get##func(); \
        if (result) { return *result; } \
        throw database_error("bad value conversion"); \
    } \
    namespace detail { \
    template<typename CharT, typename Alloc> \
    struct value_getters_specializer<CharT, Alloc, ty> { \
        static bool is(const basic_value<CharT, Alloc>& v) { return v.is##func(); } \
        static ty as(const basic_value<CharT, Alloc>& v) { return v.as##func(); } \
        static uxs::optional<ty> get(const basic_value<CharT, Alloc>& v) { return v.get##func(); } \
    }; \
    }
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(bool, _bool)
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(std::int32_t, _int)
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(std::uint32_t, _uint)
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(std::int64_t, _int64)
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(std::uint64_t, _uint64)
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(float, _float)
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(double, _double)
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(std::basic_string<CharT>, _string)
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(std::basic_string_view<CharT>, _string_view)
#undef UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS

template<typename CharT, typename Alloc>
template<typename Ty>
bool basic_value<CharT, Alloc>::is() const noexcept {
    return detail::value_getters_specializer<CharT, Alloc, Ty>::is(*this);
}

template<typename CharT, typename Alloc>
template<typename Ty>
Ty basic_value<CharT, Alloc>::as() const {
    return detail::value_getters_specializer<CharT, Alloc, Ty>::as(*this);
}

template<typename CharT, typename Alloc>
template<typename Ty>
uxs::optional<Ty> basic_value<CharT, Alloc>::get() const {
    return detail::value_getters_specializer<CharT, Alloc, Ty>::get(*this);
}

template<typename CharT, typename Alloc>
bool operator!=(const basic_value<CharT, Alloc>& lhs, const basic_value<CharT, Alloc>& rhs) noexcept {
    return !(lhs == rhs);
}

using value = basic_value<char>;

}  // namespace db
}  // namespace uxs

namespace std {
template<typename CharT, typename Alloc>
void swap(uxs::db::basic_value<CharT, Alloc>& v1, uxs::db::basic_value<CharT, Alloc>& v2) noexcept {
    v1.swap(v2);
}
}  // namespace std
