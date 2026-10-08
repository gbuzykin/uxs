#pragma once

#include "database_error.h"

#include "uxs/optional.h"
#include "uxs/span.h"
#include "uxs/string_util.h"

#include <atomic>
#include <cstring>
#include <functional>
#include <initializer_list>
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
    object,
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
        alignas(std::alignment_of<Ty>::value) std::uint8_t data_buf[1];
        Ty* data() noexcept { return reinterpret_cast<Ty*>(&data_buf); }
    };

 public:
    using const_view_type =
        std::conditional_t<est::is_character<Ty>::value, std::basic_string_view<Ty>, est::span<const Ty>>;
    using view_type = est::span<Ty>;
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<std::uintptr_t>;
    using alloc_traits = std::allocator_traits<alloc_type>;

    enum : unsigned { tail_zero = est::is_character<Ty>::value ? 1 : 0 };

    std::size_t size() const noexcept { return p_ ? p_->size : 0; }
    const_view_type cview() const noexcept { return p_ ? const_view_type(p_->data(), p_->size) : get_empty_view(); }

    template<typename Ty_ = Ty, typename = std::enable_if_t<est::is_character<Ty_>::value>>
    const Ty_* c_str() const noexcept {
        return p_ ? p_->data() : get_empty_view().data();
    }

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

    bool is_equal_to(flexarray_t other) const noexcept {
        if (p_ == other.p_) { return true; }
        const auto lhs = cview();
        const auto rhs = other.cview();
        return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
    }

    view_type view(alloc_type& al) {
        if (!p_) { return view_type(); }
        ensure_unique(al);
        return view_type(p_->data(), p_->size);
    }

    void construct_empty() noexcept { p_ = nullptr; }
    void construct_empty(alloc_type& al, std::size_t count) { p_ = count ? alloc_checked(al, count) : nullptr; }
    UXS_EXPORT void construct_from_view(alloc_type& al, const_view_type view, std::size_t extra);
    UXS_EXPORT void construct_fill_value(alloc_type& al, std::size_t count, const Ty& v);
    void construct_from_initializer(alloc_type& al, std::initializer_list<Ty> init) {
        construct_from_view(al, const_view_type(init.begin(), init.size()), 0);
    }

    template<typename InputIt>
    void construct_from_range(alloc_type& al, InputIt first, InputIt last) {
        if (first != last) {
            construct_dispatch(al, first, last, est::is_random_access_iterator<InputIt>());
        } else {
            p_ = nullptr;
        }
    }

    template<typename Ty_ = Ty, typename FillFn>
    void construct_fill(alloc_type& al, std::enable_if_t<std::is_trivially_copyable<Ty_>::value, std::size_t> max_count,
                        FillFn&& fn) {
        if (max_count) {
            p_ = alloc_checked(al, max_count + tail_zero);
            initialize_constructed(al, [this, max_count, &fn]() {
                p_->size = fn(est::as_span(p_->data(), max_count));
                put_tail_zero(p_->data() + p_->size);
            });
        } else {
            p_ = nullptr;
        }
    }

    UXS_EXPORT void assign_view(alloc_type& al, const_view_type view);
    UXS_EXPORT void append_view(alloc_type& al, const_view_type view);

    template<typename InputIt>
    void assign_range(alloc_type& al, InputIt first, InputIt last) {
        assign_dispatch(al, first, last, est::is_random_access_iterator<InputIt>());
    }

    template<typename InputIt>
    void append_range(alloc_type& al, InputIt first, InputIt last) {
        if (first == last) { return; }
        append_dispatch(al, first, last, est::is_random_access_iterator<InputIt>());
    }

    template<typename Ty_ = Ty, typename FillFn>
    void append_fill(alloc_type& al, std::enable_if_t<std::is_trivially_copyable<Ty_>::value, std::size_t> max_count,
                     FillFn&& fn) {
        if (!max_count) { return; }
        reserve(al, size() + max_count);
        try {
            p_->size += fn(est::as_span(p_->data() + p_->size, max_count));
            put_tail_zero(p_->data() + p_->size);
        } catch (...) {
            put_tail_zero(p_->data() + p_->size);
            throw;
        }
    }

    template<typename InputIt>
    void insert_range(alloc_type& al, std::size_t pos, InputIt first, InputIt last) {
        const std::size_t prev_sz = size();
        append_range(al, first, last);
        if (pos < prev_sz) { std::rotate(p_->data() + pos, p_->data() + prev_sz, p_->data() + p_->size); }
    }

    Ty& push_back(alloc_type& al, Ty&& v) {
        if (!p_ || p_->ref_count > 1 || p_->size + tail_zero == p_->capacity) { reserve(al, size() + 1); }
        Ty* item = p_->data() + p_->size;
        ::new (item) Ty(std::move(v));
        put_tail_zero(item + 1);
        ++p_->size;
        return *item;
    }

    void pop_back(alloc_type& al) {
        assert(p_ && p_->size);
        ensure_unique(al);
        --p_->size;
        Ty* item = p_->data() + p_->size;
        item->~Ty();
        put_tail_zero(item);
    }

    Ty& insert(alloc_type& al, std::size_t pos, Ty&& v) {
        push_back(al, std::move(v));
        if (pos < p_->size - 1) {
            rotate_back(pos);
        } else {
            pos = p_->size - 1;
        }
        return *(p_->data() + pos);
    }

    UXS_EXPORT void clear(alloc_type& al) noexcept;
    UXS_EXPORT void reserve(alloc_type& al, std::size_t size);
    UXS_EXPORT void resize(alloc_type& al, std::size_t size, const Ty& v);

    UXS_EXPORT Ty* erase(alloc_type& al, std::size_t pos);

    void ref() noexcept {
        if (p_) { ++p_->ref_count; }
    }

    void unref(alloc_type& al) noexcept {
        if (p_ && --p_->ref_count == 0) { destruct(al); }
    }

    void ensure_unique(alloc_type& al) {
        if (!p_ || p_->ref_count == 1) { return; }
        flexarray_t new_arr;
        new_arr.construct_from_view(al, const_view_type(p_->data(), p_->size), 0);
        reset(al, new_arr);
    }

 private:
    data_t* p_;

    static void put_tail_zero(Ty* p) noexcept { put_tail_zero_dispatch(p, est::is_character<Ty>()); }

    template<typename Ty_>
    static void put_tail_zero_dispatch(Ty_* p, std::true_type /* is character */) noexcept {
        *p = '\0';
    }

    template<typename Ty_>
    static void put_tail_zero_dispatch(Ty_* /*p*/, std::false_type /* is character */) noexcept {}

    static const_view_type get_empty_view() noexcept { return get_empty_view_dispatch(est::is_character<Ty>()); }

    template<typename Ty_ = Ty>
    static const_view_type get_empty_view_dispatch(std::true_type /* is character */) noexcept {
        static constexpr Ty empty_string[1] = {'\0'};
        return const_view_type(empty_string, 0);
    }

    template<typename Ty_ = Ty>
    static const_view_type get_empty_view_dispatch(std::false_type /* is character */) noexcept {
        return const_view_type();
    }

    template<typename InitFn>
    void initialize_constructed(alloc_type& al, InitFn&& fn) {
        try {
            fn();
        } catch (...) {
            destruct(al);
            throw;
        }
    }

    template<typename InputIt>
    void construct_dispatch(alloc_type& al, InputIt first, InputIt last, std::true_type /* random access iterator */);
    template<typename InputIt>
    void construct_dispatch(alloc_type& al, InputIt first, InputIt last, std::false_type /* random access iterator */);

    template<typename InputIt>
    void assign_no_realloc(InputIt first, std::size_t count);
    template<typename InputIt>
    void assign_dispatch(alloc_type& al, InputIt first, InputIt last, std::true_type /* random access iterator */);
    template<typename InputIt>
    void assign_dispatch(alloc_type& al, InputIt first, InputIt last, std::false_type /* random access iterator */);

    template<typename InputIt>
    void append_items_one_by_one(alloc_type& al, InputIt first, InputIt last);
    template<typename InputIt>
    void append_dispatch(alloc_type& al, InputIt first, InputIt last, std::true_type /* random access iterator */);
    template<typename InputIt>
    void append_dispatch(alloc_type& al, InputIt first, InputIt last, std::false_type /* random access iterator */);

    static void destruct_items(Ty* first, Ty* last) noexcept {
        destruct_items_dispatch(first, last, std::is_trivially_destructible<Ty>());
        put_tail_zero(first);
    }

    template<typename Ty_>
    static void destruct_items_dispatch(Ty_* /*first*/, Ty_* /*last*/,
                                        std::true_type /* trivially destructible */) noexcept {}

    template<typename Ty_>
    static void destruct_items_dispatch(Ty_* first, Ty_* last, std::false_type /* trivially destructible */) noexcept {
        for (; first != last; ++first) { first->~Ty(); }
    }

    template<typename InputIt>
    static void init_items_copy(Ty* dst, Ty* dst_last, InputIt first) {
        init_items_copy_dispatch(dst, dst_last, first, std::is_integral<Ty>(),
                                 std::is_nothrow_assignable<Ty&, decltype(*first)>());
        put_tail_zero(dst_last);
    }

    template<typename InputIt>
    static void init_items_copy_dispatch(Ty* dst, Ty* dst_last, InputIt first, std::true_type /* integral */,
                                         std::true_type /* nothrow assignable */) {
        std::copy_n(first, static_cast<std::size_t>(dst_last - dst), dst);
    }

    template<typename InputIt>
    static void init_items_copy_dispatch(Ty* dst, Ty* dst_last, InputIt first, std::false_type /* integral */,
                                         std::true_type /* nothrow assignable */) {
        for (; dst != dst_last; (void)++first, ++dst) { ::new (dst) Ty(*first); }
    }

    template<typename InputIt>
    static void init_items_copy_dispatch(Ty* dst, Ty* dst_last, InputIt first, std::false_type /* integral */,
                                         std::false_type /* nothrow assignable */) {
        Ty* dst0 = dst;
        try {
            for (; dst != dst_last; (void)++first, ++dst) { ::new (dst) Ty(*first); }
        } catch (...) {
            destruct_items(dst0, dst);
            throw;
        }
    }

    static void init_items_fill(Ty* dst, Ty* dst_last, const Ty& v) noexcept {
        init_items_fill_dispatch(dst, dst_last, v, std::is_trivially_copyable<Ty>());
        put_tail_zero(dst_last);
    }

    template<typename Ty_>
    static void init_items_fill_dispatch(Ty_* dst, Ty_* dst_last, const Ty& v,
                                         std::true_type /* trivially copyable */) noexcept {
        std::fill_n(dst, static_cast<std::size_t>(dst_last - dst), v);
    }

    template<typename Ty_>
    static void init_items_fill_dispatch(Ty_* dst, Ty_* dst_last, const Ty& v,
                                         std::false_type /* trivially copyable */) noexcept {
        for (; dst != dst_last; ++dst) { ::new (dst) Ty(v); }
    }

    UXS_EXPORT void grow(alloc_type& al, std::size_t extra);
    UXS_EXPORT void rotate_back(std::size_t pos) noexcept;
    UXS_EXPORT void destruct(alloc_type& al) noexcept;

    void reset(alloc_type& al, flexarray_t other) noexcept {
        unref(al);
        p_ = other.p_;
    }

    static std::size_t max_size(const alloc_type& al) noexcept {
        return (std::allocator_traits<alloc_type>::max_size(al) * sizeof(typename alloc_traits::value_type) -
                offsetof(data_t, data_buf)) /
               sizeof(Ty);
    }

    static std::size_t get_alloc_sz(std::size_t cap) noexcept {
        return (offsetof(data_t, data_buf) + cap * sizeof(Ty) + sizeof(typename alloc_traits::value_type) - 1) /
               sizeof(typename alloc_traits::value_type);
    }

    UXS_NODISCARD UXS_EXPORT static data_t* alloc(alloc_type& al, std::size_t size, std::size_t cap);
    UXS_NODISCARD static data_t* alloc_checked(alloc_type& al, std::size_t cap) {
        if (cap > max_size(al)) { report_too_much_to_allocate_error(); }
        return alloc(al, 0, cap);
    }

    static void dealloc(alloc_type& al, data_t* arr) noexcept {
        alloc_traits::deallocate(al, reinterpret_cast<typename alloc_traits::value_type*>(arr),
                                 get_alloc_sz(arr->capacity));
    }
};

template<typename Ty, typename Alloc>
template<typename InputIt>
void flexarray_t<Ty, Alloc>::construct_dispatch(alloc_type& al, InputIt first, InputIt last,
                                                std::true_type /* random access iterator */) {
    const std::size_t count = static_cast<std::size_t>(last - first);
    p_ = alloc_checked(al, count + tail_zero);
    initialize_constructed(al, [this, first, count]() {
        init_items_copy(p_->data(), p_->data() + count, first);
        p_->size = count;
    });
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void flexarray_t<Ty, Alloc>::construct_dispatch(alloc_type& al, InputIt first, InputIt last,
                                                std::false_type /* random access iterator */) {
    p_ = alloc(al, 0, 1 + tail_zero);
    initialize_constructed(al, [this, &al, first, last]() { append_items_one_by_one(al, first, last); });
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void flexarray_t<Ty, Alloc>::assign_no_realloc(InputIt first, std::size_t count) {
    Ty* dst = std::copy_n(first, std::min(count, p_->size), p_->data());
    if (count > p_->size) {
        init_items_copy(dst, p_->data() + count, first + p_->size);
    } else {
        destruct_items(dst, p_->data() + p_->size);
    }
    p_->size = count;
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void flexarray_t<Ty, Alloc>::assign_dispatch(alloc_type& al, InputIt first, InputIt last,
                                             std::true_type /* random access iterator */) {
    const std::size_t count = static_cast<std::size_t>(last - first);
    if (p_ && p_->ref_count == 1 && count + tail_zero <= p_->capacity) { return assign_no_realloc(first, count); }
    flexarray_t new_arr;
    new_arr.construct_from_range(al, first, last);
    reset(al, new_arr);
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void flexarray_t<Ty, Alloc>::assign_dispatch(alloc_type& al, InputIt first, InputIt last,
                                             std::false_type /* random access iterator */) {
    if (p_ && p_->ref_count == 1) {
        Ty* dst = p_->data();
        Ty* dst_last = p_->data() + p_->size;
        for (; dst != dst_last && first != last; (void)++first, ++dst) { *dst = *first; }
        if (dst == dst_last) { return append_items_one_by_one(al, first, last); }
        destruct_items(dst, dst_last);
        p_->size = static_cast<std::size_t>(dst - p_->data());
        return;
    }
    flexarray_t new_arr;
    new_arr.construct_from_range(al, first, last);
    reset(al, new_arr);
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void flexarray_t<Ty, Alloc>::append_items_one_by_one(alloc_type& al, InputIt first, InputIt last) {
    for (; first != last; (void)++first, ++p_->size) {
        Ty v(*first);
        if (p_->size + tail_zero == p_->capacity) { grow(al, 1 + tail_zero); }
        Ty* item = p_->data() + p_->size;
        ::new (item) Ty(std::move(v));
        put_tail_zero(item + 1);
    }
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void flexarray_t<Ty, Alloc>::append_dispatch(alloc_type& al, InputIt first, InputIt last,
                                             std::true_type /* random access iterator */) {
    const std::size_t count = static_cast<std::size_t>(last - first);
    reserve(al, size() + count);
    init_items_copy(p_->data() + p_->size, p_->data() + p_->size + count, first);
    p_->size += count;
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void flexarray_t<Ty, Alloc>::append_dispatch(alloc_type& al, InputIt first, InputIt last,
                                             std::false_type /* random access iterator */) {
    reserve(al, size() + 1);
    append_items_one_by_one(al, first, last);
}

}  // namespace detail

//-----------------------------------------------------------------------------
// Object container implementation
namespace detail {

struct list_links_t {
    list_links_t* next;
    list_links_t* prev;
#if UXS_ITERATOR_DEBUG_LEVEL != 0
    list_links_t* head;
#endif  // UXS_ITERATOR_DEBUG_LEVEL != 0
};

template<typename CharT, typename Alloc>
class object_t;

template<typename CharT, typename Alloc>
class object_node_handle;

template<typename CharT, typename Alloc>
class object_item {
 public:
    using char_type = CharT;
    using key_type = std::basic_string_view<char_type>;
    using value_type = basic_value<CharT, Alloc>;
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<std::uintptr_t>;
    using alloc_traits = std::allocator_traits<alloc_type>;

    object_item(const object_item&) = delete;
    object_item& operator=(const object_item&) = delete;

    key_type key() const noexcept { return key_type(key_chars(), key_sz_); }
    const value_type& value() const& noexcept { return *reinterpret_cast<const value_type*>(&x_); }
    value_type& value() & noexcept { return *reinterpret_cast<value_type*>(&x_); }
    value_type&& value() && noexcept { return std::move(*reinterpret_cast<value_type*>(&x_)); }

    friend bool operator==(const object_item& lhs, const object_item& rhs) noexcept {
        return lhs.key() == rhs.key() && lhs.value() == rhs.value();
    }
    friend bool operator!=(const object_item& lhs, const object_item& rhs) noexcept { return !(lhs == rhs); }

    static object_item* from_links(list_links_t* links) noexcept {
        return get_containing_record<object_item, offsetof(object_item, links_)>(links);
    }

 private:
    friend class object_t<CharT, Alloc>;
    friend class object_node_handle<CharT, Alloc>;

    list_links_t links_;
    list_links_t* next_bucket_;
    std::size_t hash_code_;
    alignas(std::alignment_of<value_type>::value) std::uint8_t x_[sizeof(value_type)];
    std::size_t key_sz_;
    std::size_t key_chars_cap_;
    char_type key_chars_buf_[1];

    const char_type* key_chars() const noexcept { return key_chars_buf_; }
    char_type* key_chars() noexcept { return key_chars_buf_; }

    enum : std::size_t { min_char_count = 2 * sizeof(std::uintptr_t) / sizeof(CharT) };

    UXS_NODISCARD static object_item* construct(alloc_type& al, key_type key, value_type&& v) {
        object_item* node = alloc(al, key);
        ::new (&node->value()) value_type(std::move(v));
        return node;
    }

    template<typename FillFn>
    UXS_NODISCARD static object_item* construct(alloc_type& al, std::size_t max_key_length, FillFn&& fill_key_fn,
                                                value_type&& v) {
        if (max_key_length + 1 > max_name_alloc_cap(al)) { report_too_much_to_allocate_error(); }
        const std::size_t alloc_sz = get_alloc_sz(std::max<std::size_t>(max_key_length + 1, min_char_count));
        object_item* node = reinterpret_cast<object_item*>(alloc_traits::allocate(al, alloc_sz));
        node->key_chars_cap_ = (alloc_sz * sizeof(typename alloc_traits::value_type) -
                                offsetof(object_item, key_chars_buf_)) /
                               sizeof(char_type);
        try {
            node->key_sz_ = fill_key_fn(est::as_span(node->key_chars(), max_key_length));
            node->key_chars()[node->key_sz_] = '\0';
            ::new (&node->value()) value_type(std::move(v));
            return node;
        } catch (...) {
            dealloc(al, node);
            throw;
        }
    }

    bool set_key(key_type key) {
        if (key.size() + 1 > key_chars_cap_) { return false; }
        key_sz_ = key.size();
        std::memcpy(key_chars(), key.data(), key.size() * sizeof(CharT));
        key_chars()[key_sz_] = '\0';
        return true;
    }

    template<typename FillFn>
    bool set_key(std::size_t max_key_length, FillFn&& fill_key_fn) {
        if (max_key_length + 1 > key_chars_cap_) { return false; }
        try {
            key_sz_ = fill_key_fn(est::as_span(key_chars(), max_key_length));
            key_chars()[key_sz_] = '\0';
            return true;
        } catch (...) {
            key_sz_ = 0;
            key_chars()[key_sz_] = '\0';
            throw;
        }
    }

    static void destroy(alloc_type& al, object_item* node) noexcept {
        node->value().~value_type();
        dealloc(al, node);
    }

    static std::size_t max_name_alloc_cap(const alloc_type& al) noexcept {
        return (std::allocator_traits<alloc_type>::max_size(al) * sizeof(typename alloc_traits::value_type) -
                offsetof(object_item, key_chars_buf_)) /
               sizeof(char_type);
    }

    static std::size_t get_alloc_sz(std::size_t key_sz) noexcept {
        return (offsetof(object_item, key_chars_buf_) + key_sz * sizeof(char_type) +
                sizeof(typename alloc_traits::value_type) - 1) /
               sizeof(typename alloc_traits::value_type);
    }

    UXS_NODISCARD UXS_EXPORT static object_item* alloc(alloc_type& al, key_type key);

    static void dealloc(alloc_type& al, object_item* node) noexcept {
        alloc_traits::deallocate(al, reinterpret_cast<typename alloc_traits::value_type*>(node),
                                 get_alloc_sz(node->key_chars_cap_));
    }
};

template<std::size_t I, typename CharT, typename Alloc, typename = std::enable_if_t<I == 0>>
auto get(const object_item<CharT, Alloc>& v) noexcept -> decltype(v.key()) {
    return v.key();
}
template<std::size_t I, typename CharT, typename Alloc, typename = std::enable_if_t<I == 1>>
auto get(const object_item<CharT, Alloc>& v) noexcept -> decltype(v.value()) {
    return v.value();
}
template<std::size_t I, typename CharT, typename Alloc, typename = std::enable_if_t<I == 1>>
auto get(object_item<CharT, Alloc>& v) noexcept -> decltype(v.value()) {
    return v.value();
}
template<std::size_t I, typename CharT, typename Alloc, typename = std::enable_if_t<I == 1>>
auto get(object_item<CharT, Alloc>&& v) noexcept -> decltype(std::move(v).value()) {
    return std::move(v).value();
}

template<typename Alloc>
using object_node_allocator_t = std::conditional_t<
    std::is_empty<Alloc>::value && std::is_nothrow_default_constructible<Alloc>::value, Alloc, est::optional<Alloc>>;

template<typename CharT, typename Alloc>
class object_node_handle
    : protected object_node_allocator_t<typename std::allocator_traits<Alloc>::template rebind_alloc<std::uintptr_t>> {
 private:
    using node_t = object_item<CharT, Alloc>;
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<std::uintptr_t>;
    using super = object_node_allocator_t<alloc_type>;
    using alloc_traits = std::allocator_traits<alloc_type>;

 public:
    using char_type = CharT;
    using allocator_type = Alloc;
    using key_type = std::basic_string_view<char_type>;
    using value_type = basic_value<CharT, Alloc>;
    using size_type = std::size_t;

    object_node_handle() noexcept = default;
    object_node_handle(object_node_handle&& other) noexcept : super(std::move(other)), node_(other.node_) {
        other.node_ = nullptr;
    }
    ~object_node_handle() {
        if (node_) { node_t::destroy(get_allocator(), node_); }
    }

    object_node_handle& operator=(const object_node_handle&) = delete;

    explicit operator bool() const noexcept { return node_; }

    void set_key(key_type key) {
        assert(node_);
        if (node_->set_key(key)) { return; }
        node_t* node = node_t::construct(get_allocator(), key, std::move(node_->value()));
        node_t::destroy(get_allocator(), node_);
        node_ = node;
    }

    template<typename FillFn>
    void set_key(size_type max_key_length, FillFn&& fill_key_fn) {
        assert(node_);
        if (node_->set_key(max_key_length, std::forward<FillFn>(fill_key_fn))) { return; }
        node_t* node = node_t::construct(get_allocator(), max_key_length, std::forward<FillFn>(fill_key_fn),
                                         std::move(node_->value()));
        node_t::destroy(get_allocator(), node_);
        node_ = node;
    }

    const node_t& operator*() const {
        assert(node_);
        return *node_;
    }

    node_t& operator*() {
        assert(node_);
        return *node_;
    }

    const node_t* operator->() const noexcept { return std::addressof(**this); }
    node_t* operator->() noexcept { return std::addressof(**this); }

 private:
    friend class object_t<CharT, Alloc>;
    friend class basic_value<CharT, Alloc>;

    alloc_type& get_allocator() { return get_allocator_dispatch(std::is_same<super, alloc_type>()); }

    template<typename Alloc_ = Alloc>
    alloc_type& get_allocator_dispatch(std::true_type) {
        return *this;
    }

    template<typename Alloc_ = Alloc>
    alloc_type& get_allocator_dispatch(std::false_type) {
        return *static_cast<super&>(*this);
    }

    object_node_handle(const alloc_type& al, node_t* node) noexcept : super(al), node_(node) {}

    object_node_handle(const alloc_type& al, key_type key, value_type&& v)
        : super(al), node_(node_t::construct(get_allocator(), key, std::move(v))) {}

    template<typename FillFn>
    object_node_handle(const alloc_type& al, size_type max_key_length, FillFn&& fill_key_fn, value_type&& v)
        : super(al),
          node_(node_t::construct(get_allocator(), max_key_length, std::forward<FillFn>(fill_key_fn), std::move(v))) {}

    node_t* release(const alloc_type& al) && noexcept {
        check_allocator_dispatch(al, typename alloc_traits::is_always_equal());
        node_t* node = node_;
        node_ = nullptr;
        return node;
    }

    template<typename CharT_ = CharT>
    void check_allocator_dispatch(const alloc_type& /*al*/, std::true_type /* always equal allocators */) const {}

    template<typename CharT_ = CharT>
    void check_allocator_dispatch(const alloc_type& al, std::false_type /* always equal allocators */) const {
        if (*static_cast<const super&>(*this) != al) { throw database_error("incompatible allocators"); }
    }

    node_t* node_ = nullptr;
};

template<typename CharT, typename Alloc>
struct object_node_traits {
    using iterator_node_t = list_links_t;
    using node_t = object_item<CharT, Alloc>;
    static list_links_t* get_next(list_links_t* node) { return node->next; }
    static list_links_t* get_prev(list_links_t* node) { return node->prev; }
#if UXS_ITERATOR_DEBUG_LEVEL != 0
    static void set_head(list_links_t* node, list_links_t* head) { node->head = head; }
    static list_links_t* get_head(list_links_t* node) { return node->head; }
    static list_links_t* get_front(list_links_t* head) { return head->next; }
#else   // UXS_ITERATOR_DEBUG_LEVEL != 0
    static void set_head(list_links_t* /*node*/, list_links_t* /*head*/) {}
#endif  // UXS_ITERATOR_DEBUG_LEVEL != 0
    static object_item<CharT, Alloc>& get_value(list_links_t* node) { return *node_t::from_links(node); }
};

template<typename Iter, typename ValueType>
struct append_return_type {
    Iter position;
    bool appended;
    ValueType value;
};

template<typename CharT, typename Alloc>
class object_t {
 private:
    struct data_t {
        std::atomic<std::size_t> ref_count;
        list_links_t head;
        std::size_t size;
        std::size_t bucket_count;
        list_links_t* data_buf[1];
        list_links_t** hashtbl() noexcept { return data_buf; }
        UXS_EXPORT void init() noexcept;
        void init_from(object_t other) noexcept;
    };

 public:
    using key_type = std::basic_string_view<CharT>;
    using value_type = basic_value<CharT, Alloc>;
    using node_handle = object_node_handle<CharT, Alloc>;
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<std::uintptr_t>;
    using alloc_traits = std::allocator_traits<alloc_type>;
    using node_traits = object_node_traits<CharT, Alloc>;
    using node_t = typename node_traits::node_t;
    using hasher_t = std::hash<key_type>;

    struct iterator_traits : node_traits {
        using value_type = object_item<CharT, Alloc>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;
    };

    using iterator = est::list_iterator<iterator_traits, false>;
    using const_iterator = est::list_iterator<iterator_traits, true>;

    std::size_t size() const noexcept { return p_->size; }
    list_links_t* cbegin() const noexcept { return p_->head.next; }
    list_links_t* cend() const noexcept { return &p_->head; }
    list_links_t* cfind(key_type key) const noexcept { return find_impl(key, nullptr); }
    UXS_EXPORT std::size_t count(key_type key) const noexcept;

    bool is_equal_to(object_t other) const noexcept {
        if (p_ == other.p_) { return true; }
        return size() == other.size() &&
               std::equal(const_iterator(cbegin()), const_iterator(cend()), const_iterator(other.cbegin()));
    }

    void construct_empty(alloc_type& al) {
        p_ = alloc(al, 0);
        p_->init();
    }

    void construct_empty(alloc_type& al, std::size_t bucket_count) {
        if (bucket_count > max_size(al)) { report_too_much_to_allocate_error(); }
        p_ = alloc(al, bucket_count);
        p_->init();
    }

    UXS_EXPORT void construct_copy(alloc_type& al, object_t other, std::size_t extra);
    void construct_from_common_initializer(alloc_type& al, std::initializer_list<value_type> init);
    UXS_EXPORT void construct_from_initializer(alloc_type& al,
                                               std::initializer_list<std::pair<key_type, value_type>> init);

    template<typename InputIt>
    void construct_from_range(alloc_type& al, InputIt first, InputIt last) {
        construct_dispatch(al, first, last, est::is_random_access_iterator<InputIt>());
    }

    void assign_common_initializer(alloc_type& al, std::initializer_list<value_type> init);
    void append_common_initializer(alloc_type& al, std::initializer_list<value_type> init);

    template<typename InputIt>
    void assign_range(alloc_type& al, InputIt first, InputIt last) {
        assign_dispatch(al, first, last, est::is_random_access_iterator<InputIt>());
    }

    template<typename InputIt>
    void append_range(alloc_type& al, InputIt first, InputIt last) {
        if (first == last) { return; }
        append_dispatch(al, first, last, est::is_random_access_iterator<InputIt>());
    }

    list_links_t* append_new(alloc_type& al, key_type key, value_type&& v) {
        if (p_->ref_count > 1 || p_->size == p_->bucket_count) { reserve(al, p_->size + 1); }
        node_t* node = node_t::construct(al, key, std::move(v));
        append_node(node, nullptr);
        return &node->links_;
    }

    list_links_t* append_new(alloc_type& al, node_handle&& h) {
        if (p_->ref_count > 1 || p_->size == p_->bucket_count) { reserve(al, p_->size + 1); }
        node_t* node = std::move(h).release(al);
        append_node(node, nullptr);
        return &node->links_;
    }

    append_return_type<list_links_t*, value_type> append_unique(alloc_type& al, key_type key, value_type&& v) {
        if (p_->ref_count > 1 || p_->size == p_->bucket_count) { reserve(al, p_->size + 1); }
        std::size_t hash_code = 0;
        list_links_t* found_node = find_impl(key, &hash_code);
        if (found_node != cend()) { return {found_node, false, std::move(v)}; }
        node_t* node = node_t::construct(al, key, std::move(v));
        append_node(node, &hash_code);
        return {&node->links_, true, {}};
    }

    append_return_type<list_links_t*, node_handle> append_unique(alloc_type& al, node_handle&& h) {
        if (!h.node_) { return {cend(), false, {}}; }
        if (p_->ref_count > 1 || p_->size == p_->bucket_count) { reserve(al, p_->size + 1); }
        std::size_t hash_code = 0;
        list_links_t* found_node = find_impl(h.node_->key(), &hash_code);
        if (found_node != cend()) { return {found_node, false, std::move(h)}; }
        node_t* node = std::move(h).release(al);
        append_node(node, &hash_code);
        return {&node->links_, true, {}};
    }

    template<typename... Args>
    std::pair<list_links_t*, bool> try_append_unique(alloc_type& al, key_type key, Args&&... args) {
        if (p_->ref_count > 1 || p_->size == p_->bucket_count) { reserve(al, p_->size + 1); }
        std::size_t hash_code = 0;
        list_links_t* found_node = find_impl(key, &hash_code);
        if (found_node != cend()) { return {found_node, false}; }
        node_t* node = node_t::construct(al, key, value_type(std::forward<Args>(args)...));
        append_node(node, &hash_code);
        return {&node->links_, true};
    }

    UXS_EXPORT void clear(alloc_type& al);
    UXS_EXPORT void reserve(alloc_type& al, std::size_t size);
    UXS_EXPORT std::size_t erase(alloc_type& al, key_type key);

    list_links_t* erase(alloc_type& al, list_links_t* node) {
        const auto result = extract_impl(al, node);
        node_t::destroy(al, result.first);
        return result.second;
    }

    std::pair<node_handle, list_links_t*> extract(alloc_type& al, list_links_t* node) {
        const auto result = extract_impl(al, node);
        return {node_handle(al, result.first), result.second};
    }

    void ref() noexcept { ++p_->ref_count; }

    void unref(alloc_type& al) noexcept {
        if (--p_->ref_count == 0) { destruct(al); }
    }

    void ensure_unique(alloc_type& al) {
        if (p_->ref_count == 1) { return; }
        object_t new_obj;
        new_obj.construct_copy(al, *this, 0);
        reset(al, new_obj);
    }

 private:
    data_t* p_;

    template<typename InitFn>
    void initialize_constructed(alloc_type& al, InitFn&& fn) {
        try {
            fn();
        } catch (...) {
            destruct(al);
            throw;
        }
    }

    template<typename InputIt>
    void construct_dispatch(alloc_type& al, InputIt first, InputIt last, std::true_type /* random access iterator */);
    template<typename InputIt>
    void construct_dispatch(alloc_type& al, InputIt first, InputIt last, std::false_type /* random access iterator */);

    template<typename InputIt>
    void assign_dispatch(alloc_type& al, InputIt first, InputIt last, std::true_type /* random access iterator */);
    template<typename InputIt>
    void assign_dispatch(alloc_type& al, InputIt first, InputIt last, std::false_type /* random access iterator */);

    template<typename InputIt>
    void append_no_realloc(alloc_type& al, InputIt first, InputIt last) {
        append_no_realloc_dispatch(al, first, last, std::is_same<est::array_element_t<InputIt>, node_handle>());
    }
    template<typename InputIt>
    void append_no_realloc_dispatch(alloc_type& al, InputIt first, InputIt last, std::true_type /* node handles */);
    template<typename InputIt>
    void append_no_realloc_dispatch(alloc_type& al, InputIt first, InputIt last, std::false_type /* node handles */);
    void append_common_initializer_no_realloc(alloc_type& al, std::initializer_list<value_type> init);
    template<typename InputIt>
    void append_one_by_one(alloc_type& al, InputIt first, InputIt last) {
        append_one_by_one_dispatch(al, first, last, std::is_same<est::array_element_t<InputIt>, node_handle>());
    }
    template<typename InputIt>
    void append_one_by_one_dispatch(alloc_type& al, InputIt first, InputIt last, std::true_type /* node handles */);
    template<typename InputIt>
    void append_one_by_one_dispatch(alloc_type& al, InputIt first, InputIt last, std::false_type /* node handles */);
    template<typename InputIt>
    void append_dispatch(alloc_type& al, InputIt first, InputIt last, std::true_type /* random access iterator */);
    template<typename InputIt>
    void append_dispatch(alloc_type& al, InputIt first, InputIt last, std::false_type /* random access iterator */);

    UXS_EXPORT void destruct_items(alloc_type& al) noexcept;
    void add_to_hash(node_t* node) noexcept;
    UXS_EXPORT void append_node(node_t* node, const std::size_t* p_hash_code) noexcept;
    UXS_EXPORT void rehash(alloc_type& al, std::size_t extra);
    UXS_EXPORT void destruct(alloc_type& al) noexcept;
    UXS_EXPORT list_links_t* find_impl(key_type key, std::size_t* p_hash_code) const noexcept;

    void reset(alloc_type& al, object_t other) noexcept {
        unref(al);
        p_ = other.p_;
    }

    list_links_t* map_node(object_t other, list_links_t* node_to_map) noexcept;

    UXS_EXPORT std::pair<node_t*, list_links_t*> extract_impl(alloc_type& al, list_links_t* node);

    static std::size_t max_size(const alloc_type& al) noexcept {
        return (std::allocator_traits<alloc_type>::max_size(al) * sizeof(typename alloc_traits::value_type) -
                offsetof(data_t, data_buf)) /
               sizeof(list_links_t*);
    }

    static std::size_t get_alloc_sz(std::size_t bucket_count) noexcept {
        return (offsetof(data_t, data_buf) + bucket_count * sizeof(list_links_t*) +
                sizeof(typename alloc_traits::value_type) - 1) /
               sizeof(typename alloc_traits::value_type);
    }

    UXS_NODISCARD UXS_EXPORT static data_t* alloc(alloc_type& al, std::size_t bucket_count);

    static void dealloc(alloc_type& al, data_t* obj) noexcept {
        alloc_traits::deallocate(al, reinterpret_cast<typename alloc_traits::value_type*>(obj),
                                 get_alloc_sz(obj->bucket_count));
    }
};

template<typename Ty, typename Alloc>
template<typename InputIt>
void object_t<Ty, Alloc>::construct_dispatch(alloc_type& al, InputIt first, InputIt last,
                                             std::true_type /* random access iterator */) {
    construct_empty(al, static_cast<std::size_t>(last - first));
    initialize_constructed(al, [this, &al, first, last]() { append_no_realloc(al, first, last); });
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void object_t<Ty, Alloc>::construct_dispatch(alloc_type& al, InputIt first, InputIt last,
                                             std::false_type /* random access iterator */) {
    construct_empty(al);
    initialize_constructed(al, [this, &al, first, last]() { append_one_by_one(al, first, last); });
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void object_t<Ty, Alloc>::assign_dispatch(alloc_type& al, InputIt first, InputIt last,
                                          std::true_type /* random access iterator */) {
    const std::size_t count = static_cast<std::size_t>(last - first);
    if (p_->ref_count == 1 && count <= p_->bucket_count) {
        destruct_items(al);
        p_->init();
        return append_no_realloc(al, first, last);
    }
    object_t new_obj;
    new_obj.construct_from_range(al, first, last);
    reset(al, new_obj);
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void object_t<Ty, Alloc>::assign_dispatch(alloc_type& al, InputIt first, InputIt last,
                                          std::false_type /* random access iterator */) {
    if (p_->ref_count == 1) {
        destruct_items(al);
        p_->init();
        return append_one_by_one(al, first, last);
    }
    object_t new_obj;
    new_obj.construct_from_range(al, first, last);
    reset(al, new_obj);
}

template<typename CharT, typename Alloc>
template<typename InputIt>
void object_t<CharT, Alloc>::append_no_realloc_dispatch(alloc_type& al, InputIt first, InputIt last,
                                                        std::true_type /* node handles */) {
    for (; first != last; ++first) { append_node((*first).release(al), nullptr); }
}

template<typename CharT, typename Alloc>
template<typename InputIt>
void object_t<CharT, Alloc>::append_no_realloc_dispatch(alloc_type& al, InputIt first, InputIt last,
                                                        std::false_type /* node handles */) {
    for (; first != last; ++first) {
        const auto key = std::get<0>(*first);
        append_node(node_t::construct(al, key, value_type(std::get<1>(*first))), nullptr);
    }
}

template<typename CharT, typename Alloc>
template<typename InputIt>
void object_t<CharT, Alloc>::append_one_by_one_dispatch(alloc_type& al, InputIt first, InputIt last,
                                                        std::true_type /* node handles */) {
    for (; first != last; ++first) {
        if (p_->size == p_->bucket_count) { rehash(al, 1); }
        append_node((*first).release(al), nullptr);
    }
}

template<typename CharT, typename Alloc>
template<typename InputIt>
void object_t<CharT, Alloc>::append_one_by_one_dispatch(alloc_type& al, InputIt first, InputIt last,
                                                        std::false_type /* node handles */) {
    for (; first != last; ++first) {
        if (p_->size == p_->bucket_count) { rehash(al, 1); }
        const auto key = std::get<0>(*first);
        append_node(node_t::construct(al, key, value_type(std::get<1>(*first))), nullptr);
    }
}

template<typename CharT, typename Alloc>
template<typename InputIt>
void object_t<CharT, Alloc>::append_dispatch(alloc_type& al, InputIt first, InputIt last,
                                             std::true_type /* random access iterator */) {
    reserve(al, p_->size + static_cast<std::size_t>(last - first));
    append_no_realloc(al, first, last);
}

template<typename CharT, typename Alloc>
template<typename InputIt>
void object_t<CharT, Alloc>::append_dispatch(alloc_type& al, InputIt first, InputIt last,
                                             std::false_type /* random access iterator */) {
    reserve(al, p_->size + 1);
    append_one_by_one(al, first, last);
}

//-----------------------------------------------------------------------------
// Universal value iterator

template<typename CharT, typename Alloc, bool Const>
class value_iterator_proxy {
 public:
    using char_type = CharT;
    using key_type = std::basic_string_view<char_type>;
    using value_type = basic_value<CharT, Alloc>;

    value_iterator_proxy(void* ptr, const void* begin) noexcept : ptr_(ptr), begin_(begin) {}
#if __cplusplus >= 201703L
    value_iterator_proxy(const value_iterator_proxy&) = delete;
#else   // __cplusplus >= 201703L
    value_iterator_proxy(const value_iterator_proxy& other) noexcept : ptr_(other.ptr_), begin_(other.begin_) {}
#endif  // __cplusplus >= 201703L
    value_iterator_proxy& operator=(const value_iterator_proxy&) = delete;

    bool is_object() const noexcept { return !begin_; }

    key_type key() const noexcept {
        if (is_object()) { return get_object_item().key(); }
        if (!index_cached_) { create_index_string(); }
        return key_type(index_string_, index_string_len_);
    }

    const char_type* c_key() const noexcept { return key().data(); }

    std::conditional_t<Const, const value_type&, value_type&> value() const noexcept {
        return is_object() ? get_object_item().value() : *static_cast<value_type*>(ptr_);
    }

 private:
    void* ptr_ = nullptr;
    const void* begin_ = nullptr;

    mutable std::atomic_bool index_cached_{false};
    mutable std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    mutable std::uint8_t index_string_len_;
    mutable char_type index_string_[21];

    object_item<CharT, Alloc>& get_object_item() const noexcept {
        return *object_item<CharT, Alloc>::from_links(static_cast<list_links_t*>(ptr_));
    }

    UXS_EXPORT void create_index_string() const noexcept;
};

template<typename CharT, typename Alloc, bool Const>
class value_iterator
    : public est::input_iterator_facade<value_iterator<CharT, Alloc, Const>, basic_value<CharT, Alloc>,
                                        std::bidirectional_iterator_tag, value_iterator_proxy<CharT, Alloc, Const>,
                                        value_iterator_proxy<CharT, Alloc, Const>&&> {
 public:
    using value_type = basic_value<CharT, Alloc>;

    value_iterator() noexcept = default;
#if UXS_ITERATOR_DEBUG_LEVEL != 0
    value_iterator(value_type* ptr, const value_type* begin, const value_type* end) noexcept
        : ptr_(ptr), begin_(begin), end_(end) {}
    value_iterator(const value_iterator& other) noexcept : ptr_(other.ptr_), begin_(other.begin_), end_(other.end_) {}
    value_iterator& operator=(const value_iterator& other) noexcept {
        ptr_ = other.ptr_, begin_ = other.begin_, end_ = other.end_;
        return *this;
    }
    template<bool Const_ = Const>
    value_iterator(const std::enable_if_t<Const_, value_iterator<CharT, Alloc, false>>& other) noexcept
        : ptr_(other.ptr_), begin_(other.begin_), end_(other.end_) {}
    template<bool Const_ = Const>
    value_iterator& operator=(const std::enable_if_t<Const_, value_iterator<CharT, Alloc, false>>& other) noexcept {
        ptr_ = other.ptr_, begin_ = other.begin_, end_ = other.end_;
        return *this;
    }
#else   // UXS_ITERATOR_DEBUG_LEVEL != 0
    value_iterator(value_type* ptr, const value_type* begin, const value_type* /*end*/) noexcept
        : ptr_(ptr), begin_(begin) {}
    value_iterator(const value_iterator& other) noexcept : ptr_(other.ptr_), begin_(other.begin_) {}
    value_iterator& operator=(const value_iterator& other) noexcept {
        ptr_ = other.ptr_, begin_ = other.begin_;
        return *this;
    }
    template<bool Const_ = Const>
    value_iterator(const std::enable_if_t<Const_, value_iterator<CharT, Alloc, false>>& other) noexcept
        : ptr_(other.ptr_), begin_(other.begin_) {}
    template<bool Const_ = Const>
    value_iterator& operator=(const std::enable_if_t<Const_, value_iterator<CharT, Alloc, false>>& other) noexcept {
        ptr_ = other.ptr_, begin_ = other.begin_;
        return *this;
    }
#endif  // UXS_ITERATOR_DEBUG_LEVEL != 0

    explicit value_iterator(list_links_t* node) noexcept : ptr_(node) {}
    value_iterator(const est::list_iterator<typename object_t<CharT, Alloc>::iterator_traits, Const>& other) noexcept
        : ptr_(other.node()) {}
    value_iterator& operator=(
        const est::list_iterator<typename object_t<CharT, Alloc>::iterator_traits, Const>& other) noexcept {
#if UXS_ITERATOR_DEBUG_LEVEL != 0
        ptr_ = other.node(), begin_ = nullptr, end_ = nullptr;
#else   // UXS_ITERATOR_DEBUG_LEVEL != 0
        ptr_ = other.node(), begin_ = nullptr;
#endif  // UXS_ITERATOR_DEBUG_LEVEL != 0
        return *this;
    }

    template<bool Const_ = Const>
    value_iterator(
        const std::enable_if_t<Const_, est::list_iterator<typename object_t<CharT, Alloc>::iterator_traits, false>>&
            other) noexcept
        : ptr_(other.node()) {}
    template<bool Const_ = Const>
    value_iterator& operator=(
        const std::enable_if_t<Const_, est::list_iterator<typename object_t<CharT, Alloc>::iterator_traits, false>>&
            other) noexcept {
#if UXS_ITERATOR_DEBUG_LEVEL != 0
        ptr_ = other.node(), begin_ = nullptr, end_ = nullptr;
#else   // UXS_ITERATOR_DEBUG_LEVEL != 0
        ptr_ = other.node(), begin_ = nullptr;
#endif  // UXS_ITERATOR_DEBUG_LEVEL != 0
        return *this;
    }

    void increment() noexcept {
        assert(ptr_);
        uxs_iterator_assert(is_object() ? ptr_ != get_head() : begin_ <= ptr_ && ptr_ < end_);
        ptr_ = is_object() ? static_cast<void*>(static_cast<list_links_t*>(ptr_)->next) :
                             static_cast<void*>(static_cast<value_type*>(ptr_) + 1);
    }

    void decrement() noexcept {
        assert(ptr_);
        uxs_iterator_assert(is_object() ? ptr_ != get_head()->next : begin_ < ptr_ && ptr_ <= end_);
        ptr_ = is_object() ? static_cast<void*>(static_cast<list_links_t*>(ptr_)->prev) :
                             static_cast<void*>(static_cast<value_type*>(ptr_) - 1);
    }

    template<bool ConstOther>
    bool is_equal_to(const value_iterator<CharT, Alloc, ConstOther>& other) const noexcept {
        assert(is_object() == other.is_object());
        assert(!is_object() || (!ptr_ && !other.ptr_) || (ptr_ && other.ptr_));
        uxs_iterator_assert(is_object() ? !ptr_ || get_head() == other.get_head() :
                                          begin_ == other.begin_ && end_ == other.end_);
        return ptr_ == other.ptr_;
    }

    value_iterator_proxy<CharT, Alloc, Const> dereference() const noexcept {
        assert(ptr_);
        uxs_iterator_assert(is_object() ? ptr_ != get_head() : begin_ <= ptr_ && ptr_ < end_);
        return {ptr_, begin_};
    }

 private:
    template<typename CharT_, typename Alloc_, bool Const_>
    friend class value_iterator;
    friend class basic_value<CharT, Alloc>;

    void* ptr_ = nullptr;
    const void* begin_ = nullptr;
#if UXS_ITERATOR_DEBUG_LEVEL != 0
    const void* end_ = nullptr;
    list_links_t* get_head() const noexcept { return static_cast<list_links_t*>(ptr_)->head; }
#endif  // UXS_ITERATOR_DEBUG_LEVEL != 0

    bool is_object() const noexcept { return !begin_; }
};

template<std::size_t I, typename CharT, typename Alloc, bool Const, typename = std::enable_if_t<I == 0>>
auto get(const value_iterator_proxy<CharT, Alloc, Const>& v) noexcept -> decltype(v.key()) {
    return v.key();
}
template<std::size_t I, typename CharT, typename Alloc, bool Const, typename = std::enable_if_t<I == 1>>
auto get(const value_iterator_proxy<CharT, Alloc, Const>& v) noexcept -> decltype(v.value()) {
    return v.value();
}

}  // namespace detail

//-----------------------------------------------------------------------------
// uxs::basic_value<> implementation

struct scalar_tag_t {
    explicit constexpr scalar_tag_t(int) {}
};
struct string_tag_t {
    explicit constexpr string_tag_t(int) {}
};
struct array_tag_t {
    explicit constexpr array_tag_t(int) {}
};
struct object_tag_t {
    explicit constexpr object_tag_t(int) {}
};

constexpr scalar_tag_t scalar_tag{0};
constexpr string_tag_t string_tag{0};
constexpr array_tag_t array_tag{0};
constexpr object_tag_t object_tag{0};

namespace detail {

template<typename CharT, typename Alloc, typename Ty, typename = void>
struct is_object_item : std::false_type {};
template<typename CharT, typename Alloc>
struct is_object_item<CharT, Alloc, object_node_handle<CharT, Alloc>> : std::true_type {};
template<typename CharT, typename Alloc, typename Ty>
struct is_object_item<
    CharT, Alloc, Ty,
    std::enable_if_t<
        std::is_convertible<typename std::tuple_element<0, Ty>::type, typename object_t<CharT, Alloc>::key_type>::value &&
        std::is_convertible<typename std::tuple_element<1, Ty>::type, basic_value<CharT, Alloc>>::value>>
    : std::true_type {};

template<typename CharT, typename Alloc, typename InputIt>
using is_object_iterator = is_object_item<CharT, Alloc, typename std::iterator_traits<InputIt>::value_type>;

template<typename CharT, typename Alloc, typename InputIt>
using select_construct_t =
    std::conditional_t<is_object_iterator<CharT, Alloc, InputIt>::value, object_tag_t, array_tag_t>;

template<typename Ty>
Ty get_optional_value(est::optional<Ty>&& opt) {
    return std::move(*opt);
}

template<typename Ty>
const Ty* get_optional_value(const Ty* opt) {
    return opt;
}

template<typename Iter>
class object_range {
 public:
    using iterator = Iter;
    using size_type = std::size_t;

    template<typename CharT, typename Alloc>
    explicit object_range(const object_t<CharT, Alloc>& obj) noexcept
        : size_(obj.size()), from_(obj.cbegin()), to_(obj.cend()) {}

    Iter begin() const noexcept { return from_; }
    Iter end() const noexcept { return to_; }
    size_type size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

 private:
    size_type size_;
    Iter from_, to_;
};

}  // namespace detail

template<typename CharT, typename Alloc = std::allocator<CharT>>
class basic_value : protected std::allocator_traits<Alloc>::template rebind_alloc<std::uintptr_t> {
    static_assert(std::is_same<std::remove_cv_t<CharT>, CharT>::value,
                  "uxs::basic_value<> must have a non-const, non-volatile character type");
    static_assert(est::is_character<CharT>::value, "uxs::basic_value<> defined for character types");

 private:
    using char_array_t = detail::flexarray_t<CharT, Alloc>;
    using value_array_t = detail::flexarray_t<basic_value, Alloc>;
    using object_t = detail::object_t<CharT, Alloc>;
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<std::uintptr_t>;
    using alloc_traits = std::allocator_traits<alloc_type>;

 public:
    using char_type = CharT;
    using allocator_type = Alloc;
    using key_type = typename object_t::key_type;
    using mapped_type = basic_value<CharT, Alloc>;
    using value_type = basic_value<CharT, Alloc>;
    using node_handle = typename object_t::node_handle;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using iterator = detail::value_iterator<CharT, Alloc, false>;
    using const_iterator = detail::value_iterator<CharT, Alloc, true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using object_iterator = typename object_t::iterator;
    using const_object_iterator = typename object_t::const_iterator;
    using array_range = est::span<value_type>;
    using const_array_range = est::span<const value_type>;
    using object_range = detail::object_range<object_iterator>;
    using const_object_range = detail::object_range<const_object_iterator>;

    basic_value() noexcept(std::is_nothrow_default_constructible<alloc_type>::value)
        : alloc_type(), type_(dtype::null) {
        value_.null = nullptr;
    }
    basic_value(std::nullptr_t) noexcept(std::is_nothrow_default_constructible<alloc_type>::value)
        : alloc_type(), type_(dtype::null) {
        value_.null = nullptr;
    }
    basic_value(string_tag_t) noexcept(std::is_nothrow_default_constructible<alloc_type>::value)
        : alloc_type(), type_(dtype::string) {
        value_.str.construct_empty();
    }
    basic_value(array_tag_t) noexcept(std::is_nothrow_default_constructible<alloc_type>::value)
        : alloc_type(), type_(dtype::array) {
        value_.arr.construct_empty();
    }
    basic_value(object_tag_t) : alloc_type(), type_(dtype::object) { value_.obj.construct_empty(*this); }

    explicit basic_value(const Alloc& al) noexcept : alloc_type(al), type_(dtype::null) { value_.null = nullptr; }
    basic_value(std::nullptr_t, const Alloc& al) noexcept : alloc_type(al), type_(dtype::null) {
        value_.null = nullptr;
    }
    basic_value(string_tag_t, const Alloc& al) noexcept : alloc_type(al), type_(dtype::string) {
        value_.str.construct_empty();
    }
    basic_value(array_tag_t, const Alloc& al) noexcept : alloc_type(al), type_(dtype::array) {
        value_.arr.construct_empty();
    }
    basic_value(object_tag_t, const Alloc& al) : alloc_type(al), type_(dtype::object) {
        value_.obj.construct_empty(*this);
    }

    template<typename StrLikeTy,
             typename = std::enable_if_t<std::is_convertible<const StrLikeTy&, std::basic_string_view<char_type>>::value>>
    basic_value(const StrLikeTy& s, const Alloc& al = Alloc()) : alloc_type(al), type_(dtype::string) {
        value_.str.construct_from_view(*this, to_string_view(s), 0);
    }

    template<typename FillFn>
    basic_value(string_tag_t, size_type max_length, FillFn&& fn, const Alloc& al = Alloc())
        : alloc_type(al), type_(dtype::string) {
        value_.str.construct_fill(*this, max_length, std::forward<FillFn>(fn));
    }

    basic_value(array_tag_t, size_type count, const Alloc& al = Alloc()) : alloc_type(al), type_(dtype::array) {
        value_.arr.construct_fill_value(*this, count, value_type());
    }

    basic_value(array_tag_t, size_type count, const value_type& v, const Alloc& al = Alloc())
        : alloc_type(al), type_(dtype::array) {
        value_.arr.construct_fill_value(*this, count, v);
    }

    template<typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>>
    basic_value(InputIt first, InputIt last, const Alloc& al = Alloc())
        : basic_value(detail::select_construct_t<CharT, Alloc, InputIt>(0), first, last, al) {}
    template<typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>>
    basic_value(array_tag_t, InputIt first, InputIt last, const Alloc& al = Alloc())
        : alloc_type(al), type_(dtype::array) {
        value_.arr.construct_from_range(*this, first, last);
    }
    template<typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>>
    basic_value(object_tag_t, InputIt first, InputIt last, const Alloc& al = Alloc())
        : alloc_type(al), type_(dtype::object) {
        value_.obj.construct_from_range(*this, first, last);
    }

    UXS_EXPORT basic_value(std::initializer_list<value_type> init, const Alloc& al = Alloc());
    basic_value(array_tag_t, std::initializer_list<value_type> init, const Alloc& al = Alloc())
        : alloc_type(al), type_(dtype::array) {
        value_.arr.construct_from_initializer(*this, init);
    }
    basic_value(object_tag_t, std::initializer_list<std::pair<key_type, value_type>> init, const Alloc& al = Alloc())
        : alloc_type(al), type_(dtype::object) {
        value_.obj.construct_from_initializer(*this, init);
    }

    template<typename Func>
    basic_value(dtype type, Func&& fn, const Alloc& al = Alloc()) : alloc_type(al), type_(type) {
        switch (type_) {
            case dtype::null: break;
            case dtype::boolean: fn(scalar_tag, value_.b); break;
            case dtype::integer: fn(scalar_tag, value_.i); break;
            case dtype::unsigned_integer: fn(scalar_tag, value_.u); break;
            case dtype::long_integer: fn(scalar_tag, value_.i64); break;
            case dtype::unsigned_long_integer: fn(scalar_tag, value_.u64); break;
            case dtype::double_precision: fn(scalar_tag, value_.dbl); break;
            case dtype::string: {
                value_.str.construct_empty();
                fn(string_tag, *this);
            } break;
            case dtype::array: {
                value_.arr.construct_empty();
                fn(array_tag, *this);
            } break;
            case dtype::object: {
                value_.obj.construct_empty(*this);
                fn(object_tag, *this);
            } break;
            default: UXS_UNREACHABLE_CODE;
        }
    }

    ~basic_value() { destroy(); }

    basic_value(const basic_value& other) noexcept : alloc_type(other), type_(other.type_) { init_from(other); }
    basic_value(const basic_value& other, const Alloc& al) noexcept : alloc_type(al), type_(other.type_) {
        init_from(other);
    }
    basic_value& operator=(const basic_value& other) noexcept {
        if (&other == this) { return *this; }
        destroy();
        alloc_type::operator=(other);
        type_ = other.type_;
        init_from(other);
        return *this;
    }

    basic_value(basic_value&& other) noexcept : alloc_type(std::move(other)), type_(other.type_), value_(other.value_) {
        other.type_ = dtype::null;
        other.value_.null = nullptr;
    }
    basic_value(basic_value&& other, const Alloc& al) noexcept : alloc_type(al), type_(other.type_) {
        move_construct_dispatch(std::move(other), typename alloc_traits::is_always_equal());
    }
    basic_value& operator=(basic_value&& other) noexcept {
        if (&other == this) { return *this; }
        destroy();
        alloc_type::operator=(std::move(other));
        type_ = other.type_;
        value_ = other.value_;
        other.type_ = dtype::null;
        other.value_.null = nullptr;
        return *this;
    }

    void swap(basic_value& other) noexcept {
        if (&other == this) { return; }
        std::swap(static_cast<alloc_type&>(*this), static_cast<alloc_type&>(other));
        std::swap(value_, other.value_);
        std::swap(type_, other.type_);
    }

    basic_value& operator=(std::nullptr_t) noexcept {
        destroy();
        type_ = dtype::null;
        value_.null = nullptr;
        return *this;
    }

#define UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(ty, id, field) \
    basic_value(ty v) noexcept(std::is_nothrow_default_constructible<alloc_type>::value) : alloc_type(), type_(id) { \
        value_.field = static_cast<decltype(value_.field)>(v); \
    } \
    basic_value(ty v, const Alloc& al) noexcept : alloc_type(al), type_(id) { \
        value_.field = static_cast<decltype(value_.field)>(v); \
    } \
    basic_value& operator=(ty v) noexcept { \
        destroy(); \
        type_ = id; \
        value_.field = static_cast<decltype(value_.field)>(v); \
        return *this; \
    } \
    static_assert(true, "")
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(bool, dtype::boolean, b);
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(signed, dtype::integer, i);
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(unsigned, dtype::unsigned_integer, u);
#if ULONG_MAX > 0xffffffff
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(signed long, dtype::long_integer, i64);
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(unsigned long, dtype::unsigned_long_integer, u64);
#else   // ULONG_MAX > 0xffffffff
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(signed long, dtype::integer, i);
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(unsigned long, dtype::unsigned_integer, u);
#endif  // ULONG_MAX > 0xffffffff
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(signed long long, dtype::long_integer, i64);
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(unsigned long long, dtype::unsigned_long_integer, u64);
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(float, dtype::double_precision, dbl);
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(double, dtype::double_precision, dbl);
    UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT(long double, dtype::double_precision, dbl);
#undef UXS_DB_VALUE_IMPLEMENT_SCALAR_INIT

    // --------------------------

    template<typename StrLikeTy,
             typename = std::enable_if_t<std::is_convertible<const StrLikeTy&, std::basic_string_view<char_type>>::value>>
    basic_value& operator=(const StrLikeTy& s);

    template<typename FillFn>
    void assign(string_tag_t, size_type max_length, FillFn&& fn);

    template<typename StrLikeTy,
             typename = std::enable_if_t<std::is_convertible<const StrLikeTy&, std::basic_string_view<char_type>>::value>>
    basic_value& append_string(const StrLikeTy& s);

    template<typename FillFn>
    basic_value& append_string(size_type max_length, FillFn&& fn);

    basic_value& operator=(std::initializer_list<value_type> init) {
        assign(init);
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>>
    void assign(InputIt first, InputIt last) {
        assign(detail::select_construct_t<CharT, Alloc, InputIt>(0), first, last);
    }
    template<typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>>
    void assign(array_tag_t, InputIt first, InputIt last);
    template<typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>>
    void assign(object_tag_t, InputIt first, InputIt last);

    UXS_EXPORT void assign(std::initializer_list<value_type> init);
    UXS_EXPORT void assign(array_tag_t, std::initializer_list<value_type> init);
    UXS_EXPORT void assign(object_tag_t, std::initializer_list<std::pair<key_type, value_type>> init);

    template<typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>>
    void append(InputIt first, InputIt last) {
        append(detail::select_construct_t<CharT, Alloc, InputIt>(0), first, last);
    }
    template<typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>>
    void append(array_tag_t, InputIt first, InputIt last);
    template<typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>>
    void append(object_tag_t, InputIt first, InputIt last);

    UXS_EXPORT void append(std::initializer_list<value_type> init);
    UXS_EXPORT void append(array_tag_t, std::initializer_list<value_type> init);
    UXS_EXPORT void append(object_tag_t, std::initializer_list<std::pair<key_type, value_type>> init);

    template<typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>>
    void insert(size_type pos, InputIt first, InputIt last);
    UXS_EXPORT void insert(size_type pos, std::initializer_list<value_type> init);

    // --------------------------

    void clear() {
        switch (type_) {
            case dtype::string: value_.str.clear(*this); break;
            case dtype::array: value_.arr.clear(*this); break;
            case dtype::object: value_.obj.clear(*this); break;
            default: break;
        }
    }

    void ensure_unique() {
        switch (type_) {
            case dtype::string: value_.str.ensure_unique(*this); break;
            case dtype::array: value_.arr.ensure_unique(*this); break;
            case dtype::object: value_.obj.ensure_unique(*this); break;
            default: break;
        }
    }

    void reserve(string_tag_t, size_type size) {
        if (type_ == dtype::string) {
            value_.str.reserve(*this, size);
        } else {
            if (type_ != dtype::null) { report_not_a_string_error(); }
            value_.str.construct_empty(*this, size);
            type_ = dtype::string;
        }
    }

    void reserve(array_tag_t, size_type size) {
        if (type_ == dtype::array) {
            value_.arr.reserve(*this, size);
        } else {
            if (type_ != dtype::null) { report_not_an_array_error(); }
            value_.arr.construct_empty(*this, size);
            type_ = dtype::array;
        }
    }

    void reserve(object_tag_t, size_type size) {
        if (type_ == dtype::object) {
            value_.obj.reserve(*this, size);
        } else {
            if (type_ != dtype::null) { report_not_an_object_error(); }
            value_.obj.construct_empty(*this, size);
            type_ = dtype::object;
        }
    }

    void resize(size_type size, const value_type& v = {}) {
        if (type_ == dtype::array) {
            value_.arr.resize(*this, size, v);
        } else {
            if (type_ != dtype::null) { report_not_an_array_error(); }
            value_.arr.construct_fill_value(*this, size, v);
            type_ = dtype::array;
        }
    }

    // --------------------------

    UXS_EXPORT bool is_equal_to(const basic_value& other) const noexcept;

    friend bool operator==(const basic_value& lhs, const basic_value& rhs) noexcept { return lhs.is_equal_to(rhs); }
    friend bool operator!=(const basic_value& lhs, const basic_value& rhs) noexcept { return !(lhs == rhs); }

    dtype type() const noexcept { return type_; }
    allocator_type get_allocator() const noexcept { return allocator_type(*this); }

    template<typename Ty>
    Ty as() const;

    template<typename Ty>
    est::optional<Ty> get() const;

    template<typename Ty, typename U>
    Ty value_or(U&& default_value) const {
        auto result = get<Ty>();
        return result ? detail::get_optional_value(std::move(result)) : Ty(std::forward<U>(default_value));
    }

    template<typename Ty, typename U>
    Ty value_or(size_type i, U&& default_value) const {
        const auto range = as_array();
        if (i < range.size()) {
            auto result = range[i].template get<Ty>();
            if (result) { return detail::get_optional_value(std::move(result)); }
        }
        return Ty(std::forward<U>(default_value));
    }

    template<typename Ty, typename U>
    Ty value_or(key_type key, U&& default_value) const {
        const auto it = find_optional(key);
        if (it) {
            auto result = (**it).value().template get<Ty>();
            if (result) { return detail::get_optional_value(std::move(result)); }
        }
        return Ty(std::forward<U>(default_value));
    }

    template<typename Ty>
    Ty value() const {
        return value_or<Ty>(Ty());
    }

    template<typename Ty>
    Ty value(size_type i) const {
        return value_or<Ty>(i, Ty());
    }

    template<typename Ty>
    Ty value(key_type key) const {
        return value_or<Ty>(key, Ty());
    }

    value_type value(size_type i) const {
        const auto range = as_array();
        return i < range.size() ? range[i] : value_type();
    }

    value_type value(key_type key) const {
        const auto it = find_optional(key);
        return it ? (**it).value() : value_type();
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
    bool is_array() const noexcept { return type_ == dtype::array; }
    bool is_object() const noexcept { return type_ == dtype::object; }

    bool as_bool() const;
    std::int32_t as_int() const;
    std::uint32_t as_uint() const;
    std::int64_t as_int64() const;
    std::uint64_t as_uint64() const;
    double as_double() const;
    std::basic_string<char_type> as_string() const;

    std::basic_string_view<char_type> as_string_view() const {
        if (type_ != dtype::string) { report_value_conversion_error(); }
        return value_.str.cview();
    }

    const char_type* as_c_string() const {
        if (type_ != dtype::string) { report_value_conversion_error(); }
        return value_.str.c_str();
    }

    UXS_EXPORT est::optional<bool> get_bool() const;
    UXS_EXPORT est::optional<std::int32_t> get_int() const;
    UXS_EXPORT est::optional<std::uint32_t> get_uint() const;
    UXS_EXPORT est::optional<std::int64_t> get_int64() const;
    UXS_EXPORT est::optional<std::uint64_t> get_uint64() const;
    UXS_EXPORT est::optional<double> get_double() const;
    UXS_EXPORT est::optional<std::basic_string<char_type>> get_string() const;
    est::optional<std::basic_string_view<char_type>> get_string_view() const {
        return type_ == dtype::string ? est::make_optional(value_.str.cview()) : est::nullopt;
    }
    const char_type* get_c_string() const { return type_ == dtype::string ? value_.str.c_str() : nullptr; }

    // --------------------------

    bool empty() const noexcept { return size() == 0; }

    size_type size() const noexcept {
        switch (type_) {
            case dtype::null: return 0;
            case dtype::array: return value_.arr.size();
            case dtype::object: return value_.obj.size();
            default: return 1;
        }
    }

    const_iterator begin() const noexcept {
        if (type_ == dtype::object) { return const_iterator(value_.obj.cbegin()); }
        const auto range = as_array();
        return const_iterator(const_cast<value_type*>(range.data()), range.data(), range.data() + range.size());
    }

    iterator begin() {
        if (type_ == dtype::object) {
            value_.obj.ensure_unique(*this);
            return iterator(value_.obj.cbegin());
        }
        const auto range = as_array();
        return iterator(range.data(), range.data(), range.data() + range.size());
    }

    const_iterator cbegin() const noexcept { return begin(); }

    const_iterator end() const noexcept {
        if (type_ == dtype::object) { return const_iterator(value_.obj.cend()); }
        const auto range = as_array();
        return const_iterator(const_cast<value_type*>(range.data()) + range.size(), range.data(),
                              range.data() + range.size());
    }

    iterator end() {
        if (type_ == dtype::object) {
            value_.obj.ensure_unique(*this);
            return iterator(value_.obj.cend());
        }
        const auto range = as_array();
        return iterator(range.data() + range.size(), range.data(), range.data() + range.size());
    }
    const_iterator cend() const noexcept { return end(); }

    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }

    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return rend(); }

    const_array_range as_array() const noexcept {
        if (type_ != dtype::array) { return type_ != dtype::null ? est::as_span(this, 1) : const_array_range(); }
        return value_.arr.cview();
    }

    array_range as_array() {
        if (type_ != dtype::array) { return type_ != dtype::null ? est::as_span(this, 1) : array_range(); }
        return value_.arr.view(*this);
    }

    const_array_range as_const_array() const noexcept { return as_array(); }

    const value_type& operator[](size_type i) const { return as_array()[i]; }
    value_type& operator[](size_type i) { return as_array()[i]; }

    const_object_range as_object() const {
        if (type_ != dtype::object) { report_not_an_object_error(); }
        return const_object_range(value_.obj);
    }

    object_range as_object() {
        if (type_ != dtype::object) { report_not_an_object_error(); }
        value_.obj.ensure_unique(*this);
        return object_range(value_.obj);
    }

    const_object_range as_const_object() const { return as_object(); }

    est::optional<const_object_iterator> find_optional(key_type key) const noexcept {
        if (type_ != dtype::object) { return est::nullopt; }
        auto* node = value_.obj.cfind(key);
        if (node == value_.obj.cend()) { return est::nullopt; }
        return const_object_iterator(node);
    }

    est::optional<object_iterator> find_optional(key_type key) {
        if (type_ != dtype::object) { return est::nullopt; }
        value_.obj.ensure_unique(*this);
        auto* node = value_.obj.cfind(key);
        if (node == value_.obj.cend()) { return est::nullopt; }
        return object_iterator(node);
    }

    est::optional<const_object_iterator> cfind_optional(key_type key) const noexcept { return find_optional(key); }

    const_iterator find(key_type key) const noexcept {
        const auto it = find_optional(key);
        return it ? *it : end();
    }

    iterator find(key_type key) {
        const auto it = find_optional(key);
        return it ? *it : end();
    }

    const_iterator cfind(key_type key) const noexcept { return find(key); }

    bool contains(key_type key) const noexcept { return !!find_optional(key); }
    size_type count(key_type key) const noexcept { return type_ == dtype::object ? value_.obj.count(key) : 0; }

    const value_type& at(size_type i) const {
        const auto range = as_array();
        if (i >= range.size()) { report_index_out_of_range_error(); }
        return range[i];
    }

    value_type& at(size_type i) {
        const auto range = as_array();
        if (i >= range.size()) { report_index_out_of_range_error(); }
        return range[i];
    }

    const value_type& at(key_type key) const {
        const auto it = find_optional(key);
        if (!it) { report_invalid_key_error(); }
        return (**it).value();
    }

    value_type& at(key_type key) {
        const auto it = find_optional(key);
        if (!it) { report_invalid_key_error(); }
        return (**it).value();
    }

    value_type& operator[](key_type key) {
        return (*try_append_unique(key, static_cast<const Alloc&>(*this)).first).value();
    }

    template<typename Func>
    auto visit(Func&& fn) const -> decltype(fn(nullptr)) {
        switch (type_) {
            case dtype::null: return fn(nullptr);
            case dtype::boolean: return fn(value_.b);
            case dtype::integer: return fn(value_.i);
            case dtype::unsigned_integer: return fn(value_.u);
            case dtype::long_integer: return fn(value_.i64);
            case dtype::unsigned_long_integer: return fn(value_.u64);
            case dtype::double_precision: return fn(value_.dbl);
            case dtype::string: return fn(value_.str.cview());
            case dtype::array: return fn(value_.arr.cview());
            case dtype::object: return fn(const_object_range(value_.obj));
            default: UXS_UNREACHABLE_CODE;
        }
    }

    // --------------------------

    value_type& push_back(value_type v) {
        if (type_ != dtype::array) { convert_to_array(); }
        return value_.arr.push_back(*this, std::move(v));
    }

    void pop_back() {
        if (type_ != dtype::array) { report_not_an_array_error(); }
        value_.arr.pop_back(*this);
    }

    iterator insert(size_type pos, value_type v) {
        if (type_ != dtype::array) { init_as_array(); }
        value_type& item = value_.arr.insert(*this, pos, std::move(v));
        return iterator(&item, value_.arr.cbegin(), value_.arr.cend());
    }

    object_iterator append_new(key_type key, value_type v) {
        if (type_ != dtype::object) { init_as_object(); }
        return object_iterator(value_.obj.append_new(*this, key, std::move(v)));
    }

    object_iterator append_new(node_handle&& h) {
        if (type_ != dtype::object) { init_as_object(); }
        return object_iterator(value_.obj.append_new(*this, std::move(h)));
    }

    detail::append_return_type<object_iterator, value_type> append_unique(key_type key, value_type v) {
        if (type_ != dtype::object) { init_as_object(); }
        auto result = value_.obj.append_unique(*this, key, std::move(v));
        return {object_iterator(result.position), result.appended, std::move(result.value)};
    }

    detail::append_return_type<object_iterator, node_handle> append_unique(node_handle&& h) {
        if (type_ != dtype::object) { init_as_object(); }
        auto result = value_.obj.append_unique(*this, std::move(h));
        return {object_iterator(result.position), result.appended, std::move(result.value)};
    }

    template<typename... Args>
    std::pair<object_iterator, bool> try_append_unique(key_type key, Args&&... args) {
        if (type_ != dtype::object) { init_as_object(); }
        const auto result = value_.obj.try_append_unique(*this, key, std::forward<Args>(args)...);
        return {object_iterator(result.first), result.second};
    }

    // --------------------------

    void erase(size_type pos);
    iterator erase(const_iterator it);
    std::pair<node_handle, object_iterator> extract(const_iterator it);
    size_type erase(key_type key);

    // --------------------------

    node_handle make_node(key_type key, value_type v) const { return node_handle(*this, key, std::move(v)); }

    template<typename FillFn>
    node_handle make_node(std::size_t max_key_length, FillFn&& fill_key_fn, value_type v) const {
        return node_handle(*this, max_key_length, std::forward<FillFn>(fill_key_fn), std::move(v));
    }

 private:
    friend class detail::object_t<CharT, Alloc>;

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
        object_t obj;
        std::nullptr_t null;
    } value_;

    [[noreturn]] static void report_not_a_string_error() { throw database_error("not a string"); }
    [[noreturn]] static void report_not_an_array_error() { throw database_error("not an array"); }
    [[noreturn]] static void report_not_an_object_error() { throw database_error("not an object"); }
    [[noreturn]] static void report_index_out_of_range_error() { throw database_error("index out of range"); }
    [[noreturn]] static void report_invalid_key_error() { throw database_error("invalid key"); }
    [[noreturn]] static void report_value_conversion_error() { throw database_error("bad value conversion"); }

    void init_from(const basic_value& other) noexcept {
        value_ = other.value_;
        switch (other.type_) {
            case dtype::string: value_.str.ref(); break;
            case dtype::array: value_.arr.ref(); break;
            case dtype::object: value_.obj.ref(); break;
            default: break;
        }
    }

    void destroy() noexcept {
        switch (type_) {
            case dtype::string: value_.str.unref(*this); break;
            case dtype::array: value_.arr.unref(*this); break;
            case dtype::object: value_.obj.unref(*this); break;
            default: break;
        }
    }

    void init_as_array() {
        if (type_ != dtype::null) { report_not_an_array_error(); }
        value_.arr.construct_empty();
        type_ = dtype::array;
    }

    void init_as_object() {
        if (type_ != dtype::null) { report_not_an_object_error(); }
        value_.obj.construct_empty(*this);
        type_ = dtype::object;
    }

    void convert_to_array() {
        value_array_t arr;
        arr.construct_empty();
        if (type_ != dtype::null) { arr.push_back(*this, std::move(*this)); }
        value_.arr = arr;
        type_ = dtype::array;
    }

    template<typename CharT_ = CharT>
    void move_construct_dispatch(basic_value&& other, std::true_type /* always equal allocators */) noexcept {
        type_ = other.type_;
        value_ = other.value_;
        other.type_ = dtype::null;
        other.value_.null = nullptr;
    }

    template<typename CharT_ = CharT>
    void move_construct_dispatch(basic_value&& other, std::false_type /* always equal allocators */) noexcept {
        if (static_cast<const alloc_type&>(*this) == static_cast<const alloc_type&>(other)) {
            type_ = other.type_;
            value_ = other.value_;
            other.type_ = dtype::null;
            other.value_.null = nullptr;
        } else {
            init_from(other);
        }
    }
};

// --------------------------

template<typename CharT, typename Alloc>
template<typename StrLikeTy, typename>
auto basic_value<CharT, Alloc>::operator=(const StrLikeTy& s) -> basic_value& {
    if (type_ == dtype::string) {
        value_.str.assign_view(*this, to_string_view(s));
    } else {
        char_array_t new_str;
        new_str.construct_from_view(*this, to_string_view(s), 0);
        destroy();
        type_ = dtype::string;
        value_.str = new_str;
    }
    return *this;
}

template<typename CharT, typename Alloc>
template<typename FillFn>
void basic_value<CharT, Alloc>::assign(string_tag_t, size_type max_length, FillFn&& fn) {
    if (type_ == dtype::string) {
        value_.str.clear();
        value_.str.append_fill(*this, max_length, std::forward<FillFn>(fn));
    } else {
        char_array_t new_str;
        new_str.construct_fill(*this, max_length, std::forward<FillFn>(fn));
        destroy();
        type_ = dtype::string;
        value_.str = new_str;
    }
}

template<typename CharT, typename Alloc>
template<typename StrLikeTy, typename>
auto basic_value<CharT, Alloc>::append_string(const StrLikeTy& s) -> basic_value& {
    if (type_ == dtype::string) {
        value_.str.append_view(*this, to_string_view(s));
    } else {
        if (type_ != dtype::null) { report_not_a_string_error(); }
        value_.str.construct_from_view(*this, to_string_view(s), 0);
        type_ = dtype::string;
    }
    return *this;
}

template<typename CharT, typename Alloc>
template<typename FillFn>
auto basic_value<CharT, Alloc>::append_string(size_type max_length, FillFn&& fn) -> basic_value& {
    if (type_ == dtype::string) {
        value_.str.append_fill(*this, max_length, std::forward<FillFn>(fn));
    } else {
        if (type_ != dtype::null) { report_not_a_string_error(); }
        value_.str.construct_fill(*this, max_length, std::forward<FillFn>(fn));
        type_ = dtype::string;
    }
    return *this;
}

template<typename CharT, typename Alloc>
template<typename InputIt, typename>
void basic_value<CharT, Alloc>::assign(array_tag_t, InputIt first, InputIt last) {
    if (type_ == dtype::array) {
        value_.arr.assign_range(*this, first, last);
    } else {
        value_array_t new_arr;
        new_arr.construct_from_range(*this, first, last);
        destroy();
        type_ = dtype::array;
        value_.arr = new_arr;
    }
}

template<typename CharT, typename Alloc>
template<typename InputIt, typename>
void basic_value<CharT, Alloc>::assign(object_tag_t, InputIt first, InputIt last) {
    if (type_ == dtype::object) {
        value_.obj.assign_range(*this, first, last);
    } else {
        object_t new_obj;
        new_obj.construct_from_range(*this, first, last);
        destroy();
        type_ = dtype::object;
        value_.obj = new_obj;
    }
}

template<typename CharT, typename Alloc>
template<typename InputIt, typename>
void basic_value<CharT, Alloc>::append(array_tag_t, InputIt first, InputIt last) {
    if (type_ == dtype::array) {
        value_.arr.append_range(*this, first, last);
    } else {
        if (type_ != dtype::null) { report_not_an_array_error(); }
        value_.arr.construct_from_range(*this, first, last);
        type_ = dtype::array;
    }
}

template<typename CharT, typename Alloc>
template<typename InputIt, typename>
void basic_value<CharT, Alloc>::append(object_tag_t, InputIt first, InputIt last) {
    if (type_ == dtype::object) {
        value_.obj.append_range(*this, first, last);
    } else {
        if (type_ != dtype::null) { report_not_an_object_error(); }
        value_.obj.construct_from_range(*this, first, last);
        type_ = dtype::object;
    }
}

template<typename CharT, typename Alloc>
template<typename InputIt, typename>
void basic_value<CharT, Alloc>::insert(size_type pos, InputIt first, InputIt last) {
    if (type_ == dtype::array) {
        value_.arr.insert_range(*this, pos, first, last);
    } else {
        if (type_ != dtype::null) { report_not_an_array_error(); }
        value_.arr.construct_from_range(*this, first, last);
        type_ = dtype::array;
    }
}

// --------------------------

template<typename CharT, typename Alloc>
void basic_value<CharT, Alloc>::erase(size_type pos) {
    if (type_ != dtype::array) { report_not_an_array_error(); }
    assert(pos < value_.arr.size());
    value_.arr.erase(*this, pos);
}

template<typename CharT, typename Alloc>
auto basic_value<CharT, Alloc>::erase(const_iterator it) -> iterator {
    if (it.is_object()) {
        if (type_ != dtype::object) { report_not_an_object_error(); }
        detail::list_links_t* node = static_cast<detail::list_links_t*>(it.ptr_);
        uxs_iterator_assert(object_t::node_traits::get_head(node) == value_.obj.cend());
        return iterator(value_.obj.erase(*this, node));
    }
    if (type_ != dtype::array) { report_not_an_array_error(); }
    uxs_iterator_assert(it.begin_ == value_.arr.cbegin() && it.end_ == value_.arr.cend());
    const size_type pos = static_cast<value_type*>(it.ptr_) - static_cast<const value_type*>(it.begin_);
    value_type* next = value_.arr.erase(*this, pos);
    return iterator(next, value_.arr.cbegin(), value_.arr.cend());
}

template<typename CharT, typename Alloc>
auto basic_value<CharT, Alloc>::extract(const_iterator it) -> std::pair<node_handle, object_iterator> {
    if (!it.is_object() || type_ != dtype::object) { report_not_an_object_error(); }
    detail::list_links_t* node = static_cast<detail::list_links_t*>(it.ptr_);
    uxs_iterator_assert(object_t::node_traits::get_head(node) == value_.obj.cend());
    auto result = value_.obj.extract(*this, node);
    return {std::move(result.first), object_iterator(result.second)};
}

template<typename CharT, typename Alloc>
auto basic_value<CharT, Alloc>::erase(key_type key) -> size_type {
    if (type_ != dtype::object) { report_not_an_object_error(); }
    return value_.obj.erase(*this, key);
}

// --------------------------

#define UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(ty, func) \
    template<typename CharT, typename Alloc> \
    ty basic_value<CharT, Alloc>::as##func() const { \
        const auto result = get##func(); \
        if (!result) { report_value_conversion_error(); } \
        return *result; \
    } \
    static_assert(true, "")
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(bool, _bool);
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(std::int32_t, _int);
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(std::uint32_t, _uint);
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(std::int64_t, _int64);
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(std::uint64_t, _uint64);
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(double, _double);
UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC(std::basic_string<CharT>, _string);
#undef UXS_DB_VALUE_IMPLEMENT_SCALAR_AS_FUNC

namespace detail {

template<typename CharT, typename Alloc, typename Ty>
struct value_getters_specializer;

template<typename Ty, typename TyOther, typename = std::enable_if_t<!std::is_same<Ty, TyOther>::value>>
est::optional<Ty> cast_optional(const est::optional<TyOther>& opt) {
    return opt ? est::make_optional(static_cast<Ty>(*opt)) : est::nullopt;
}

template<typename Ty>
est::optional<Ty> cast_optional(est::optional<Ty>&& opt) {
    return std::move(opt);
}

template<typename Ty>
const Ty* cast_optional(const Ty* opt) {
    return opt;
}

#define UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(ty, func) \
    template<typename CharT, typename Alloc> \
    struct value_getters_specializer<CharT, Alloc, ty> { \
        static ty as(const basic_value<CharT, Alloc>& v) { return static_cast<ty>(v.as##func()); } \
        static auto get(const basic_value<CharT, Alloc>& v) \
            -> decltype(detail::cast_optional<ty>(std::move(v.get##func()))) { \
            return detail::cast_optional<ty>(std::move(v.get##func())); \
        } \
    }
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(bool, _bool);
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(signed, _int);
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(unsigned, _uint);
#if ULONG_MAX > 0xffffffff
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(signed long, _int64);
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(unsigned long, _uint64);
#else   // ULONG_MAX > 0xffffffff
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(signed long, _int);
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(unsigned long, _uint);
#endif  // ULONG_MAX > 0xffffffff
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(signed long long, _int64);
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(unsigned long long, _uint64);
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(float, _double);
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(double, _double);
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(long double, _double);
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(std::basic_string<CharT>, _string);
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(std::basic_string_view<CharT>, _string_view);
UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS(const CharT*, _c_string);
#undef UXS_DB_VALUE_IMPLEMENT_SCALAR_GETTERS

}  // namespace detail

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

// --------------------------

#define UXS_DECLARE_TYPE_ALIASES(type, prefix) using prefix##value = basic_value<type>
UXS_DECLARE_TYPE_ALIASES(char, );
#if UXS_USE_WCHAR_T != 0
UXS_DECLARE_TYPE_ALIASES(wchar_t, w);
#endif  // UXS_USE_WCHAR_T != 0
#if UXS_USE_CHAR8_T != 0
UXS_DECLARE_TYPE_ALIASES(char8_t, u8);
#endif  // UXS_USE_CHAR8_T != 0
UXS_DECLARE_TYPE_ALIASES(char16_t, u16);
UXS_DECLARE_TYPE_ALIASES(char32_t, u32);
#undef UXS_DECLARE_TYPE_ALIASES

}  // namespace db
}  // namespace uxs

namespace std {
template<typename CharT, typename Alloc>
class tuple_size<uxs::db::detail::object_item<CharT, Alloc>> : public std::integral_constant<std::size_t, 2> {};
template<std::size_t I, typename CharT, typename Alloc>
class tuple_element<I, uxs::db::detail::object_item<CharT, Alloc>> {
 public:
    using type = std::remove_cvref_t<decltype(uxs::db::detail::get<I>(
        std::declval<uxs::db::detail::object_item<CharT, Alloc>&>()))>;
};

template<typename CharT, typename Alloc, bool Const>
class tuple_size<uxs::db::detail::value_iterator_proxy<CharT, Alloc, Const>>
    : public std::integral_constant<std::size_t, 2> {};
template<std::size_t I, typename CharT, typename Alloc, bool Const>
class tuple_element<I, uxs::db::detail::value_iterator_proxy<CharT, Alloc, Const>> {
 public:
    using type = std::remove_cvref_t<decltype(uxs::db::detail::get<I>(
        std::declval<uxs::db::detail::value_iterator_proxy<CharT, Alloc, Const>&>()))>;
};

template<typename CharT, typename Alloc>
void swap(uxs::db::basic_value<CharT, Alloc>& v1, uxs::db::basic_value<CharT, Alloc>& v2) noexcept {
    v1.swap(v2);
}
}  // namespace std
