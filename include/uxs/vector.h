#pragma once

#include "allocator.h"
#include "iterator.h"

#include <algorithm>

namespace uxs {

//-----------------------------------------------------------------------------
// Vector implementation

template<typename Ty, typename Alloc = std::allocator<Ty>>
class vector : protected std::allocator_traits<Alloc>::template rebind_alloc<Ty> {
 private:
    static_assert(std::is_same<std::remove_cv_t<Ty>, Ty>::value,
                  "uxs::vector must have a non-const, non-volatile value type");

    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<Ty>;
    using alloc_traits = std::allocator_traits<alloc_type>;

 public:
    using value_type = Ty;
    using allocator_type = Alloc;
    using size_type = typename alloc_traits::size_type;
    using difference_type = typename alloc_traits::difference_type;
    using pointer = typename alloc_traits::pointer;
    using const_pointer = typename alloc_traits::const_pointer;
    using reference = value_type&;
    using const_reference = const value_type&;
    using iterator = array_iterator<vector, false>;
    using const_iterator = array_iterator<vector, true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    vector() NOEXCEPT_IF(std::is_nothrow_default_constructible<alloc_type>::value) : alloc_type(allocator_type()) {}
    explicit vector(const allocator_type& alloc) NOEXCEPT : alloc_type(alloc) {}
    explicit vector(size_type sz, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        init_default(sz);
    }

    vector(size_type sz, const value_type& val, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        init(sz, const_value(val));
    }

    vector(std::initializer_list<value_type> l, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        init(l.size(), l.begin());
    }

    vector& operator=(std::initializer_list<value_type> l) {
        assign_impl(l.size(), l.begin());
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    vector(InputIt first, InputIt last, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        init_from_range(first, last, is_random_access_iterator<InputIt>());
    }

    vector(const vector& other) : alloc_type(alloc_traits::select_on_container_copy_construction(other)) {
        init(other.size(), other.begin());
    }

    vector(const vector& other, const allocator_type& alloc) : alloc_type(alloc) { init(other.size(), other.begin()); }

    vector& operator=(const vector& other) {
        if (std::addressof(other) == this) { return *this; }
        assign_impl(other, std::bool_constant<(!alloc_traits::propagate_on_container_copy_assignment::value ||
                                               is_alloc_always_equal<alloc_type>::value)>());
        return *this;
    }

    vector(vector&& other) NOEXCEPT : alloc_type(std::move(other)) {
        v_ = other.v_;
        other.v_.nullify();
    }

    vector(vector&& other, const allocator_type& alloc) NOEXCEPT_IF(is_alloc_always_equal<alloc_type>::value)
        : alloc_type(alloc) {
        construct_impl(std::move(other), alloc, is_alloc_always_equal<alloc_type>());
    }

    vector& operator=(vector&& other) NOEXCEPT_IF(alloc_traits::propagate_on_container_move_assignment::value ||
                                                  is_alloc_always_equal<alloc_type>::value) {
        if (std::addressof(other) == this) { return *this; }
        assign_impl(std::move(other), std::bool_constant<(alloc_traits::propagate_on_container_move_assignment::value ||
                                                          is_alloc_always_equal<alloc_type>::value)>());
        return *this;
    }

    ~vector() { tidy(); }

    void swap(vector& other) NOEXCEPT {
        if (std::addressof(other) == this) { return; }
        swap_impl(other, typename alloc_traits::propagate_on_container_swap());
    }

    allocator_type get_allocator() const NOEXCEPT { return allocator_type(*this); }

    bool empty() const NOEXCEPT { return v_.end == v_.begin; }
    size_type size() const NOEXCEPT { return static_cast<size_type>(v_.end - v_.begin); }
    size_type capacity() const NOEXCEPT { return static_cast<size_type>(v_.boundary - v_.begin); }
    size_type max_size() const NOEXCEPT { return alloc_traits::max_size(*this); }

    iterator begin() NOEXCEPT { return iterator(v_.begin, v_.begin, v_.end); }
    const_iterator begin() const NOEXCEPT { return const_iterator(v_.begin, v_.begin, v_.end); }
    const_iterator cbegin() const NOEXCEPT { return const_iterator(v_.begin, v_.begin, v_.end); }

    iterator end() NOEXCEPT { return iterator(v_.end, v_.begin, v_.end); }
    const_iterator end() const NOEXCEPT { return const_iterator(v_.end, v_.begin, v_.end); }
    const_iterator cend() const NOEXCEPT { return const_iterator(v_.end, v_.begin, v_.end); }

    reverse_iterator rbegin() NOEXCEPT { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const NOEXCEPT { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const NOEXCEPT { return const_reverse_iterator(end()); }

    reverse_iterator rend() NOEXCEPT { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const NOEXCEPT { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const NOEXCEPT { return const_reverse_iterator(begin()); }

    pointer data() NOEXCEPT { return v_.begin; }
    const_pointer data() const NOEXCEPT { return v_.begin; }

    reference operator[](size_type i) {
        assert(i < size());
        return v_.begin[i];
    }
    const_reference operator[](size_type i) const {
        assert(i < size());
        return v_.begin[i];
    }

    reference at(size_type i) {
        if (i < size()) { return v_.begin[i]; }
        throw std::out_of_range("index out of range");
    }
    const_reference at(size_type i) const {
        if (i < size()) { return v_.begin[i]; }
        throw std::out_of_range("index out of range");
    }

    reference front() {
        assert(v_.begin != v_.end);
        return *begin();
    }
    const_reference front() const {
        assert(v_.begin != v_.end);
        return *begin();
    }

    reference back() {
        assert(v_.begin != v_.end);
        return *rbegin();
    }
    const_reference back() const {
        assert(v_.begin != v_.end);
        return *rbegin();
    }

    void assign(size_type sz, const value_type& val) { assign_impl(sz, const_value(val)); }

    void assign(std::initializer_list<value_type> l) { assign_impl(l.size(), l.begin()); }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    void assign(InputIt first, InputIt last) {
        assign_range(first, last, is_random_access_iterator<InputIt>());
    }

    void clear() NOEXCEPT { v_.end = helpers::truncate(*this, v_.begin, v_.end); }

    void reserve(size_type reserve_sz) {
        if (reserve_sz <= capacity()) { return; }
        auto v = alloc_new_checked(reserve_sz);
        relocate(v, std::is_nothrow_move_constructible<Ty>());
        reset(v);
    }

    void shrink_to_fit() {
        if (v_.end == v_.boundary) { return; }
        vector_ptrs_t v;
        if (v_.begin != v_.end) {
            v = alloc_new(size());
            relocate(v, std::is_nothrow_move_constructible<Ty>());
        }
        reset(v);
    }

    void resize(size_type sz) {
        if (sz <= size()) {
            v_.end = helpers::truncate(*this, v_.begin + sz, v_.end);
        } else {
            size_type count = sz - size();
            if (sz <= capacity()) {
                v_.end = helpers::construct_default(*this, v_.end, count);
            } else {
                auto v = alloc_new(grow_capacity(count));
                resize_relocate_default(v, count, std::is_nothrow_move_constructible<Ty>());
                reset(v);
            }
        }
    }

    void resize(size_type sz, const value_type& val) {
        if (sz <= size()) {
            v_.end = helpers::truncate(*this, v_.begin + sz, v_.end);
        } else {
            size_type count = sz - size();
            if (sz <= capacity()) {
                v_.end = helpers::construct_copy(*this, v_.end, count, const_value(val));
            } else {
                auto v = alloc_new(grow_capacity(count));
                resize_relocate_fill(v, count, val, std::is_nothrow_move_constructible<Ty>());
                reset(v);
            }
        }
    }

    iterator insert(const_iterator pos, size_type count, const value_type& val) {
        auto p = insert_fill(to_ptr(pos), count, val);
        return iterator(p, v_.begin, v_.end);
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        auto p = insert_range(to_ptr(pos), first, last, is_random_access_iterator<InputIt>());
        return iterator(p, v_.begin, v_.end);
    }

    iterator insert(const_iterator pos, std::initializer_list<value_type> l) {
        auto p = insert_copy(to_ptr(pos), l.size(), l.begin(), std::is_copy_assignable<Ty>());
        return iterator(p, v_.begin, v_.end);
    }

    iterator insert(const_iterator pos, const value_type& val) { return emplace(pos, val); }
    iterator insert(const_iterator pos, value_type&& val) { return emplace(pos, std::move(val)); }
    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        auto p = to_ptr(pos);
        if (v_.end != v_.boundary) {
            if (p == v_.end) {
                alloc_traits::construct(*this, std::addressof(*v_.end), std::forward<Args>(args)...);
            } else {
                helpers::emplace(*this, p, v_.end, std::forward<Args>(args)...);
            }
            return iterator(p, v_.begin, ++v_.end);
        }
        auto v = alloc_new(grow_capacity(1));
        p = emplace_relocate(v, p, std::is_nothrow_move_constructible<Ty>(), std::forward<Args>(args)...);
        reset(v);
        return iterator(p, v_.begin, v_.end);
    }

    void push_back(const value_type& val) { emplace_back(val); }
    void push_back(value_type&& val) { emplace_back(std::move(val)); }
    template<typename... Args>
    reference emplace_back(Args&&... args) {
        if (v_.end != v_.boundary) {
            alloc_traits::construct(*this, std::addressof(*v_.end), std::forward<Args>(args)...);
            return *v_.end++;
        }
        auto v = alloc_new(grow_capacity(1));
        emplace_back_relocate(v, std::is_nothrow_move_constructible<Ty>(), std::forward<Args>(args)...);
        reset(v);
        return *(v_.end - 1);
    }

    void pop_back() {
        assert(v_.begin != v_.end);
        alloc_traits::destroy(*this, std::addressof(*(--v_.end)));
    }

    iterator erase(const_iterator pos) {
        auto p = to_ptr(pos);
        assert(p != v_.end);
        std::move(p + 1, v_.end--, p);
        alloc_traits::destroy(*this, std::addressof(*v_.end));
        return iterator(p, v_.begin, v_.end);
    }

    iterator erase(const_iterator first, const_iterator last) {
        auto p_first = to_ptr(first);
        auto p_last = to_ptr(last);
        assert(v_.begin <= p_first && p_first <= p_last && p_last <= v_.end);
        size_type count = static_cast<size_type>(p_last - p_first);
        if (count) {
            std::move(p_first + count, v_.end, p_first);
            v_.end = helpers::truncate(*this, v_.end - count, v_.end);
        }
        return iterator(p_first, v_.begin, v_.end);
    }

 private:
    struct vector_ptrs_t {
        vector_ptrs_t() = default;
        vector_ptrs_t(pointer p1, pointer p2, pointer p3) : begin(p1), end(p2), boundary(p3) {}
        void nullify() { begin = end = boundary = nullptr; }
        pointer begin{nullptr};
        pointer end{nullptr};
        pointer boundary{nullptr};
    };

    vector_ptrs_t v_;

    enum : unsigned { kStartCapacity = 8 };

    bool is_same_alloc(const alloc_type& alloc) { return static_cast<alloc_type&>(*this) == alloc; }

    pointer to_ptr(const_iterator it) const {
        auto p = std::addressof(const_cast<reference>(*it.ptr()));
        iterator_assert(it.begin() == v_.begin && it.end() == v_.end);
        assert(v_.begin <= p && p <= v_.end);
        return p;
    }

    size_type grow_capacity(size_type extra) const {
        size_type sz = size(), delta_sz = std::max(extra, sz >> 1);
        const size_type max_avail = alloc_traits::max_size(*this) - sz;
        if (delta_sz > max_avail) {
            if (extra > max_avail) { throw std::length_error("too much to reserve"); }
            delta_sz = std::max(extra, max_avail >> 1);
        }
        return std::max<size_type>(sz + delta_sz, kStartCapacity);
    }

    vector_ptrs_t alloc_new_checked(size_type sz) {
        if (sz > alloc_traits::max_size(*this)) { throw std::length_error("too much to reserve"); }
        return alloc_new(sz);
    }

    vector_ptrs_t alloc_new(size_type sz) {
        assert(sz);
        auto p = alloc_traits::allocate(*this, sz);
        return vector_ptrs_t(p, p, p + sz);
    }

    void reset(const vector_ptrs_t& v_new) {
        if (v_.begin != v_.boundary) { tidy(v_); }
        v_ = v_new;
    }
    void tidy() {
        if (v_.begin != v_.boundary) { tidy(v_); }
        v_.nullify();
    }
    void tidy(vector_ptrs_t& v) {
        assert(v.begin != v.boundary);
        v.end = helpers::truncate(*this, v.begin, v.end);
        alloc_traits::deallocate(*this, v.begin, static_cast<size_type>(v.boundary - v.begin));
    }

    void construct_impl(vector&& other, const allocator_type& alloc, std::true_type) NOEXCEPT {
        v_ = other.v_;
        other.v_.nullify();
    }

    void construct_impl(vector&& other, const allocator_type& alloc, std::false_type) {
        if (is_same_alloc(other)) {
            v_ = other.v_;
            other.v_.nullify();
        } else {
            init(other.size(), std::make_move_iterator(other.begin()));
        }
    }

    void assign_impl(const vector& other, std::true_type) { assign_impl(other.size(), other.begin()); }

    void assign_impl(const vector& other, std::false_type) {
        if (is_same_alloc(other)) {
            assign_impl(other.size(), other.begin());
        } else {
            tidy();
            alloc_type::operator=(other);
            init(other.size(), other.begin());
        }
    }

    void assign_impl(vector&& other, std::true_type) NOEXCEPT {
        reset(other.v_);
        if (alloc_traits::propagate_on_container_move_assignment::value) { alloc_type::operator=(std::move(other)); }
        other.v_.nullify();
    }

    void assign_impl(vector&& other, std::false_type) {
        if (is_same_alloc(other)) {
            reset(other.v_);
            other.v_.nullify();
        } else {
            assign_impl(other.size(), std::make_move_iterator(other.begin()));
        }
    }

    void swap_impl(vector& other, std::false_type) NOEXCEPT { std::swap(v_, other.v_); }
    void swap_impl(vector& other, std::true_type) NOEXCEPT {
        std::swap(static_cast<alloc_type&>(*this), static_cast<alloc_type&>(other));
        std::swap(v_, other.v_);
    }

    void init_default(size_type sz) {
        assert(v_.begin == nullptr);
        if (!sz) { return; }
        v_ = alloc_new_checked(sz);
        try {
            v_.end = helpers::construct_default(*this, v_.end, sz);
        } catch (...) {
            alloc_traits::deallocate(*this, v_.begin, capacity());
            throw;
        }
    }

    template<typename RandIt>
    void init(size_type sz, RandIt src) {
        assert(v_.begin == nullptr);
        if (!sz) { return; }
        v_ = alloc_new_checked(sz);
        try {
            v_.end = helpers::construct_copy(*this, v_.end, sz, src);
        } catch (...) {
            alloc_traits::deallocate(*this, v_.begin, capacity());
            throw;
        }
    }

    template<typename RandIt>
    void init_from_range(RandIt first, RandIt last, std::true_type /* random access iterator */) {
        assert(first <= last);
        init(static_cast<size_type>(last - first), first);
    }

    template<typename InputIt>
    void init_from_range(InputIt first, InputIt last, std::false_type /* random access iterator */) {
        assert(v_.begin == nullptr);
        try {
            for (; first != last; ++first) { emplace_back(*first); }
        } catch (...) {
            tidy(v_);
            throw;
        }
    }

    template<typename RandIt>
    void assign_impl(size_type sz, RandIt src) {
        if (sz <= size()) {
            v_.end = helpers::truncate(*this, sz ? std::copy_n(src, sz, v_.begin) : v_.begin, v_.end);
        } else if (sz <= capacity()) {
            size_type old_sz = size();
            std::copy_n(src, old_sz, v_.begin);
            v_.end = helpers::construct_copy(*this, v_.end, sz - old_sz, src + old_sz);
        } else {
            assign_relocate(sz, src);
        }
    }

    template<typename RandIt>
    void assign_relocate(size_type sz, RandIt src) {
        auto v = alloc_new_checked(sz);
        try {
            v.end = helpers::construct_copy(*this, v.end, sz, src);
            reset(v);
        } catch (...) {
            alloc_traits::deallocate(*this, v.begin, static_cast<size_type>(v.boundary - v.begin));
            throw;
        }
    }

    template<typename RandIt>
    void assign_range(RandIt first, RandIt last, std::true_type /* random access iterator */) {
        assert(first <= last);
        assign_impl(static_cast<size_type>(last - first), first);
    }

    template<typename InputIt>
    void assign_range(InputIt first, InputIt last, std::false_type /* random access iterator */) {
        auto end_new = v_.begin;
        for (; end_new != v_.end && first != last; ++first) { *end_new++ = *first; }
        if (end_new != v_.end) {
            v_.end = helpers::truncate(*this, end_new, v_.end);
        } else {
            for (; first != last; ++first) { emplace_back(*first); }
        }
    }

    void relocate(vector_ptrs_t& v, std::true_type /* nothrow move */) NOEXCEPT {
        v.end = helpers::construct_relocate(*this, v.end, v_.begin, v_.end);
    }

    void relocate(vector_ptrs_t& v, std::false_type /* nothrow move */) {
        try {
            v.end = helpers::construct_relocate_copy(*this, v.end, v_.begin, v_.end);
        } catch (...) {
            alloc_traits::deallocate(*this, v.begin, static_cast<size_type>(v.boundary - v.begin));
            throw;
        }
    }

    void resize_relocate_default(vector_ptrs_t& v, size_type count, std::true_type /* nothrow move */) {
        try {
            v.end = helpers::construct_default(*this, v.begin + size(), count);
            helpers::construct_relocate(*this, v.begin, v_.begin, v_.end);
        } catch (...) {
            alloc_traits::deallocate(*this, v.begin, static_cast<size_type>(v.boundary - v.begin));
            throw;
        }
    }

    void resize_relocate_default(vector_ptrs_t& v, size_type count, std::false_type /* nothrow move */) {
        try {
            v.end = helpers::construct_relocate_copy(*this, v.end, v_.begin, v_.end);
            v.end = helpers::construct_default(*this, v.end, count);
        } catch (...) {
            tidy(v);
            throw;
        }
    }

    void resize_relocate_fill(vector_ptrs_t& v, size_type count, const value_type& val,
                              std::true_type /* nothrow move */) {
        try {
            v.end = helpers::construct_copy(*this, v.begin + size(), count, const_value(val));
            helpers::construct_relocate(*this, v.begin, v_.begin, v_.end);
        } catch (...) {
            alloc_traits::deallocate(*this, v.begin, static_cast<size_type>(v.boundary - v.begin));
            throw;
        }
    }

    void resize_relocate_fill(vector_ptrs_t& v, size_type count, const value_type& val,
                              std::false_type /* nothrow move */) {
        try {
            v.end = helpers::construct_relocate_copy(*this, v.end, v_.begin, v_.end);
            v.end = helpers::construct_copy(*this, v.end, count, const_value(val));
        } catch (...) {
            tidy(v);
            throw;
        }
    }

    template<typename... Args>
    pointer emplace_relocate(vector_ptrs_t& v, pointer p, std::true_type /* nothrow move */, Args&&... args) {
        try {
            size_type n = static_cast<size_type>(p - v_.begin);
            auto mid = v.begin + n;
            alloc_traits::construct(*this, std::addressof(*mid), std::forward<Args>(args)...);
            helpers::construct_relocate(*this, v.begin, v_.begin, p);
            v.end = helpers::construct_relocate(*this, mid + 1, p, v_.end);
            return mid;
        } catch (...) {
            alloc_traits::deallocate(*this, v.begin, static_cast<size_type>(v.boundary - v.begin));
            throw;
        }
    }

    template<typename... Args>
    pointer emplace_relocate(vector_ptrs_t& v, pointer p, std::false_type /* nothrow move */, Args&&... args) {
        try {
            size_type n = static_cast<size_type>(p - v_.begin);
            v.end = helpers::construct_relocate_copy(*this, v.end, v_.begin, p);
            alloc_traits::construct(*this, std::addressof(*v.end), std::forward<Args>(args)...);
            v.end = helpers::construct_relocate_copy(*this, ++v.end, p, v_.end);
            return v.begin + n;
        } catch (...) {
            tidy(v);
            throw;
        }
    }

    template<typename... Args>
    void emplace_back_relocate(vector_ptrs_t& v, std::true_type /* nothrow move */, Args&&... args) {
        try {
            v.end = v.begin + size();
            alloc_traits::construct(*this, std::addressof(*v.end), std::forward<Args>(args)...);
            helpers::construct_relocate(*this, v.begin, v_.begin, v_.end);
            ++v.end;
        } catch (...) {
            alloc_traits::deallocate(*this, v.begin, static_cast<size_type>(v.boundary - v.begin));
            throw;
        }
    }

    template<typename... Args>
    void emplace_back_relocate(vector_ptrs_t& v, std::false_type /* nothrow move */, Args&&... args) {
        try {
            v.end = helpers::construct_relocate_copy(*this, v.end, v_.begin, v_.end);
            alloc_traits::construct(*this, std::addressof(*v.end), std::forward<Args>(args)...);
            ++v.end;
        } catch (...) {
            tidy(v);
            throw;
        }
    }

    template<typename RandIt, typename Bool>
    pointer insert_copy(pointer p, size_type count, RandIt src, Bool /* assignable */) {
        if (count <= static_cast<size_type>(v_.boundary - v_.end)) {
            if (count) { insert_no_relocate(p, count, src, Bool()); }
            return p;
        }
        auto v = alloc_new(grow_capacity(count));
        p = insert_relocate(v, p, count, src, std::is_nothrow_move_constructible<Ty>());
        reset(v);
        return p;
    }

    pointer insert_fill(pointer p, size_type count, const value_type& val) {
        if (count <= static_cast<size_type>(v_.boundary - v_.end)) {
            if (!count) { return p; }
            typename std::aligned_storage<sizeof(Ty), std::alignment_of<Ty>::value>::type buf;
            auto* val_copy = reinterpret_cast<value_type*>(std::addressof(buf));
            alloc_traits::construct(*this, val_copy, val);
            try {
                insert_no_relocate(p, count, const_value(*val_copy), std::is_copy_assignable<Ty>());
                alloc_traits::destroy(*this, val_copy);
                return p;
            } catch (...) {
                alloc_traits::destroy(*this, val_copy);
                throw;
            }
        }
        auto v = alloc_new(grow_capacity(count));
        p = insert_relocate(v, p, count, const_value(val), std::is_nothrow_move_constructible<Ty>());
        reset(v);
        return p;
    }

    template<typename RandIt>
    pointer insert_relocate(vector_ptrs_t& v, pointer p, size_type count, RandIt src,
                            std::true_type /* nothrow move */) {
        try {
            size_type n = static_cast<size_type>(p - v_.begin);
            auto mid = helpers::construct_copy(*this, v.begin + n, count, src);
            helpers::construct_relocate(*this, v.begin, v_.begin, p);
            v.end = helpers::construct_relocate(*this, mid, p, v_.end);
            return v.begin + n;
        } catch (...) {
            alloc_traits::deallocate(*this, v.begin, static_cast<size_type>(v.boundary - v.begin));
            throw;
        }
    }

    template<typename RandIt>
    pointer insert_relocate(vector_ptrs_t& v, pointer p, size_type count, RandIt src,
                            std::false_type /* nothrow move */) {
        try {
            size_type n = static_cast<size_type>(p - v_.begin);
            v.end = helpers::construct_relocate_copy(*this, v.end, v_.begin, p);
            v.end = helpers::construct_copy(*this, v.end, count, src);
            v.end = helpers::construct_relocate_copy(*this, v.end, p, v_.end);
            return v.begin + n;
        } catch (...) {
            alloc_traits::deallocate(*this, v.begin, static_cast<size_type>(v.boundary - v.begin));
            throw;
        }
    }

    template<typename RandIt>
    void insert_no_relocate(pointer p, size_type count, RandIt src, std::true_type /* assignable */) {
        size_type tail = static_cast<size_type>(v_.end - p);
        if (tail == 0) {
            v_.end = helpers::construct_copy(*this, v_.end, count, src);
        } else if (count > tail) {
            v_.end = helpers::construct_copy(
                *this, v_.end, count - tail,
                src + static_cast<typename std::iterator_traits<RandIt>::difference_type>(tail));
            v_.end = helpers::construct_relocate(*this, v_.end, p, p + tail);
            std::copy_n(src, tail, p);
        } else {
            helpers::construct_relocate(*this, v_.end, v_.end - count, v_.end);
            std::move_backward(p, v_.end - count, v_.end);
            std::copy_n(src, count, p);
            v_.end += count;
        }
    }

    template<typename RandIt>
    void insert_no_relocate(pointer p, size_type count, RandIt src, std::false_type /* assignable */) {
        v_.end = helpers::construct_copy(*this, v_.end, count, src);
        std::rotate(p, v_.end - count, v_.end);
    }

    template<typename RandIt>
    pointer insert_range(pointer p, RandIt first, RandIt last, std::true_type /* random access iterator */) {
        assert(first <= last);
        return insert_copy(p, static_cast<size_type>(last - first), first, std::is_assignable<Ty&, decltype(*first)>());
    }

    template<typename InputIt>
    pointer insert_range(pointer p, InputIt first, InputIt last, std::false_type /* random access iterator */) {
        size_type old_sz = size(), n = static_cast<size_type>(p - v_.begin);
        for (; first != last; ++first) { emplace_back(*first); }
        size_type count = size() - old_sz;
        std::rotate(v_.begin + n, v_.end - count, v_.end);
        return v_.begin + n;
    }

    struct helpers {
        static pointer construct_default(alloc_type& alloc, pointer first, size_type n) {
            pointer p = first, last = first + n;
            try {
                for (; p != last; ++p) { alloc_traits::construct(alloc, std::addressof(*p)); }
                return last;
            } catch (...) {
                helpers::truncate(alloc, first, p);
                throw;
            }
        }

        template<typename InputIt>
        static pointer construct_copy(std::allocator<Ty>& alloc, pointer first, size_type n, InputIt src) {
            std::uninitialized_copy_n(src, n, first);
            return first + n;
        }

        template<typename Ty_ = Ty, typename InputIt>
        static pointer construct_copy(
            std::enable_if_t<!std::is_same<alloc_type, std::allocator<Ty_>>::value, alloc_type>& alloc, pointer first,
            size_type n, InputIt src) {
            pointer p = first, last = first + n;
            try {
                for (; p != last; ++p) {
                    alloc_traits::construct(alloc, std::addressof(*p), *src);
                    ++src;
                }
                return last;
            } catch (...) {
                helpers::truncate(alloc, first, p);
                throw;
            }
        }

        template<typename Ty_ = Ty>
        static pointer construct_relocate(std::allocator<Ty_>& alloc, pointer dst, pointer first, pointer last) {
            return std::uninitialized_copy(std::make_move_iterator(first), std::make_move_iterator(last), dst);
        }

        template<typename Ty_ = Ty>
        static pointer construct_relocate(
            std::enable_if_t<!std::is_same<alloc_type, std::allocator<Ty_>>::value, alloc_type>& alloc, pointer dst,
            pointer first, pointer last) {
            pointer p = first;
            try {
                for (; p != last; ++dst, ++p) { alloc_traits::construct(alloc, std::addressof(*dst), std::move(*p)); }
                return dst;
            } catch (...) {
                helpers::truncate(alloc, first, p);
                throw;
            }
        }

        static pointer construct_relocate_copy(alloc_type& alloc, pointer dst, pointer first, pointer last) {
            return helpers::construct_relocate_copy_impl(alloc, dst, first, last, std::is_copy_constructible<Ty>());
        }

        static pointer construct_relocate_copy_impl(alloc_type& alloc, pointer dst, pointer first, pointer last,
                                                    std::true_type /* use copy constructor */) {
            pointer p = first;
            try {
                for (; p != last; ++dst, ++p) {
                    alloc_traits::construct(alloc, std::addressof(*dst), static_cast<const Ty&>(*p));
                }
                return dst;
            } catch (...) {
                helpers::truncate(alloc, first, p);
                throw;
            }
        }

        static pointer construct_relocate_copy_impl(alloc_type& alloc, pointer dst, pointer first, pointer last,
                                                    std::false_type /* use copy constructor */) {
            return helpers::construct_relocate(alloc, dst, first, last);
        }

        static pointer truncate(alloc_type& alloc, pointer first, pointer last) {
            return helpers::truncate_impl(alloc, first, last, std::is_trivially_destructible<Ty>());
        }

        static pointer truncate_impl(std::allocator<Ty>& alloc, pointer first, pointer last,
                                     std::true_type /* trivially destructible */) {
            return first;
        }

        template<typename Ty_ = Ty>
        static pointer truncate_impl(
            std::enable_if_t<!std::is_same<alloc_type, std::allocator<Ty_>>::value, alloc_type>& alloc, pointer first,
            pointer last, std::true_type /* trivially destructible */) {
            return helpers::truncate_impl(alloc, first, last, std::false_type());
        }

        static pointer truncate_impl(alloc_type& alloc, pointer first, pointer last,
                                     std::false_type /* trivially destructible */) {
            for (auto p = first; p != last; ++p) { alloc_traits::destroy(alloc, std::addressof(*p)); }
            return first;
        }

        static void emplace(alloc_type& alloc, pointer pos, pointer end, Ty&& val) {
            assert(pos != end);
            alloc_traits::construct(alloc, std::addressof(*end), std::move(*(end - 1)));
            std::move_backward(pos, end - 1, end);
            *pos = std::move(val);
        }

        template<typename... Args>
        static void emplace(alloc_type& alloc, pointer pos, pointer end, Args&&... args) {
            typename std::aligned_storage<sizeof(Ty), std::alignment_of<Ty>::value>::type buf;
            auto* val = reinterpret_cast<Ty*>(std::addressof(buf));
            alloc_traits::construct(alloc, val, std::forward<Args>(args)...);
            try {
                helpers::emplace(alloc, pos, end, std::move(*val));
                alloc_traits::destroy(alloc, val);
            } catch (...) {
                alloc_traits::destroy(alloc, val);
                throw;
            }
        }
    };
};

#if __cplusplus >= 201703L
template<typename InputIt, typename Alloc = std::allocator<typename std::iterator_traits<InputIt>::value_type>,
         typename = std::enable_if_t<is_allocator<Alloc>::value>>
vector(InputIt, InputIt, Alloc = Alloc()) -> vector<typename std::iterator_traits<InputIt>::value_type, Alloc>;
#endif  // __cplusplus >= 201703L

template<typename Ty, typename Alloc>
bool operator==(const vector<Ty, Alloc>& lhs, const vector<Ty, Alloc>& rhs) {
    if (lhs.size() != rhs.size()) { return false; }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template<typename Ty, typename Alloc>
bool operator<(const vector<Ty, Alloc>& lhs, const vector<Ty, Alloc>& rhs) {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template<typename Ty, typename Alloc>
bool operator!=(const vector<Ty, Alloc>& lhs, const vector<Ty, Alloc>& rhs) {
    return !(lhs == rhs);
}
template<typename Ty, typename Alloc>
bool operator<=(const vector<Ty, Alloc>& lhs, const vector<Ty, Alloc>& rhs) {
    return !(rhs < lhs);
}
template<typename Ty, typename Alloc>
bool operator>(const vector<Ty, Alloc>& lhs, const vector<Ty, Alloc>& rhs) {
    return rhs < lhs;
}
template<typename Ty, typename Alloc>
bool operator>=(const vector<Ty, Alloc>& lhs, const vector<Ty, Alloc>& rhs) {
    return !(lhs < rhs);
}

}  // namespace uxs

namespace std {
template<typename Ty, typename Alloc>
void swap(uxs::vector<Ty, Alloc>& v1, uxs::vector<Ty, Alloc>& v2) NOEXCEPT_IF(NOEXCEPT_IF(v1.swap(v2))) {
    v1.swap(v2);
}
}  // namespace std
