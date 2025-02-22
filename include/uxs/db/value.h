#pragma once

#include "database_error.h"

#include "uxs/dllist.h"  // NOLINT
#include "uxs/iterator.h"
#include "uxs/memory.h"
#include "uxs/optional.h"
#include "uxs/span.h"
#include "uxs/string_view.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <tuple>

namespace uxs {
namespace db {

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

template<typename Ty, typename Alloc>
struct flexarray_t {
    std::size_t size;
    std::size_t capacity;
    alignas(std::alignment_of<Ty>::value) std::uint8_t x[sizeof(Ty)];

    enum : unsigned { start_capacity = 8 };

    flexarray_t(const flexarray_t&) = delete;
    flexarray_t& operator=(const flexarray_t&) = delete;

    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<flexarray_t>;

    est::span<const Ty> view() const { return est::as_span(reinterpret_cast<const Ty*>(&x), size); }
    est::span<Ty> view() { return est::as_span(reinterpret_cast<Ty*>(&x), size); }
    const Ty& operator[](std::size_t i) const { return reinterpret_cast<const Ty*>(&x)[i]; }
    Ty& operator[](std::size_t i) { return reinterpret_cast<Ty*>(&x)[i]; }

    void clear() {
        destroy();
        size = 0;
    }

    static std::size_t max_size(const alloc_type& arr_al) {
        return (std::allocator_traits<alloc_type>::max_size(arr_al) * sizeof(flexarray_t) - offsetof(flexarray_t, x)) /
               sizeof(Ty);
    }

    static std::size_t get_alloc_sz(std::size_t cap) {
        return (offsetof(flexarray_t, x) + cap * sizeof(Ty) + sizeof(flexarray_t) - 1) / sizeof(flexarray_t);
    }

    UXS_EXPORT static flexarray_t* alloc(alloc_type& arr_al, std::size_t cap);
    UXS_EXPORT static flexarray_t* grow(alloc_type& arr_al, flexarray_t* arr, std::size_t extra);

    void destroy(std::size_t start_from = 0) {
        std::for_each(&(*this)[start_from], &(*this)[size], [](Ty& v) { v.~Ty(); });
    }

    static void dealloc(alloc_type& arr_al, flexarray_t* arr) { arr_al.deallocate(arr, get_alloc_sz(arr->capacity)); }

    static flexarray_t* alloc_checked(alloc_type& arr_al, std::size_t cap) {
        if (cap > max_size(arr_al)) { throw std::length_error("too much to reserve"); }
        return alloc(arr_al, cap);
    }
};

#if UXS_ITERATOR_DEBUG_LEVEL != 0
struct list_links_t {
    list_links_t* next;
    list_links_t* prev;
    list_links_t* head;
};
#else   // UXS_ITERATOR_DEBUG_LEVEL != 0
using list_links_t = dllist_node_t;
#endif  // UXS_ITERATOR_DEBUG_LEVEL != 0

template<typename CharT, typename Alloc>
struct record_t;

template<typename CharT, typename Alloc>
class record_value {
 private:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<record_value>;

 public:
    using char_type = CharT;
    using value_type = basic_value<CharT, Alloc>;

    std::basic_string_view<char_type> key() const { return std::basic_string_view<char_type>(key_chars_, key_sz_); }
    const value_type& value() const { return *reinterpret_cast<const value_type*>(&x_); }
    value_type& value() { return *reinterpret_cast<value_type*>(&x_); }

    friend bool operator==(const record_value& lhs, const record_value& rhs) noexcept {
        return lhs.key() == rhs.key() && lhs.value() == rhs.value();
    }
    friend bool operator!=(const record_value& lhs, const record_value& rhs) noexcept { return !(lhs == rhs); }

    static record_value* from_links(list_links_t* links) {
        return get_containing_record<record_value, offsetof(record_value, links_)>(links);
    }

 private:
    friend struct record_t<CharT, Alloc>;

    list_links_t links_;
    list_links_t* next_bucket_;
    std::size_t hash_code_;
    alignas(std::alignment_of<value_type>::value) std::uint8_t x_[sizeof(value_type)];
    std::size_t key_sz_;
    char_type key_chars_[1];

    static std::size_t max_name_size(const alloc_type& node_al) {
        return (std::allocator_traits<alloc_type>::max_size(node_al) * sizeof(record_value) -
                offsetof(record_value, key_chars_)) /
               sizeof(CharT);
    }

    static std::size_t get_alloc_sz(std::size_t key_sz) {
        return (offsetof(record_value, key_chars_) + key_sz * sizeof(CharT) + sizeof(record_value) - 1) /
               sizeof(record_value);
    }

    UXS_EXPORT static record_value* alloc_checked(alloc_type& node_al, std::basic_string_view<CharT> key);

    static void dealloc(alloc_type& node_al, record_value* node) {
        node_al.deallocate(node, get_alloc_sz(node->key_sz_));
    }
};

template<typename CharT, typename Alloc>
struct record_node_traits {
    using iterator_node_t = list_links_t;
    using node_t = record_value<CharT, Alloc>;
    static list_links_t* get_next(list_links_t* node) { return node->next; }
    static list_links_t* get_prev(list_links_t* node) { return node->prev; }
#if UXS_ITERATOR_DEBUG_LEVEL != 0
    static void set_head(list_links_t* node, list_links_t* head) { node->head = head; }
    static list_links_t* get_head(list_links_t* node) { return node->head; }
    static list_links_t* get_front(list_links_t* head) { return head->next; }
#else   // UXS_ITERATOR_DEBUG_LEVEL != 0
    static void set_head(list_links_t* node, list_links_t* head) {}
#endif  // UXS_ITERATOR_DEBUG_LEVEL != 0
    static record_value<CharT, Alloc>& get_value(list_links_t* node) { return *node_t::from_links(node); }
};

template<typename CharT, typename Alloc>
struct record_t {
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<record_t>;
    using node_traits = record_node_traits<CharT, Alloc>;
    using node_t = typename node_traits::node_t;
    using hasher = std::hash<std::basic_string_view<CharT>>;

    using value_type = record_value<CharT, Alloc>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using reference = value_type&;
    using const_reference = const value_type&;
    using iterator = list_iterator<record_t, node_traits, false>;
    using const_iterator = list_iterator<record_t, node_traits, true>;

    mutable list_links_t head;
    std::size_t size;
    std::size_t bucket_count;
    list_links_t* hashtbl[1];

    enum : unsigned { min_bucket_count_inc = 12 };

    record_t(const record_t&) = delete;
    record_t& operator=(const record_t&) = delete;

    UXS_EXPORT void init();
    UXS_EXPORT void destroy(alloc_type& rec_al, list_links_t* node);

    iterator begin() const { return iterator(head.next); }
    iterator end() const { return iterator(&head); }

    const_iterator cbegin() const { return const_iterator(head.next); }
    const_iterator cend() const { return const_iterator(&head); }

    UXS_EXPORT list_links_t* find(std::basic_string_view<CharT> key, std::size_t hash_code) const;
    UXS_EXPORT std::size_t count(std::basic_string_view<CharT> key) const;

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
    list_links_t* new_node(alloc_type& rec_al, std::basic_string_view<CharT> key, Args&&... args);
    static void delete_node(alloc_type& rec_al, list_links_t* node);
    void add_to_hash(list_links_t* node, std::size_t hash_code);
    UXS_EXPORT static record_t* insert(alloc_type& rec_al, record_t* rec, std::size_t hash_code, list_links_t* node);
    list_links_t* erase(alloc_type& rec_al, list_links_t* node);
    std::size_t erase(alloc_type& rec_al, std::basic_string_view<CharT> key);

    static std::size_t max_size(const alloc_type& rec_al) {
        return (std::allocator_traits<alloc_type>::max_size(rec_al) * sizeof(record_t) - offsetof(record_t, hashtbl)) /
               sizeof(list_links_t*);
    }

    static std::size_t get_alloc_sz(std::size_t bckt_cnt) {
        return (offsetof(record_t, hashtbl) + bckt_cnt * sizeof(list_links_t*) + sizeof(record_t) - 1) /
               sizeof(record_t);
    }

    static std::size_t next_bucket_count(const alloc_type& rec_al, std::size_t sz);
    UXS_EXPORT static record_t* alloc(alloc_type& rec_al, std::size_t bckt_cnt);
    static record_t* rehash(alloc_type& rec_al, record_t* rec, std::size_t bckt_cnt);
    UXS_EXPORT static void dealloc(alloc_type& rec_al, record_t* rec) {
        rec_al.deallocate(rec, get_alloc_sz(rec->bucket_count));
    }
};

template<typename CharT, typename Alloc, bool Const>
class value_iterator : public container_iterator_facade<basic_value<CharT, Alloc>, value_iterator<CharT, Alloc, Const>,
                                                        std::bidirectional_iterator_tag, Const> {
 public:
    using value_type = basic_value<CharT, Alloc>;

    template<typename, typename, bool>
    friend class value_iterator;

    value_iterator() noexcept = default;
#if UXS_ITERATOR_DEBUG_LEVEL != 0
    explicit value_iterator(value_type* ptr, value_type* begin, value_type* end) noexcept
        : ptr_(ptr), begin_(begin), end_(end) {}
    template<bool Const_ = Const>
    value_iterator(const std::enable_if_t<Const_, value_iterator<CharT, Alloc, false>>& it) noexcept
        : is_record_(it.is_record_), ptr_(it.ptr_), begin_(it.begin_), end_(it.end_) {}
    template<bool Const_ = Const>
    value_iterator& operator=(const std::enable_if_t<Const_, value_iterator<CharT, Alloc, false>>& it) noexcept {
        is_record_ = it.is_record_, ptr_ = it.ptr_, begin_ = it.begin_, end_ = it.end_;
        return *this;
    }
#else   // UXS_ITERATOR_DEBUG_LEVEL != 0
    explicit value_iterator(value_type* ptr, value_type* begin, value_type* end) noexcept
        : is_record_(false), ptr_(ptr) {
        (void)begin, (void)end;
    }
    template<bool Const_ = Const>
    value_iterator(const std::enable_if_t<Const_, value_iterator<CharT, Alloc, false>>& it) noexcept
        : is_record_(it.is_record_), ptr_(it.ptr_) {}
    template<bool Const_ = Const>
    value_iterator& operator=(const std::enable_if_t<Const_, value_iterator<CharT, Alloc, false>>& it) noexcept {
        is_record_ = it.is_record_, ptr_ = it.ptr_;
        return *this;
    }
#endif  // UXS_ITERATOR_DEBUG_LEVEL != 0
    explicit value_iterator(list_links_t* node) noexcept : is_record_(true), ptr_(node) {}

    void increment() noexcept {
        uxs_iterator_assert(is_record_ || (begin_ <= ptr_ && ptr_ < end_));
        uxs_iterator_assert(!is_record_ || (ptr_ && ptr_ != head()));
        ptr_ = is_record_ ? static_cast<void*>(static_cast<list_links_t*>(ptr_)->next) :
                            static_cast<void*>(static_cast<value_type*>(ptr_) + 1);
    }

    void decrement() noexcept {
        uxs_iterator_assert(is_record_ || (begin_ < ptr_ && ptr_ <= end_));
        uxs_iterator_assert(!is_record_ || (ptr_ && ptr_ != head()->next));
        ptr_ = is_record_ ? static_cast<void*>(static_cast<list_links_t*>(ptr_)->prev) :
                            static_cast<void*>(static_cast<value_type*>(ptr_) - 1);
    }

    template<bool Const2>
    bool is_equal_to(const value_iterator<CharT, Alloc, Const2>& it) const noexcept {
        uxs_iterator_assert(is_record_ == it.is_record_);
        uxs_iterator_assert(is_record_ || (begin_ == it.begin_ && end_ == it.end_));
        uxs_iterator_assert(!is_record_ || (!ptr_ && !it.ptr_) || (ptr_ && it.ptr_ && head() == it.head()));
        return ptr_ == it.ptr_;
    }

    value_iterator dereference() const noexcept { return *this; }

    bool is_record() const noexcept { return is_record_; }

    std::basic_string_view<CharT> key() const {
        if (!is_record_) { throw database_error("cannot use key() for non-record iterators"); }
        return record_value<CharT, Alloc>::from_links(static_cast<list_links_t*>(ptr_))->key();
    }

    std::conditional_t<Const, const value_type&, value_type&> value() const noexcept {
        uxs_iterator_assert(is_record_ || (begin_ <= ptr_ && ptr_ < end_));
        uxs_iterator_assert(!is_record_ || (ptr_ && ptr_ != head()));
        return is_record_ ? record_value<CharT, Alloc>::from_links(static_cast<list_links_t*>(ptr_))->value() :
                            *static_cast<value_type*>(ptr_);
    }

 private:
    friend class basic_value<CharT, Alloc>;

    bool is_record_ = false;
    void* ptr_ = nullptr;
#if UXS_ITERATOR_DEBUG_LEVEL != 0
    void* begin_ = nullptr;
    void* end_ = nullptr;
    list_links_t* head() const noexcept { return static_cast<list_links_t*>(ptr_)->head; }
#endif  // UXS_ITERATOR_DEBUG_LEVEL != 0
};

template<typename Iter>
class value_reverse_iterator : public std::reverse_iterator<Iter> {
 public:
    explicit value_reverse_iterator(const Iter& it) noexcept : std::reverse_iterator<Iter>(it) {}
    bool is_record() const noexcept { return (**this).is_record(); }
    auto key() const -> decltype((**this).key()) { return (**this).key(); }
    auto value() const noexcept -> decltype((**this).value()) { return (**this).value(); }
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
 private:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<CharT>;
    using char_flexarray_t = detail::flexarray_t<CharT, Alloc>;
    using value_flexarray_t = detail::flexarray_t<basic_value, Alloc>;
    using record_t = detail::record_t<CharT, Alloc>;

 public:
    using char_type = CharT;
    using value_type = basic_value<CharT, Alloc>;
    using allocator_type = Alloc;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using iterator = detail::value_iterator<CharT, Alloc, false>;
    using const_iterator = detail::value_iterator<CharT, Alloc, true>;
    using reverse_iterator = detail::value_reverse_iterator<iterator>;
    using const_reverse_iterator = detail::value_reverse_iterator<const_iterator>;
    using pointer = void;
    using const_pointer = void;
    using reference = iterator;
    using const_reference = const_iterator;

    basic_value() noexcept(std::is_nothrow_default_constructible<alloc_type>::value)
        : alloc_type(), type_(dtype::null) {}
    basic_value(std::nullptr_t) noexcept(std::is_nothrow_default_constructible<alloc_type>::value)
        : alloc_type(), type_(dtype::null) {}
    basic_value(std::basic_string_view<char_type> s) : alloc_type(), type_(dtype::string) {
        value_.str = alloc_string(s);
    }
    basic_value(const char_type* cstr) : alloc_type(), type_(dtype::string) {
        value_.str = alloc_string(std::basic_string_view<char_type>(cstr));
    }

    explicit basic_value(const Alloc& al) noexcept : alloc_type(al), type_(dtype::null) {}
    basic_value(std::nullptr_t, const Alloc& al) noexcept : alloc_type(al), type_(dtype::null) {}
    basic_value(std::basic_string_view<char_type> s, const Alloc& al) : alloc_type(al), type_(dtype::string) {
        value_.str = alloc_string(s);
    }
    basic_value(const char_type* cstr, const Alloc& al) : alloc_type(al), type_(dtype::string) {
        value_.str = alloc_string(std::basic_string_view<char_type>(cstr));
    }

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

#define UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(ty, id, field) \
    basic_value(ty v) : alloc_type(), type_(id) { value_.field = static_cast<decltype(value_.field)>(v); } \
    basic_value(ty v, const Alloc& al) : alloc_type(al), type_(id) { \
        value_.field = static_cast<decltype(value_.field)>(v); \
    } \
    basic_value& operator=(ty v) { \
        if (type_ != dtype::null) { destroy(); } \
        type_ = id, value_.field = static_cast<decltype(value_.field)>(v); \
        return *this; \
    }
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(bool, dtype::boolean, b)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(signed, dtype::integer, i)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(unsigned, dtype::unsigned_integer, u)
#if ULONG_MAX > 0xffffffff
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(signed long, dtype::long_integer, i64)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(unsigned long, dtype::unsigned_long_integer, u64)
#else   // ULONG_MAX > 0xffffffff
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(signed long, dtype::integer, i)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(unsigned long, dtype::unsigned_integer, u)
#endif  // ULONG_MAX > 0xffffffff
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(signed long long, dtype::long_integer, i64)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(unsigned long long, dtype::unsigned_long_integer, u64)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(float, dtype::double_precision, dbl)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(double, dtype::double_precision, dbl)
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(long double, dtype::double_precision, dbl)
#undef UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT

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

    basic_value& append_string(std::basic_string_view<char_type> s);
    basic_value& append_string(const char_type* cstr) { return append_string(std::basic_string_view<char_type>(cstr)); }

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
    est::optional<Ty> get() const;

    template<typename Ty, typename U>
    Ty value_or(U&& default_value) const {
        auto result = get<Ty>();
        return result ? *result : Ty(std::forward<U>(default_value));
    }

    template<typename Ty, typename U>
    Ty value_or(std::basic_string_view<char_type> key, U&& default_value) const {
        auto it = find(key);
        if (it != end()) {
            auto result = it.value().template get<Ty>();
            if (result) { return *result; }
        }
        return Ty(std::forward<U>(default_value));
    }

    template<typename Ty>
    Ty value() const {
        return value_or<Ty>(Ty());
    }

    template<typename Ty>
    Ty value(std::basic_string_view<char_type> key) const {
        return value_or<Ty>(key, Ty());
    }

    bool is_null() const noexcept { return type_ == dtype::null; }
    bool is_bool() const noexcept { return type_ == dtype::boolean; }
    UXS_EXPORT bool is_int() const noexcept;
    UXS_EXPORT bool is_uint() const noexcept;
    UXS_EXPORT bool is_int64() const noexcept;
    UXS_EXPORT bool is_uint64() const noexcept;
    UXS_EXPORT bool is_integral() const noexcept;
    bool is_double() const noexcept { return is_numeric(); }
    bool is_numeric() const noexcept { return type_ >= dtype::integer && type_ <= dtype::double_precision; }
    bool is_string() const noexcept { return type_ == dtype::string; }
    bool is_string_view() const noexcept { return type_ == dtype::string; }
    bool is_array() const noexcept { return type_ == dtype::array; }
    bool is_record() const noexcept { return type_ == dtype::record; }

    bool as_bool() const;
    std::int32_t as_int() const;
    std::uint32_t as_uint() const;
    std::int64_t as_int64() const;
    std::uint64_t as_uint64() const;
    double as_double() const;
    std::basic_string<char_type> as_string() const;
    std::basic_string_view<char_type> as_string_view() const;

    UXS_EXPORT est::optional<bool> get_bool() const;
    UXS_EXPORT est::optional<std::int32_t> get_int() const;
    UXS_EXPORT est::optional<std::uint32_t> get_uint() const;
    UXS_EXPORT est::optional<std::int64_t> get_int64() const;
    UXS_EXPORT est::optional<std::uint64_t> get_uint64() const;
    UXS_EXPORT est::optional<double> get_double() const;
    UXS_EXPORT est::optional<std::basic_string<char_type>> get_string() const;
    est::optional<std::basic_string_view<char_type>> get_string_view() const {
        return type_ == dtype::string ? est::make_optional(str_view()) : est::nullopt();
    }

    bool empty() const noexcept { return size() == 0; }
    UXS_EXPORT std::size_t size() const noexcept;

    iterator begin() noexcept {
        if (type_ == dtype::record) { return iterator(value_.rec->head.next); }
        auto range = as_array();
        return iterator(range.data(), range.data(), range.data() + range.size());
    }
    const_iterator begin() const noexcept {
        if (type_ == dtype::record) { return const_iterator(value_.rec->head.next); }
        auto range = const_cast<basic_value&>(*this).as_array();
        return const_iterator(range.data(), range.data(), range.data() + range.size());
    }
    const_iterator cbegin() const noexcept { return begin(); }

    iterator end() noexcept {
        if (type_ == dtype::record) { return iterator(&value_.rec->head); }
        auto range = as_array();
        return iterator(range.data() + range.size(), range.data(), range.data() + range.size());
    }
    const_iterator end() const noexcept {
        if (type_ == dtype::record) { return const_iterator(&value_.rec->head); }
        auto range = const_cast<basic_value&>(*this).as_array();
        return const_iterator(range.data() + range.size(), range.data(), range.data() + range.size());
    }
    const_iterator cend() const noexcept { return end(); }

    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }

    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return rend(); }

    reference front() { return begin(); }
    const_reference front() const { return begin(); }

    reference back() { return std::prev(end()); }
    const_reference back() const { return std::prev(end()); }

    est::span<const basic_value> as_array() const noexcept;
    est::span<basic_value> as_array() noexcept;

    iterator_range<typename record_t::const_iterator> as_record() const;
    iterator_range<typename record_t::iterator> as_record();

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

    const basic_value& at(std::basic_string_view<char_type> key) const {
        auto it = find(key);
        if (it != end()) { return it.value(); }
        throw database_error("invalid key");
    }

    basic_value& at(std::basic_string_view<char_type> key) {
        auto it = find(key);
        if (it != end()) { return it.value(); }
        throw database_error("invalid key");
    }

    UXS_EXPORT basic_value& operator[](std::basic_string_view<char_type> key);

    template<typename Func>
    auto visit(const Func& func) const -> decltype(func(nullptr)) {
        switch (type_) {
            case dtype::null: return func(nullptr);
            case dtype::boolean: return func(value_.b);
            case dtype::integer: return func(value_.i);
            case dtype::unsigned_integer: return func(value_.u);
            case dtype::long_integer: return func(value_.i64);
            case dtype::unsigned_long_integer: return func(value_.u64);
            case dtype::double_precision: return func(value_.dbl);
            case dtype::string: return func(str_view());
            case dtype::array:
            case dtype::record: return func(make_range(begin(), end()));
            default: UXS_UNREACHABLE_CODE;
        }
    }

    const_iterator find(std::basic_string_view<char_type> key) const;
    iterator find(std::basic_string_view<char_type> key);
    bool contains(std::basic_string_view<char_type> key) const;
    std::size_t count(std::basic_string_view<char_type> key) const;

    UXS_EXPORT void clear() noexcept;
    UXS_EXPORT void reserve(std::size_t sz);
    UXS_EXPORT void resize(std::size_t sz);

    template<typename... Args>
    basic_value& emplace_back(Args&&... args);
    void push_back(const basic_value& v) { emplace_back(v); }
    void push_back(basic_value&& v) { emplace_back(std::move(v)); }
    void pop_back();

    template<typename... Args>
    iterator emplace(std::size_t pos, Args&&... args);
    iterator insert(std::size_t pos, const basic_value& v) { return emplace(pos, v); }
    iterator insert(std::size_t pos, basic_value&& v) { return emplace(pos, std::move(v)); }

    template<typename... Args>
    iterator emplace(std::basic_string_view<char_type> key, Args&&... args);
    iterator insert(std::basic_string_view<char_type> key, const basic_value& v) { return emplace(key, v); }
    iterator insert(std::basic_string_view<char_type> key, basic_value&& v) { return emplace(key, std::move(v)); }

    template<typename... Args>
    std::pair<iterator, bool> emplace_unique(std::basic_string_view<char_type> key, Args&&... args);
    std::pair<iterator, bool> insert_unique(std::basic_string_view<char_type> key, const basic_value& v) {
        return emplace_unique(key, v);
    }
    std::pair<iterator, bool> insert_unique(std::basic_string_view<char_type> key, basic_value&& v) {
        return emplace_unique(key, std::move(v));
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
    UXS_EXPORT iterator erase(const_iterator it);
    UXS_EXPORT std::size_t erase(std::basic_string_view<char_type> key);

 private:
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
    est::span<const basic_value> array_view() const {
        return value_.arr ? value_.arr->view() : est::span<basic_value>();
    }
    est::span<basic_value> array_view() { return value_.arr ? value_.arr->view() : est::span<basic_value>(); }

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
    value_flexarray_t* alloc_array(est::span<const basic_value> v) { return alloc_array(v.size(), v.data()); }

    template<typename RandIt>
    void assign_array(std::size_t sz, RandIt src);
    template<typename RandIt>
    void assign_array(RandIt first, RandIt last, std::true_type /* random access iterator */) {
        assign_array(static_cast<std::size_t>(last - first), first);
    }
    template<typename InputIt>
    void assign_array(InputIt first, InputIt last, std::false_type /* random access iterator */);
    void assign_array(est::span<const basic_value> v) { assign_array(v.size(), v.data()); }

    template<typename RandIt>
    void append_array(std::size_t sz, RandIt src);
    template<typename RandIt>
    std::size_t append_array(RandIt first, RandIt last, std::true_type /* random access iterator */) {
        const std::size_t count = static_cast<std::size_t>(last - first);
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
auto basic_value<CharT, Alloc>::emplace(std::size_t pos, Args&&... args) -> iterator {
    if (type_ != dtype::array || !value_.arr || value_.arr->size == value_.arr->capacity) { reserve_back(); }
    new (&(*value_.arr)[value_.arr->size]) basic_value(std::forward<Args>(args)...);
    ++value_.arr->size;
    if (pos < value_.arr->size - 1) {
        rotate_back(pos);
    } else {
        pos = value_.arr->size - 1;
    }
    return iterator(&(*value_.arr)[pos], &(*value_.arr)[0], &(*value_.arr)[value_.arr->size]);
}

template<typename CharT, typename Alloc>
template<typename... Args>
auto basic_value<CharT, Alloc>::emplace(std::basic_string_view<char_type> key, Args&&... args) -> iterator {
    typename record_t::alloc_type rec_al(*this);
    if (type_ != dtype::record) {
        if (type_ != dtype::null) { throw database_error("not a record"); }
        value_.rec = record_t::create(rec_al);
        type_ = dtype::record;
    }
    detail::list_links_t* node = value_.rec->new_node(rec_al, key, std::forward<Args>(args)...);
    value_.rec = record_t::insert(rec_al, value_.rec, typename record_t::hasher{}(key), node);
    return iterator(node);
}

template<typename CharT, typename Alloc>
template<typename... Args>
auto basic_value<CharT, Alloc>::emplace_unique(std::basic_string_view<char_type> key,
                                               Args&&... args) -> std::pair<iterator, bool> {
    typename record_t::alloc_type rec_al(*this);
    if (type_ != dtype::record) {
        if (type_ != dtype::null) { throw database_error("not a record"); }
        value_.rec = record_t::create(rec_al);
        type_ = dtype::record;
    }
    const std::size_t hash_code = typename record_t::hasher{}(key);
    detail::list_links_t* node = value_.rec->find(key, hash_code);
    if (node == &value_.rec->head) {
        node = value_.rec->new_node(rec_al, key, std::forward<Args>(args)...);
        value_.rec = record_t::insert(rec_al, value_.rec, hash_code, node);
        return std::make_pair(iterator(node), true);
    }
    return std::make_pair(iterator(node), false);
}

template<typename CharT, typename Alloc>
template<typename InputIt, typename>
void basic_value<CharT, Alloc>::insert(std::size_t pos, InputIt first, InputIt last) {
    if (type_ != dtype::array) {
        if (type_ != dtype::null) { throw database_error("not an array"); }
        value_.arr = alloc_array(first, last, is_random_access_iterator<InputIt>());
        type_ = dtype::array;
    } else if (first != last) {
        const std::size_t count = append_array(first, last, is_random_access_iterator<InputIt>());
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
            detail::list_links_t* node = value_.rec->new_node(rec_al, first->first, first->second);
            value_.rec = record_t::insert(rec_al, value_.rec, typename record_t::hasher{}(first->first), node);
        }
    }
}

template<typename CharT, typename Alloc>
est::span<const basic_value<CharT, Alloc>> basic_value<CharT, Alloc>::as_array() const noexcept {
    if (type_ == dtype::array) { return array_view(); }
    return type_ != dtype::null ? est::as_span(this, 1) : est::span<basic_value>();
}

template<typename CharT, typename Alloc>
est::span<basic_value<CharT, Alloc>> basic_value<CharT, Alloc>::as_array() noexcept {
    if (type_ == dtype::array) { return array_view(); }
    return type_ != dtype::null ? est::as_span(this, 1) : est::span<basic_value>();
}

template<typename CharT, typename Alloc>
auto basic_value<CharT, Alloc>::as_record() const -> iterator_range<typename record_t::const_iterator> {
    if (type_ != dtype::record) { throw database_error("not a record"); }
    return make_range(value_.rec->cbegin(), value_.rec->cend());
}

template<typename CharT, typename Alloc>
auto basic_value<CharT, Alloc>::as_record() -> iterator_range<typename record_t::iterator> {
    if (type_ != dtype::record) { throw database_error("not a record"); }
    return make_range(value_.rec->begin(), value_.rec->end());
}

template<typename CharT, typename Alloc>
auto basic_value<CharT, Alloc>::find(std::basic_string_view<char_type> key) const -> const_iterator {
    return type_ == dtype::record ? const_iterator(value_.rec->find(key, typename record_t::hasher{}(key))) : end();
}

template<typename CharT, typename Alloc>
auto basic_value<CharT, Alloc>::find(std::basic_string_view<char_type> key) -> iterator {
    return type_ == dtype::record ? iterator(value_.rec->find(key, typename record_t::hasher{}(key))) : end();
}

template<typename CharT, typename Alloc>
bool basic_value<CharT, Alloc>::contains(std::basic_string_view<char_type> key) const {
    return find(key) != end();
}

template<typename CharT, typename Alloc>
std::size_t basic_value<CharT, Alloc>::count(std::basic_string_view<char_type> key) const {
    return type_ == dtype::record ? value_.rec->count(key) : 0;
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
auto basic_value<CharT, Alloc>::alloc_array(InputIt first, InputIt last,
                                            std::false_type /* random access iterator */) -> value_flexarray_t* {
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
        arr->destroy();
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
            value_.arr->destroy();
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
        basic_value* p0 = &(*value_.arr)[0];
        basic_value* p = p0;
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
        const std::size_t old_sz = value_.arr->size;
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
            detail::list_links_t* node = value_.rec->new_node(rec_al, first->first, first->second);
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

#define UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(ty, func) \
    template<typename CharT, typename Alloc> \
    ty basic_value<CharT, Alloc>::as##func() const { \
        auto result = this->get##func(); \
        if (result) { return *result; } \
        throw database_error("bad value conversion"); \
    }
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(bool, _bool)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(std::int32_t, _int)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(std::uint32_t, _uint)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(std::int64_t, _int64)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(std::uint64_t, _uint64)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(double, _double)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(std::basic_string<CharT>, _string)
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(std::basic_string_view<CharT>, _string_view)
#undef UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC

namespace detail {

template<typename CharT, typename Alloc>
template<typename InputIt>
/*static*/ record_t<CharT, Alloc>* record_t<CharT, Alloc>::create(alloc_type& rec_al, InputIt first, InputIt last) {
    record_t* rec = create(rec_al);
    try {
        for (; first != last; ++first) {
            list_links_t* node = rec->new_node(rec_al, first->first, first->second);
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
list_links_t* record_t<CharT, Alloc>::new_node(alloc_type& rec_al, std::basic_string_view<CharT> key, Args&&... args) {
    typename node_t::alloc_type node_al(rec_al);
    node_t* node = node_t::alloc_checked(node_al, key);
    try {
        new (&node->value()) basic_value<CharT, Alloc>(std::forward<Args>(args)...);
        return &node->links_;
    } catch (...) {
        node_t::dealloc(node_al, node);
        throw;
    }
}

template<typename CharT, typename Alloc>
/*static*/ void record_t<CharT, Alloc>::delete_node(alloc_type& rec_al, list_links_t* links) {
    node_t* node = node_t::from_links(links);
    node->value().~basic_value<CharT, Alloc>();
    typename node_t::alloc_type node_al(rec_al);
    node_t::dealloc(node_al, node);
}

template<typename, typename, typename>
struct value_getters_specializer;

#define UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(ty, func) \
    template<typename CharT, typename Alloc> \
    struct value_getters_specializer<CharT, Alloc, ty> { \
        static bool is(const basic_value<CharT, Alloc>& v) { return v.is##func(); } \
        static ty as(const basic_value<CharT, Alloc>& v) { return static_cast<ty>(v.as##func()); } \
        static est::optional<ty> get(const basic_value<CharT, Alloc>& v) { \
            auto result = v.get##func(); \
            return result ? est::make_optional(static_cast<ty>(*result)) : est::nullopt(); \
        } \
    };
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(bool, _bool)
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(signed, _int)
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(unsigned, _uint)
#if ULONG_MAX > 0xffffffff
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(signed long, _int64)
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(unsigned long, _uint64)
#else   // ULONG_MAX > 0xffffffff
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(signed long, _int)
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(unsigned long, _uint)
#endif  // ULONG_MAX > 0xffffffff
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(signed long long, _int64)
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(unsigned long long, _uint64)
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(float, _double)
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(double, _double)
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(long double, _double)
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(std::basic_string<CharT>, _string)
#undef UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS

}  // namespace detail

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
est::optional<Ty> basic_value<CharT, Alloc>::get() const {
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
template<std::size_t N, typename CharT, typename Alloc, typename = std::enable_if_t<N == 0>>
auto get(const uxs::db::detail::record_value<CharT, Alloc>& v) -> decltype(v.key()) {
    return v.key();
}
template<std::size_t N, typename CharT, typename Alloc, typename = std::enable_if_t<N == 1>>
auto get(const uxs::db::detail::record_value<CharT, Alloc>& v) -> decltype(v.value()) {
    return v.value();
}
template<std::size_t N, typename CharT, typename Alloc, typename = std::enable_if_t<N == 1>>
auto get(uxs::db::detail::record_value<CharT, Alloc>& v) -> decltype(v.value()) {
    return v.value();
}

template<typename CharT, typename Alloc>
class tuple_size<uxs::db::detail::record_value<CharT, Alloc>> : public std::integral_constant<std::size_t, 2> {};

template<std::size_t N, typename CharT, typename Alloc>
class tuple_element<N, uxs::db::detail::record_value<CharT, Alloc>> {
 public:
    using type = decltype(get<N>(std::declval<uxs::db::detail::record_value<CharT, Alloc>>()));
};

template<std::size_t N, typename CharT, typename Alloc, bool Const, typename = std::enable_if_t<N == 0>>
auto get(const uxs::db::detail::value_iterator<CharT, Alloc, Const>& v) -> decltype(v.key()) {
    return v.key();
}
template<std::size_t N, typename CharT, typename Alloc, bool Const, typename = std::enable_if_t<N == 1>>
auto get(const uxs::db::detail::value_iterator<CharT, Alloc, Const>& v) -> decltype(v.value()) {
    return v.value();
}
template<std::size_t N, typename CharT, typename Alloc, bool Const, typename = std::enable_if_t<N == 1>>
auto get(uxs::db::detail::value_iterator<CharT, Alloc, Const>& v) -> decltype(v.value()) {
    return v.value();
}

template<typename CharT, typename Alloc, bool Const>
class tuple_size<uxs::db::detail::value_iterator<CharT, Alloc, Const>> : public std::integral_constant<std::size_t, 2> {
};

template<std::size_t N, typename CharT, typename Alloc, bool Const>
class tuple_element<N, uxs::db::detail::value_iterator<CharT, Alloc, Const>> {
 public:
    using type = decltype(get<N>(std::declval<uxs::db::detail::value_iterator<CharT, Alloc, Const>>()));
};

template<typename CharT, typename Alloc, bool Const>
void addressof(uxs::db::detail::value_iterator<CharT, Alloc, Const>&) = delete;

template<typename CharT, typename Alloc>
void swap(uxs::db::basic_value<CharT, Alloc>& v1, uxs::db::basic_value<CharT, Alloc>& v2) noexcept {
    v1.swap(v2);
}
}  // namespace std
