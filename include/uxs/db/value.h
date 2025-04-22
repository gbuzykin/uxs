#pragma once

#include "database_error.h"

#include "uxs/dllist.h"  // NOLINT
#include "uxs/iterator.h"
#include "uxs/memory.h"
#include "uxs/optional.h"
#include "uxs/span.h"
#include "uxs/string_view.h"

#include <algorithm>
#include <atomic>
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

//-----------------------------------------------------------------------------
// Flexible array implementation
namespace detail {

template<typename Ty, typename Alloc>
class flexarray_t {
 private:
    struct data_t {
        std::atomic<std::size_t> ref_count;
        std::size_t size;
        std::size_t capacity;
        alignas(std::alignment_of<Ty>::value) std::uint8_t x[4 * sizeof(Ty)];
        Ty* data() noexcept { return reinterpret_cast<Ty*>(&x); }
    };

 public:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<data_t>;
    using const_view_type = std::conditional_t<is_character<Ty>::value, std::basic_string_view<Ty>, est::span<const Ty>>;
    using view_type = est::span<Ty>;

    std::size_t size() const noexcept { return p_ ? p_->size : 0; }
    const_view_type cview() const noexcept { return p_ ? const_view_type(p_->data(), p_->size) : const_view_type(); }

    const Ty* cbegin() const noexcept {
        assert(p_);
        return p_->data();
    }

    const Ty* cend() const noexcept {
        assert(p_);
        return p_->data() + p_->size;
    }

    const Ty& operator[](std::size_t i) const noexcept {
        assert(p_ && i < p_->size);
        return *(p_->data() + i);
    }

    friend bool operator==(const flexarray_t& lhs, const flexarray_t& rhs) noexcept {
        const auto lhv = lhs.cview();
        const auto rhv = rhs.cview();
        return lhv.size() == rhv.size() && std::equal(lhv.begin(), lhv.end(), rhv.begin());
    }

    void construct() noexcept { p_ = nullptr; }
    UXS_EXPORT void construct(alloc_type& al, const_view_type init);
#if __cplusplus < 201703L
    UXS_EXPORT void construct(alloc_type& al, std::initializer_list<Ty> init);
#endif  // __cplusplus < 201703L

    template<typename InputIt>
    void construct(alloc_type& al, InputIt first, InputIt last) {
        p_ = nullptr;
        try {
            create_impl(al, first, last, is_random_access_iterator<InputIt>());
        } catch (...) {
            if (p_) { destruct(al); }
            throw;
        }
    }

    view_type view(alloc_type& al) {
        if (!p_) { return view_type(); }
        unique(al);
        return view_type(p_->data(), p_->size);
    }

    void assign(alloc_type& al, const_view_type init);
    void append(alloc_type& al, const_view_type init);

    template<typename InputIt>
    void assign(alloc_type& al, InputIt first, InputIt last) {
        if (p_ && p_->ref_count == 1) { return assign_impl(al, first, last, is_random_access_iterator<InputIt>()); }
        flexarray_t new_arr;
        new_arr.construct(al, first, last);
        reset(al, new_arr.p_);
    }

    template<typename InputIt>
    void insert(alloc_type& al, std::size_t pos, InputIt first, InputIt last) {
        if (!p_) { return create_impl(al, first, last, is_random_access_iterator<InputIt>()); }
        unique(al);
        const std::size_t old_sz = p_->size;
        append_impl(al, first, last, is_random_access_iterator<InputIt>());
        if (pos < old_sz) { std::rotate(p_->data() + pos, p_->data() + old_sz, p_->data() + p_->size); }
    }

    template<typename... Args>
    Ty& emplace_back(alloc_type& al, Args&&... args) {
        if (!p_) {
            p_ = alloc(al, 0, 0);
        } else {
            unique(al);
            if (p_->size == p_->capacity) { grow(al, 1); }
        }
        Ty* item = new (p_->data() + p_->size) Ty(std::forward<Args>(args)...);
        ++p_->size;
        return *item;
    }

    template<typename... Args>
    Ty& emplace(alloc_type& al, std::size_t pos, Args&&... args) {
        emplace_back(al, std::forward<Args>(args)...);
        if (pos < p_->size - 1) {
            rotate_back(pos);
        } else {
            pos = p_->size - 1;
        }
        return *(p_->data() + pos);
    }

    void pop_back(alloc_type& al) {
        assert(p_ && p_->size);
        unique(al);
        (p_->data() + --p_->size)->~Ty();
    }

    void clear(alloc_type& al) noexcept;
    void reserve(alloc_type& al, std::size_t sz);
    void resize(alloc_type& al, std::size_t sz, const Ty& v);
    Ty* erase(alloc_type& al, const Ty* item_to_erase);

    void ref() noexcept {
        if (p_) { ++p_->ref_count; }
    }

    void unref(alloc_type& al) noexcept {
        if (p_ && --p_->ref_count == 0) { destruct(al); }
    }

    void unique(alloc_type& al) {
        if (p_->ref_count != 1) { unique_impl(al); }
    }

 private:
    data_t* p_;

    template<typename RandIt>
    void create_impl(alloc_type& al, std::size_t count, RandIt first);

    template<typename RandIt>
    void create_impl(alloc_type& al, RandIt first, RandIt last, std::true_type /* random access iterator */) {
        create_impl(al, static_cast<std::size_t>(last - first), first);
    }
    template<typename InputIt>
    void create_impl(alloc_type& al, InputIt first, InputIt last, std::false_type /* random access iterator */);

    template<typename RandIt>
    void assign_impl(alloc_type& al, std::size_t count, RandIt first);

    template<typename RandIt>
    void assign_impl(alloc_type& al, RandIt first, RandIt last, std::true_type /* random access iterator */) {
        assign_impl(al, static_cast<std::size_t>(last - first), first);
    }
    template<typename InputIt>
    void assign_impl(alloc_type& al, InputIt first, InputIt last, std::false_type /* random access iterator */);

    template<typename RandIt>
    void append_impl(alloc_type& al, std::size_t count, RandIt first);

    template<typename RandIt>
    void append_impl(alloc_type& al, RandIt first, RandIt last, std::true_type /* random access iterator */) {
        append_impl(al, static_cast<std::size_t>(last - first), first);
    }
    template<typename InputIt>
    void append_impl(alloc_type& al, InputIt first, InputIt last, std::false_type /* random access iterator */);

    template<typename Ty_ = Ty, typename = std::enable_if_t<std::is_trivially_destructible<Ty_>::value>>
    static void destruct_items(Ty_* first, Ty_* last) noexcept {}

    template<typename Ty_ = Ty, typename... Dummy>
    static void destruct_items(Ty_* first, Ty_* last, Dummy&&...) noexcept {
        for (; first != last; ++first) { first->~Ty(); };
    }

    UXS_EXPORT void grow(alloc_type& al, std::size_t extra);
    UXS_EXPORT void rotate_back(std::size_t pos) noexcept;
    UXS_EXPORT void unique_impl(alloc_type& al);
    UXS_EXPORT void destruct(alloc_type& al) noexcept;

    void reset(alloc_type& al, data_t* p) noexcept {
        unref(al);
        p_ = p;
    }

    static std::size_t max_size(const alloc_type& al) noexcept {
        return (std::allocator_traits<alloc_type>::max_size(al) * sizeof(data_t) - offsetof(data_t, x)) / sizeof(Ty);
    }

    static std::size_t get_alloc_sz(std::size_t cap) noexcept {
        return (offsetof(data_t, x) + cap * sizeof(Ty) + sizeof(data_t) - 1) / sizeof(data_t);
    }

    UXS_NODISCARD UXS_EXPORT static data_t* alloc(alloc_type& al, std::size_t sz, std::size_t cap);
    UXS_NODISCARD static data_t* alloc_checked(alloc_type& al, std::size_t sz, std::size_t cap) {
        if (cap > max_size(al)) { throw std::length_error("too much to reserve"); }
        return alloc(al, sz, cap);
    }

    static void dealloc(alloc_type& al, data_t* arr) noexcept { al.deallocate(arr, get_alloc_sz(arr->capacity)); }
};

template<typename Ty, typename Alloc>
template<typename RandIt>
void flexarray_t<Ty, Alloc>::create_impl(alloc_type& al, std::size_t count, RandIt first) {
    if (!count) { return; }
    p_ = alloc_checked(al, 0, count);
    std::uninitialized_copy_n(first, count, p_->data());
    p_->size = count;
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void flexarray_t<Ty, Alloc>::create_impl(alloc_type& al, InputIt first, InputIt last,
                                         std::false_type /* random access iterator */) {
    if (first == last) { return; }
    p_ = alloc(al, 0, 0);
    append_impl(al, first, last, std::false_type{});
}

template<typename Ty, typename Alloc>
template<typename RandIt>
void flexarray_t<Ty, Alloc>::assign_impl(alloc_type& al, std::size_t count, RandIt first) {
    if (count > p_->capacity) { grow(al, count - p_->size); }
    Ty* item = std::copy_n(first, std::min(count, p_->size), p_->data());
    if (count <= p_->size) {
        destruct_items(item, p_->data() + p_->size);
    } else {
        std::uninitialized_copy_n(first + p_->size, count - p_->size, item);
    }
    p_->size = count;
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void flexarray_t<Ty, Alloc>::assign_impl(alloc_type& al, InputIt first, InputIt last,
                                         std::false_type /* random access iterator */) {
    Ty* item = p_->data();
    Ty* end = p_->data() + p_->size;
    for (; item != end && first != last; ++first) { *item++ = *first; }
    if (item != end) {
        destruct_items(item, end);
        p_->size = static_cast<std::size_t>(item - p_->data());
    } else {
        append_impl(al, first, last, std::false_type{});
    }
}

template<typename Ty, typename Alloc>
template<typename RandIt>
void flexarray_t<Ty, Alloc>::append_impl(alloc_type& al, std::size_t count, RandIt first) {
    if (count > p_->capacity - p_->size) { grow(al, count); }
    std::uninitialized_copy_n(first, count, p_->data() + p_->size);
    p_->size += count;
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void flexarray_t<Ty, Alloc>::append_impl(alloc_type& al, InputIt first, InputIt last,
                                         std::false_type /* random access iterator */) {
    for (; first != last; ++first) {
        if (p_->size == p_->capacity) { grow(al, 1); }
        new (p_->data() + p_->size) Ty(*first);
        ++p_->size;
    }
}

}  // namespace detail

//-----------------------------------------------------------------------------
// Record container implementation
namespace detail {

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
class record_t;

template<typename CharT, typename Alloc>
class record_value {
 public:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<record_value>;
    using char_type = CharT;
    using key_type = std::basic_string_view<char_type>;
    using value_type = basic_value<char_type, Alloc>;

    key_type key() const noexcept { return key_type(key_chars_, key_sz_); }
    const value_type& value() const noexcept { return *reinterpret_cast<const value_type*>(&x_); }
    value_type& value() noexcept { return *reinterpret_cast<value_type*>(&x_); }

    friend bool operator==(const record_value& lhs, const record_value& rhs) noexcept {
        return lhs.key() == rhs.key() && lhs.value() == rhs.value();
    }
    friend bool operator!=(const record_value& lhs, const record_value& rhs) noexcept { return !(lhs == rhs); }

    static record_value* from_links(list_links_t* links) noexcept {
        return get_containing_record<record_value, offsetof(record_value, links_)>(links);
    }

 private:
    friend class record_t<CharT, Alloc>;

    list_links_t links_;
    list_links_t* next_bucket_;
    std::size_t hash_code_;
    alignas(std::alignment_of<value_type>::value) std::uint8_t x_[sizeof(value_type)];
    std::size_t key_sz_;
    char_type key_chars_[16];

    template<typename... Args>
    static record_value* create(alloc_type& al, key_type key, Args&&... args) {
        record_value* node = alloc(al, key);
        try {
            new (&node->value()) value_type(std::forward<Args>(args)...);
            return node;
        } catch (...) {
            dealloc(al, node);
            throw;
        }
    }

    static void destroy(alloc_type& al, record_value* node) noexcept {
        node->value().~value_type();
        dealloc(al, node);
    }

    static std::size_t max_name_size(const alloc_type& al) noexcept {
        return (std::allocator_traits<alloc_type>::max_size(al) * sizeof(record_value) -
                offsetof(record_value, key_chars_)) /
               sizeof(CharT);
    }

    static std::size_t get_alloc_sz(std::size_t key_sz) noexcept {
        return (offsetof(record_value, key_chars_) + key_sz * sizeof(CharT) + sizeof(record_value) - 1) /
               sizeof(record_value);
    }

    UXS_NODISCARD UXS_EXPORT static record_value* alloc(alloc_type& al, key_type key);

    static void dealloc(alloc_type& al, record_value* node) noexcept {
        al.deallocate(node, get_alloc_sz(node->key_sz_));
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

template<typename RandIt>
std::size_t initial_alloc_size(RandIt first, RandIt last, std::true_type /* random access iterator */) {
    return static_cast<std::size_t>(last - first);
}

template<typename InputIt>
std::false_type initial_alloc_size(InputIt first, InputIt last, std::false_type /* random access iterator */) {
    return {};
}

template<typename CharT, typename Alloc>
class record_t {
 private:
    struct data_t {
        std::atomic<std::size_t> ref_count;
        list_links_t head;
        std::size_t size;
        std::size_t bucket_count;
        list_links_t* hashtbl[4];
        UXS_EXPORT void init() noexcept;
        void init_from(data_t* p) noexcept;
    };

 public:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<data_t>;
    using key_type = std::basic_string_view<CharT>;
    using mapped_type = basic_value<CharT, Alloc>;
    using value_type = record_value<CharT, Alloc>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using reference = value_type&;
    using const_reference = const value_type&;
    using node_traits = record_node_traits<CharT, Alloc>;
    using node_t = typename node_traits::node_t;
    using hasher_t = std::hash<key_type>;
    using iterator = list_iterator<record_t, node_traits, false>;
    using const_iterator = list_iterator<record_t, node_traits, true>;

    size_type size() const noexcept { return p_->size; }
    list_links_t* cbegin() const noexcept { return p_->head.next; }
    list_links_t* cend() const noexcept { return &p_->head; }
    list_links_t* find(key_type key) const noexcept { return find_impl(key, hasher_t{}(key)); }
    UXS_EXPORT size_type count(key_type key) const noexcept;

    friend bool operator==(const record_t& lhs, const record_t& rhs) noexcept {
        return lhs.size() == rhs.size() &&
               std::equal(const_iterator(lhs.cbegin()), const_iterator(lhs.cend()), const_iterator(rhs.cbegin()));
    }

    void construct(alloc_type& al, std::false_type = {}) {
        p_ = alloc(al, 0);
        p_->init();
    }

    void construct(alloc_type& al, std::size_t bucket_count) {
        if (bucket_count > max_size(al)) { throw std::length_error("too much to reserve"); }
        p_ = alloc(al, bucket_count);
        p_->init();
    }

    void construct(alloc_type& al, record_t rec);
    void construct(alloc_type& al, std::initializer_list<mapped_type> init);
    UXS_EXPORT void construct(alloc_type& al, std::initializer_list<std::pair<key_type, mapped_type>> init);

    template<typename InputIt>
    void construct(alloc_type& al, InputIt first, InputIt last) {
        construct(al, initial_alloc_size(first, last, is_random_access_iterator<InputIt>()));
        try {
            insert_impl(al, first, last, is_random_access_iterator<InputIt>());
        } catch (...) {
            destruct(al);
            throw;
        }
    }

    void assign(alloc_type& al, std::initializer_list<mapped_type> init);

    template<typename InputIt>
    void assign(alloc_type& al, InputIt first, InputIt last) {
        clear_impl(al, initial_alloc_size(first, last, is_random_access_iterator<InputIt>()));
        insert_impl(al, first, last, is_random_access_iterator<InputIt>());
    }

    template<typename InputIt>
    void insert(alloc_type& al, InputIt first, InputIt last) {
        unique(al);
        insert_impl(al, first, last, is_random_access_iterator<InputIt>());
    }

    template<typename... Args>
    list_links_t* emplace(alloc_type& al, key_type key, Args&&... args) {
        unique(al);
        if (p_->size == p_->bucket_count) { rehash(al, 1); }
        typename node_t::alloc_type node_al(al);
        node_t* node = node_t::create(node_al, key, std::forward<Args>(args)...);
        insert_node(node, hasher_t{}(key));
        return &node->links_;
    }

    template<typename... Args>
    std::pair<list_links_t*, bool> emplace_unique(alloc_type& al, key_type key, Args&&... args) {
        unique(al);
        const std::size_t hash_code = hasher_t{}(key);
        list_links_t* node = find_impl(key, hash_code);
        if (node != &p_->head) { return std::make_pair(node, false); }
        if (p_->size == p_->bucket_count) { rehash(al, 1); }
        typename node_t::alloc_type node_al(al);
        node_t* new_node = node_t::create(node_al, key, std::forward<Args>(args)...);
        insert_node(new_node, hash_code);
        return std::make_pair(&new_node->links_, true);
    }

    void clear(alloc_type& al) { clear_impl(al); }
    list_links_t* erase(alloc_type& al, list_links_t* node);
    std::size_t erase(alloc_type& al, key_type key);

    void ref() noexcept { ++p_->ref_count; }

    void unref(alloc_type& al) noexcept {
        if (--p_->ref_count == 0) { destruct(al); }
    }

    void unique(alloc_type& al) {
        if (p_->ref_count != 1) { unique_impl(al); }
    }

 private:
    data_t* p_;

    void insert_impl(alloc_type& al, std::initializer_list<mapped_type> init);

    template<typename RandIt>
    void insert_impl(alloc_type& al, RandIt first, RandIt last, std::true_type /* random access iterator */);
    template<typename InputIt>
    void insert_impl(alloc_type& al, InputIt first, InputIt last, std::false_type /* random access iterator */);

    void destruct_items(alloc_type& al) noexcept;
    void add_to_hash(node_t* node) noexcept;
    UXS_EXPORT void insert_node(node_t* node, std::size_t hash_code) noexcept;
    UXS_EXPORT void rehash(alloc_type& al, std::size_t extra);
    UXS_EXPORT void unique_impl(alloc_type& al);
    UXS_EXPORT void clear_impl(alloc_type& al, std::false_type = {});
    UXS_EXPORT void clear_impl(alloc_type& al, std::size_t bucket_count);
    UXS_EXPORT void destruct(alloc_type& al) noexcept;
    UXS_EXPORT list_links_t* find_impl(key_type key, std::size_t hash_code) const noexcept;

    void reset(alloc_type& al, data_t* p) noexcept {
        unref(al);
        p_ = p;
    }

    static std::size_t max_size(const alloc_type& al) noexcept {
        return (std::allocator_traits<alloc_type>::max_size(al) * sizeof(data_t) - offsetof(data_t, hashtbl)) /
               sizeof(list_links_t*);
    }

    static std::size_t get_alloc_sz(std::size_t bucket_count) noexcept {
        return (offsetof(data_t, hashtbl) + bucket_count * sizeof(list_links_t*) + sizeof(data_t) - 1) / sizeof(data_t);
    }

    UXS_NODISCARD UXS_EXPORT static data_t* alloc(alloc_type& al, std::size_t bucket_count);

    static void dealloc(alloc_type& al, data_t* rec) noexcept { al.deallocate(rec, get_alloc_sz(rec->bucket_count)); }
};

template<typename CharT, typename Alloc>
template<typename RandIt>
void record_t<CharT, Alloc>::insert_impl(alloc_type& al, RandIt first, RandIt last,
                                         std::true_type /* random access iterator */) {
    const std::size_t count = static_cast<std::size_t>(last - first);
    if (p_->bucket_count - p_->size < count) { rehash(al, count); }
    typename node_t::alloc_type node_al(al);
    for (; first != last; ++first) {
        const auto key = std::get<0>(*first);
        node_t* node = node_t::create(node_al, key, std::get<1>(*first));
        insert_node(node, hasher_t{}(key));
    }
}

template<typename CharT, typename Alloc>
template<typename InputIt>
void record_t<CharT, Alloc>::insert_impl(alloc_type& al, InputIt first, InputIt last,
                                         std::false_type /* random access iterator */) {
    typename node_t::alloc_type node_al(al);
    for (; first != last; ++first) {
        if (p_->size == p_->bucket_count) { rehash(al, 1); }
        const auto key = std::get<0>(*first);
        node_t* node = node_t::create(node_al, key, std::get<1>(*first));
        insert_node(node, hasher_t{}(key));
    }
}

//-----------------------------------------------------------------------------
// Universal value iterator

template<typename CharT, typename Alloc, bool Const>
class value_iterator : public container_iterator_facade<basic_value<CharT, Alloc>, value_iterator<CharT, Alloc, Const>,
                                                        std::bidirectional_iterator_tag, Const> {
 public:
    using key_type = std::basic_string_view<CharT>;
    using value_type = basic_value<CharT, Alloc>;

    template<typename, typename, bool>
    friend class value_iterator;
    friend class basic_value<CharT, Alloc>;

    value_iterator() noexcept = default;
#if UXS_ITERATOR_DEBUG_LEVEL != 0
    explicit value_iterator(value_type* ptr, const value_type* begin, const value_type* end) noexcept
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
    explicit value_iterator(value_type* ptr, const value_type* begin, const value_type* end) noexcept
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
        assert(ptr_);
        uxs_iterator_assert(is_record_ ? ptr_ != head() : begin_ <= ptr_ && ptr_ < end_);
        ptr_ = is_record_ ? static_cast<void*>(static_cast<list_links_t*>(ptr_)->next) :
                            static_cast<void*>(static_cast<value_type*>(ptr_) + 1);
    }

    void decrement() noexcept {
        assert(ptr_);
        uxs_iterator_assert(is_record_ ? ptr_ != head()->next : begin_ < ptr_ && ptr_ <= end_);
        ptr_ = is_record_ ? static_cast<void*>(static_cast<list_links_t*>(ptr_)->prev) :
                            static_cast<void*>(static_cast<value_type*>(ptr_) - 1);
    }

    template<bool Const2>
    bool is_equal_to(const value_iterator<CharT, Alloc, Const2>& it) const noexcept {
        assert(is_record_ == it.is_record_);
        assert(!is_record_ || (!ptr_ && !it.ptr_) || (ptr_ && it.ptr_));
        uxs_iterator_assert(is_record_ ? !ptr_ || head() == it.head() : begin_ == it.begin_ && end_ == it.end_);
        return ptr_ == it.ptr_;
    }

    value_iterator dereference() const noexcept { return *this; }

    bool is_record() const noexcept { return is_record_; }

    key_type key() const {
        if (!is_record_) { throw database_error("cannot use key() for non-record iterators"); }
        return record_value<CharT, Alloc>::from_links(static_cast<list_links_t*>(ptr_))->key();
    }

    std::conditional_t<Const, const value_type&, value_type&> value() const noexcept {
        assert(ptr_);
        uxs_iterator_assert(is_record_ ? ptr_ != head() : begin_ <= ptr_ && ptr_ < end_);
        return is_record_ ? record_value<CharT, Alloc>::from_links(static_cast<list_links_t*>(ptr_))->value() :
                            *static_cast<value_type*>(ptr_);
    }

 private:
    bool is_record_ = false;
    void* ptr_ = nullptr;
#if UXS_ITERATOR_DEBUG_LEVEL != 0
    const void* begin_ = nullptr;
    const void* end_ = nullptr;
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

//-----------------------------------------------------------------------------
// uxs::basic_value<> implementation

struct array_construct_t {
    explicit array_construct_t() = default;
};
struct record_construct_t {
    explicit record_construct_t() = default;
};

#if __cplusplus >= 201703L
constexpr array_construct_t array_construct{};
constexpr record_construct_t record_construct{};
#endif  // __cplusplus >= 201703L

namespace detail {
template<typename CharT, typename Alloc, typename Ty, typename = void>
struct is_record_value : std::false_type {};
template<typename CharT, typename Alloc, typename FirstTy, typename SecondTy>
struct is_record_value<CharT, Alloc, std::pair<FirstTy, SecondTy>,
                       std::enable_if_t<std::is_convertible<FirstTy, typename record_t<CharT, Alloc>::key_type>::value &&
                                        std::is_convertible<SecondTy, basic_value<CharT, Alloc>>::value>>
    : std::true_type {};
template<typename CharT, typename Alloc, typename InputIt>
using is_record_iterator = is_record_value<CharT, Alloc, typename std::iterator_traits<InputIt>::value_type>;
template<typename CharT, typename Alloc, typename InputIt>
using select_construct_t =
    std::conditional_t<is_record_iterator<CharT, Alloc, InputIt>::value, record_construct_t, array_construct_t>;
}  // namespace detail

template<typename CharT, typename Alloc = std::allocator<CharT>>
class basic_value : protected std::allocator_traits<Alloc>::template rebind_alloc<CharT> {
 private:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<CharT>;
    using char_array_t = detail::flexarray_t<CharT, Alloc>;
    using value_array_t = detail::flexarray_t<basic_value, Alloc>;
    using record_t = detail::record_t<CharT, Alloc>;

 public:
    using char_type = CharT;
    using key_type = typename record_t::key_type;
    using value_type = basic_value<CharT, Alloc>;
    using allocator_type = Alloc;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using iterator = detail::value_iterator<CharT, Alloc, false>;
    using const_iterator = detail::value_iterator<CharT, Alloc, true>;
    using reverse_iterator = detail::value_reverse_iterator<iterator>;
    using const_reverse_iterator = detail::value_reverse_iterator<const_iterator>;
    using record_iterator = typename record_t::iterator;
    using const_record_iterator = typename record_t::const_iterator;
    using pointer = void;
    using const_pointer = void;
    using reference = iterator;
    using const_reference = const_iterator;

    basic_value() noexcept(std::is_nothrow_default_constructible<alloc_type>::value)
        : alloc_type(), type_(dtype::null) {}
    basic_value(std::nullptr_t) noexcept(std::is_nothrow_default_constructible<alloc_type>::value)
        : alloc_type(), type_(dtype::null) {}
    basic_value(array_construct_t) noexcept(std::is_nothrow_default_constructible<alloc_type>::value)
        : alloc_type(), type_(dtype::array) {
        value_.arr.construct();
    }
    basic_value(record_construct_t) : alloc_type(), type_(dtype::record) {
        typename record_t::alloc_type rec_al(*this);
        value_.rec.construct(rec_al);
    }
    basic_value(std::basic_string_view<char_type> s) : alloc_type(), type_(dtype::string) {
        typename char_array_t::alloc_type str_al(*this);
        value_.str.construct(str_al, s);
    }
    basic_value(const char_type* cstr) : basic_value(std::basic_string_view<char_type>(cstr)) {}

    explicit basic_value(const Alloc& al) noexcept : alloc_type(al), type_(dtype::null) {}
    basic_value(std::nullptr_t, const Alloc& al) noexcept : alloc_type(al), type_(dtype::null) {}
    basic_value(array_construct_t, const Alloc& al) noexcept : alloc_type(al), type_(dtype::array) {
        value_.arr.construct();
    }
    basic_value(record_construct_t, const Alloc& al) : alloc_type(al), type_(dtype::record) {
        typename record_t::alloc_type rec_al(*this);
        value_.rec.construct(rec_al);
    }
    basic_value(std::basic_string_view<char_type> s, const Alloc& al) : alloc_type(al), type_(dtype::string) {
        typename char_array_t::alloc_type str_al(*this);
        value_.str.construct(str_al, s);
    }
    basic_value(const char_type* cstr, const Alloc& al) : basic_value(std::basic_string_view<char_type>(cstr), al) {}

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    basic_value(InputIt first, InputIt last, const Alloc& al = Alloc())
        : basic_value(detail::select_construct_t<CharT, Alloc, InputIt>(), first, last, al) {}
    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    basic_value(array_construct_t, InputIt first, InputIt last, const Alloc& al = Alloc())
        : alloc_type(al), type_(dtype::array) {
        typename value_array_t::alloc_type arr_al(*this);
        value_.arr.construct(arr_al, first, last);
    }
    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value &&
                                                           detail::is_record_iterator<CharT, Alloc, InputIt>::value>>
    basic_value(record_construct_t, InputIt first, InputIt last, const Alloc& al = Alloc())
        : alloc_type(al), type_(dtype::record) {
        typename record_t::alloc_type rec_al(*this);
        value_.rec.construct(rec_al, first, last);
    }

    UXS_EXPORT basic_value(std::initializer_list<basic_value> init, const Alloc& al = Alloc());
    basic_value(array_construct_t, std::initializer_list<basic_value> init, const Alloc& al = Alloc())
        : alloc_type(al), type_(dtype::array) {
        typename value_array_t::alloc_type arr_al(al);
        value_.arr.construct(arr_al, init);
    }
    basic_value(record_construct_t, std::initializer_list<std::pair<key_type, basic_value>> init,
                const Alloc& al = Alloc())
        : alloc_type(al), type_(dtype::record) {
        typename record_t::alloc_type rec_al(*this);
        value_.rec.construct(rec_al, init);
    }

    ~basic_value() {
        if (type_ != dtype::null) { destroy(); }
    }

    basic_value(const basic_value& other) noexcept(std::is_nothrow_default_constructible<alloc_type>::value)
        : alloc_type(), type_(other.type_) {
        init_from(other);
    }
    basic_value(const basic_value& other, const Alloc& al) noexcept : alloc_type(al), type_(other.type_) {
        init_from(other);
    }
    basic_value& operator=(const basic_value& other) noexcept {
        if (&other == this) { return *this; }
        if (type_ != dtype::null) { destroy(); }
        type_ = other.type_;
        init_from(other);
        return *this;
    }

    basic_value(basic_value&& other) noexcept : alloc_type(std::move(other)), type_(other.type_), value_(other.value_) {
        other.type_ = dtype::null;
    }
    basic_value(basic_value&& other, const Alloc& al) noexcept : alloc_type(al), type_(other.type_) {
        move_construct_impl(std::move(other), is_alloc_always_equal<alloc_type>());
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
        assign(detail::select_construct_t<CharT, Alloc, InputIt>(), first, last);
    }
    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    void assign(array_construct_t, InputIt first, InputIt last);
    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value &&
                                                           detail::is_record_iterator<CharT, Alloc, InputIt>::value>>
    void assign(record_construct_t, InputIt first, InputIt last);

    UXS_EXPORT void assign(std::initializer_list<basic_value> init);
    UXS_EXPORT void assign(array_construct_t, std::initializer_list<basic_value> init);
    UXS_EXPORT void assign(record_construct_t, std::initializer_list<std::pair<key_type, basic_value>> init);

#define UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(ty, id, field) \
    basic_value(ty v) noexcept(std::is_nothrow_default_constructible<alloc_type>::value) : alloc_type(), type_(id) { \
        value_.field = static_cast<decltype(value_.field)>(v); \
    } \
    basic_value(ty v, const Alloc& al) noexcept : alloc_type(al), type_(id) { \
        value_.field = static_cast<decltype(value_.field)>(v); \
    } \
    basic_value& operator=(ty v) noexcept { \
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

    UXS_EXPORT basic_value& operator=(std::basic_string_view<char_type> s);
    basic_value& operator=(const char_type* cstr) { return (*this = std::basic_string_view<char_type>(cstr)); }

    basic_value& operator=(std::nullptr_t) noexcept {
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

    UXS_EXPORT basic_value& append_string(std::basic_string_view<char_type> s);
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
        const auto result = get<Ty>();
        return result ? *result : Ty(std::forward<U>(default_value));
    }

    template<typename Ty, typename U>
    Ty value_or(std::size_t i, U&& default_value) const {
        const auto range = as_array();
        if (i < range.size()) {
            const auto result = range[i].template get<Ty>();
            if (result) { return *result; }
        }
        return Ty(std::forward<U>(default_value));
    }

    template<typename Ty, typename U>
    Ty value_or(key_type key, U&& default_value) const {
        const auto it = find(key);
        if (it != end()) {
            const auto result = it.value().template get<Ty>();
            if (result) { return *result; }
        }
        return Ty(std::forward<U>(default_value));
    }

    template<typename Ty>
    Ty value() const {
        return value_or<Ty>(Ty());
    }

    template<typename Ty>
    Ty value(std::size_t i) const {
        return value_or<Ty>(i, Ty());
    }

    template<typename Ty>
    Ty value(key_type key) const {
        return value_or<Ty>(key, Ty());
    }

    basic_value value(std::size_t i) const {
        const auto range = as_array();
        return i < range.size() ? range[i] : basic_value();
    }

    basic_value value(key_type key) const {
        const auto it = find(key);
        return it != end() ? it.value() : basic_value();
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
        return type_ == dtype::string ? est::make_optional(value_.str.cview()) : est::nullopt();
    }

    bool empty() const noexcept { return size() == 0; }
    UXS_EXPORT std::size_t size() const noexcept;

    UXS_EXPORT iterator begin();
    UXS_EXPORT const_iterator begin() const noexcept;
    const_iterator cbegin() const noexcept { return begin(); }

    UXS_EXPORT iterator end();
    UXS_EXPORT const_iterator end() const noexcept;
    const_iterator cend() const noexcept { return end(); }

    reverse_iterator rbegin() { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }

    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return rend(); }

    reference front() { return begin(); }
    const_reference front() const { return begin(); }

    reference back() { return std::prev(end()); }
    const_reference back() const { return std::prev(end()); }

    est::span<const basic_value> as_array() const noexcept;
    est::span<basic_value> as_array();

    iterator_range<const_record_iterator> as_record() const;
    iterator_range<record_iterator> as_record();

    const basic_value& operator[](std::size_t i) const { return as_array()[i]; }
    basic_value& operator[](std::size_t i) { return as_array()[i]; }

    const basic_value& at(std::size_t i) const {
        const auto range = as_array();
        if (i < range.size()) { return range[i]; }
        throw database_error("index out of range");
    }

    basic_value& at(std::size_t i) {
        const auto range = as_array();
        if (i < range.size()) { return range[i]; }
        throw database_error("index out of range");
    }

    const basic_value& at(key_type key) const {
        const auto it = find(key);
        if (it != end()) { return it.value(); }
        throw database_error("invalid key");
    }

    basic_value& at(key_type key) {
        const auto it = find(key);
        if (it != std::as_const(*this).end()) { return it.value(); }
        throw database_error("invalid key");
    }

    basic_value& operator[](key_type key) {
        return emplace_unique(key, static_cast<const Alloc&>(*this)).first.value();
    }

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
            case dtype::string: return func(value_.str.cview());
            case dtype::array:
            case dtype::record: return func(make_range(begin(), end()));
            default: UXS_UNREACHABLE_CODE;
        }
    }

    UXS_EXPORT const_iterator find(key_type key) const noexcept;
    UXS_EXPORT iterator find(key_type key);
    bool contains(key_type key) const noexcept { return find(key) != end(); }
    std::size_t count(key_type key) const noexcept { return type_ == dtype::record ? value_.rec.count(key) : 0; }

    UXS_EXPORT void clear();
    UXS_EXPORT void reserve(std::size_t sz);
    UXS_EXPORT void resize(std::size_t sz);
    UXS_EXPORT void resize(std::size_t sz, const basic_value& v);

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
    iterator emplace(key_type key, Args&&... args);
    iterator insert(key_type key, const basic_value& v) { return emplace(key, v); }
    iterator insert(key_type key, basic_value&& v) { return emplace(key, std::move(v)); }

    template<typename... Args>
    std::pair<iterator, bool> emplace_unique(key_type key, Args&&... args);
    std::pair<iterator, bool> insert_unique(key_type key, const basic_value& v) { return emplace_unique(key, v); }
    std::pair<iterator, bool> insert_unique(key_type key, basic_value&& v) { return emplace_unique(key, std::move(v)); }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    void insert(std::size_t pos, InputIt first, InputIt last);
    UXS_EXPORT void insert(std::size_t pos, std::initializer_list<basic_value> init);

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value &&
                                                           detail::is_record_iterator<CharT, Alloc, InputIt>::value>>
    void insert(InputIt first, InputIt last);
    UXS_EXPORT void insert(std::initializer_list<std::pair<key_type, basic_value>> init);

    UXS_EXPORT void erase(std::size_t pos);
    UXS_EXPORT iterator erase(const_iterator it);
    UXS_EXPORT std::size_t erase(key_type key);

 private:
    friend class detail::record_t<CharT, Alloc>;

    dtype type_;

    union {
        bool b;
        std::int32_t i;
        std::uint32_t u;
        std::int64_t i64;
        std::uint64_t u64;
        double dbl;
        char_array_t str;
        value_array_t arr;
        record_t rec;
    } value_;

    UXS_EXPORT void init_from(const basic_value& other) noexcept;
    UXS_EXPORT void destroy() noexcept;
    UXS_EXPORT void init_as_string();
    UXS_EXPORT void init_as_array();
    UXS_EXPORT void init_as_record();
    UXS_EXPORT void convert_to_array();

    void move_construct_impl(basic_value&& other, std::true_type) noexcept {
        type_ = other.type_, value_ = other.value_;
        other.type_ = dtype::null;
    }

    void move_construct_impl(basic_value&& other, std::false_type) noexcept {
        if (static_cast<alloc_type&>(*this) == static_cast<alloc_type&>(other)) {
            type_ = other.type_, value_ = other.value_;
            other.type_ = dtype::null;
        } else {
            init_from(other);
        }
    }
};

// --------------------------

template<typename CharT, typename Alloc>
template<typename InputIt, typename>
void basic_value<CharT, Alloc>::assign(array_construct_t, InputIt first, InputIt last) {
    if (type_ != dtype::array) {
        if (type_ != dtype::null) { destroy(); }
        value_.arr.construct();
        type_ = dtype::array;
    }
    typename value_array_t::alloc_type arr_al(*this);
    value_.arr.assign(arr_al, first, last);
}

template<typename CharT, typename Alloc>
template<typename InputIt, typename>
void basic_value<CharT, Alloc>::assign(record_construct_t, InputIt first, InputIt last) {
    typename record_t::alloc_type rec_al(*this);
    if (type_ != dtype::record) {
        if (type_ != dtype::null) { destroy(); }
        value_.rec.construct(rec_al, detail::initial_alloc_size(first, last, is_random_access_iterator<InputIt>()));
        type_ = dtype::record;
    }
    value_.rec.assign(rec_al, first, last);
}

template<typename CharT, typename Alloc>
template<typename... Args>
basic_value<CharT, Alloc>& basic_value<CharT, Alloc>::emplace_back(Args&&... args) {
    if (type_ != dtype::array) { convert_to_array(); }
    typename value_array_t::alloc_type arr_al(*this);
    return value_.arr.emplace_back(arr_al, std::forward<Args>(args)...);
}

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::pop_back() {
    if (type_ != dtype::array) { throw database_error("not an array"); }
    typename value_array_t::alloc_type arr_al(*this);
    value_.arr.pop_back(arr_al);
}

template<typename CharT, typename Alloc>
template<typename... Args>
auto basic_value<CharT, Alloc>::emplace(std::size_t pos, Args&&... args) -> iterator {
    if (type_ != dtype::array) { init_as_array(); }
    typename value_array_t::alloc_type arr_al(*this);
    basic_value& item = value_.arr.emplace(arr_al, pos, std::forward<Args>(args)...);
    return iterator(&item, value_.arr.cbegin(), value_.arr.cend());
}

template<typename CharT, typename Alloc>
template<typename... Args>
auto basic_value<CharT, Alloc>::emplace(key_type key, Args&&... args) -> iterator {
    if (type_ != dtype::record) { init_as_record(); }
    typename record_t::alloc_type rec_al(*this);
    return iterator(value_.rec.emplace(rec_al, key, std::forward<Args>(args)...));
}

template<typename CharT, typename Alloc>
template<typename... Args>
auto basic_value<CharT, Alloc>::emplace_unique(key_type key, Args&&... args) -> std::pair<iterator, bool> {
    if (type_ != dtype::record) { init_as_record(); }
    typename record_t::alloc_type rec_al(*this);
    const auto result = value_.rec.emplace_unique(rec_al, key, std::forward<Args>(args)...);
    return std::make_pair(iterator(result.first), result.second);
}

template<typename CharT, typename Alloc>
template<typename InputIt, typename>
void basic_value<CharT, Alloc>::insert(std::size_t pos, InputIt first, InputIt last) {
    if (type_ != dtype::array) { init_as_array(); }
    typename value_array_t::alloc_type arr_al(*this);
    value_.arr.insert(arr_al, pos, first, last);
}

template<typename CharT, typename Alloc>
template<typename InputIt, typename>
void basic_value<CharT, Alloc>::insert(InputIt first, InputIt last) {
    typename record_t::alloc_type rec_al(*this);
    if (type_ != dtype::record) {
        if (type_ != dtype::null) { throw database_error("not a record"); }
        value_.rec.construct(rec_al, detail::initial_alloc_size(first, last, is_random_access_iterator<InputIt>()));
        type_ = dtype::record;
    }
    value_.rec.insert(rec_al, first, last);
}

// --------------------------

template<typename CharT, typename Alloc>
est::span<const basic_value<CharT, Alloc>> basic_value<CharT, Alloc>::as_array() const noexcept {
    if (type_ != dtype::array) { return type_ != dtype::null ? est::as_span(this, 1) : est::span<basic_value>(); }
    return value_.arr.cview();
}

template<typename CharT, typename Alloc>
est::span<basic_value<CharT, Alloc>> basic_value<CharT, Alloc>::as_array() {
    if (type_ != dtype::array) { return type_ != dtype::null ? est::as_span(this, 1) : est::span<basic_value>(); }
    typename value_array_t::alloc_type arr_al(*this);
    return value_.arr.view(arr_al);
}

template<typename CharT, typename Alloc>
auto basic_value<CharT, Alloc>::as_record() const -> iterator_range<const_record_iterator> {
    if (type_ != dtype::record) { throw database_error("not a record"); }
    return make_range(const_record_iterator(value_.rec.cbegin()), const_record_iterator(value_.rec.cend()));
}

template<typename CharT, typename Alloc>
auto basic_value<CharT, Alloc>::as_record() -> iterator_range<record_iterator> {
    if (type_ != dtype::record) { throw database_error("not a record"); }
    typename record_t::alloc_type rec_al(*this);
    value_.rec.unique(rec_al);
    return make_range(record_iterator(value_.rec.cbegin()), record_iterator(value_.rec.cend()));
}

// --------------------------

#define UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(ty, func) \
    template<typename CharT, typename Alloc> \
    ty basic_value<CharT, Alloc>::as##func() const { \
        const auto result = this->get##func(); \
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

template<typename, typename, typename>
struct value_getters_specializer;

#define UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(ty, func) \
    template<typename CharT, typename Alloc> \
    struct value_getters_specializer<CharT, Alloc, ty> { \
        static bool is(const basic_value<CharT, Alloc>& v) { return v.is##func(); } \
        static ty as(const basic_value<CharT, Alloc>& v) { return static_cast<ty>(v.as##func()); } \
        static est::optional<ty> get(const basic_value<CharT, Alloc>& v) { \
            const auto result = v.get##func(); \
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
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(std::basic_string_view<CharT>, _string_view)
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

// --------------------------

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
basic_value<CharT, Alloc> make_array() {
    return basic_value<CharT, Alloc>(array_construct_t{});
}

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
basic_value<CharT, Alloc> make_array(const Alloc& al) {
    return basic_value<CharT, Alloc>(array_construct_t{}, al);
}

template<typename CharT = char, typename Alloc = std::allocator<CharT>, typename InputIt,
         typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
basic_value<CharT, Alloc> make_array(InputIt first, InputIt last, const Alloc& al = Alloc()) {
    return basic_value<CharT, Alloc>(array_construct_t{}, first, last, al);
}

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
basic_value<CharT, Alloc> make_array(std::initializer_list<basic_value<CharT, Alloc>> init, const Alloc& al = Alloc()) {
    return basic_value<CharT, Alloc>(array_construct_t{}, init, al);
}

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
basic_value<CharT, Alloc> make_record() {
    return basic_value<CharT, Alloc>(record_construct_t{});
}

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
basic_value<CharT, Alloc> make_record(const Alloc& al) {
    return basic_value<CharT, Alloc>(record_construct_t{}, al);
}

template<typename CharT = char, typename Alloc = std::allocator<CharT>, typename InputIt,
         typename = std::enable_if_t<is_input_iterator<InputIt>::value &&
                                     detail::is_record_iterator<CharT, Alloc, InputIt>::value>>
basic_value<CharT, Alloc> make_record(InputIt first, InputIt last, const Alloc& al = Alloc()) {
    return basic_value<CharT, Alloc>(record_construct_t{}, first, last, al);
}

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
basic_value<CharT, Alloc> make_record(
    std::initializer_list<std::pair<typename basic_value<CharT, Alloc>::key_type, basic_value<CharT, Alloc>>> init,
    const Alloc& al = Alloc()) {
    return basic_value<CharT, Alloc>(record_construct_t{}, init, al);
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
