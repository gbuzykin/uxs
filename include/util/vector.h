#pragma once

#include "allocator.h"
#include "iterator.h"

#include <algorithm>

namespace util {

//-----------------------------------------------------------------------------
// Vector implementation

template<typename Ty, typename Alloc = std::allocator<Ty>>
class vector : public std::allocator_traits<Alloc>::template rebind_alloc<Ty> {
 private:
    static_assert(std::is_same<typename std::remove_cv<Ty>::type, Ty>::value,
                  "util::vector must have a non-const, non-volatile value type");

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
        try {
            init_default(sz);
        } catch (...) {
            tidy();
            throw;
        }
    }

    vector(size_type sz, const value_type& val, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        try {
            init(sz, const_value(val));
        } catch (...) {
            tidy();
            throw;
        }
    }

    vector(std::initializer_list<value_type> l, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        try {
            init(l.size(), l.begin());
        } catch (...) {
            tidy();
            throw;
        }
    }

    vector& operator=(std::initializer_list<value_type> l) {
        assign_impl(l.size(), l.begin(), std::is_copy_assignable<Ty>());
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    vector(InputIt first, InputIt last, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        try {
            init_from_range(first, last, is_random_access_iterator<InputIt>());
        } catch (...) {
            tidy();
            throw;
        }
    }

    vector(const vector& other) : alloc_type(alloc_traits::select_on_container_copy_construction(other)) {
        try {
            init(other.size(), other.begin());
        } catch (...) {
            tidy();
            throw;
        }
    }

    vector(const vector& other, const allocator_type& alloc) : alloc_type(alloc) {
        try {
            init(other.size(), other.begin());
        } catch (...) {
            tidy();
            throw;
        }
    }

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

    allocator_type get_allocator() const { return static_cast<const alloc_type&>(*this); }

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
        if (i >= size()) { throw std::out_of_range("invalid vector index"); }
        return v_.begin[i];
    }
    const_reference at(size_type i) const {
        if (i >= size()) { throw std::out_of_range("invalid vector index"); }
        return v_.begin[i];
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

    void assign(size_type sz, const value_type& val) {
        assign_impl(sz, const_value(val), std::is_copy_assignable<Ty>());
    }

    void assign(std::initializer_list<value_type> l) {
        assign_impl(l.size(), l.begin(), std::is_copy_assignable<Ty>());
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    void assign(InputIt first, InputIt last) {
        assign_from_range(first, last, is_random_access_iterator<InputIt>(),
                          std::is_assignable<Ty&, decltype(*first)>());
    }

    void clear() NOEXCEPT { helpers::truncate(*this, v_.begin, v_.end, std::is_trivially_destructible<value_type>()); }

    void reserve(size_type reserve_sz) {
        if (reserve_sz <= capacity()) { return; }
        auto v = alloc_new(reserve_sz);
        relocate_to(v, std::is_nothrow_move_constructible<Ty>());
        reset(v);
    }

    void shrink_to_fit() {
        if (v_.end == v_.boundary) { return; }
        vector_ptrs_t v;
        if (v_.begin != v_.end) {
            v = alloc_new(size());
            relocate_to(v, std::is_nothrow_move_constructible<Ty>());
        }
        reset(v);
    }

    void resize(size_type sz) {
        if (sz > size()) {
            size_type count = sz - size();
            if (count > static_cast<size_type>(v_.boundary - v_.end)) {
                auto v = alloc_new(grow_capacity(count));
                relocate_to_and_append_default(v, count, std::is_nothrow_move_constructible<Ty>(),
                                               std::is_nothrow_default_constructible<Ty>());
                reset(v);
            } else {
                helpers::append_default(*this, v_.end, v_.end + count);
            }
        } else {
            helpers::truncate(*this, v_.begin + sz, v_.end, std::is_trivially_destructible<value_type>());
        }
    }

    void resize(size_type sz, const value_type& val) {
        if (sz > size()) {
            size_type count = sz - size();
            if (count > static_cast<size_type>(v_.boundary - v_.end)) {
                auto v = alloc_new(grow_capacity(count));
                relocate_to_and_append(v, count, val, std::is_nothrow_move_constructible<Ty>(),
                                       std::is_nothrow_copy_constructible<Ty>());
                reset(v);
            } else {
                helpers::append_copy(*this, v_.end, v_.end + count, const_value(val));
            }
        } else {
            helpers::truncate(*this, v_.begin + sz, v_.end, std::is_trivially_destructible<value_type>());
        }
    }

    iterator insert(const_iterator pos, size_type count, const value_type& val) {
        auto p = insert_from_const(to_ptr(pos), count, val, std::is_copy_assignable<Ty>());
        return iterator(p, v_.begin, v_.end);
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        auto p = insert_from_range(to_ptr(pos), first, last, is_random_access_iterator<InputIt>(),
                                   std::is_assignable<Ty&, decltype(*first)>());
        return iterator(p, v_.begin, v_.end);
    }

    iterator insert(const_iterator pos, std::initializer_list<value_type> l) {
        auto p = insert_impl(to_ptr(pos), l.size(), l.begin(), std::is_copy_assignable<Ty>());
        return iterator(p, v_.begin, v_.end);
    }

    iterator insert(const_iterator pos, const value_type& val) { return emplace(pos, val); }
    iterator insert(const_iterator pos, value_type&& val) { return emplace(pos, std::move(val)); }
    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        auto p = to_ptr(pos);
        if (v_.end == v_.boundary) {
            auto v = alloc_new(grow_capacity(1));
            p = relocate_to_and_emplace(v, p, std::is_nothrow_move_constructible<Ty>(),
                                        std::is_nothrow_constructible<Ty, Args...>(), std::forward<Args>(args)...);
            reset(v);
        } else if (p != v_.end) {
            helpers::emplace(*this, p, v_.end, std::forward<Args>(args)...);
        } else {
            helpers::append(*this, v_.end, std::forward<Args>(args)...);
        }
        return iterator(p, v_.begin, v_.end);
    }

    void push_back(const value_type& val) { emplace_back(val); }
    void push_back(value_type&& val) { emplace_back(std::move(val)); }
    template<typename... Args>
    reference emplace_back(Args&&... args) {
        if (v_.end == v_.boundary) {
            auto v = alloc_new(grow_capacity(1));
            relocate_to_and_append(v, std::is_nothrow_move_constructible<Ty>(),
                                   std::is_nothrow_constructible<Ty, Args...>(), std::forward<Args>(args)...);
            reset(v);
        } else {
            helpers::append(*this, v_.end, std::forward<Args>(args)...);
        }
        return *(v_.end - 1);
    }

    void pop_back() {
        assert(v_.begin != v_.end);
        helpers::truncate(*this, v_.end);
    }

    iterator erase(const_iterator pos) {
        auto p = to_ptr(pos);
        assert(p != v_.end);
        helpers::erase(*this, p, v_.end);
        return iterator(p, v_.begin, v_.end);
    }

    iterator erase(const_iterator first, const_iterator last) {
        auto p_first = to_ptr(first);
        auto p_last = to_ptr(last);
        assert(v_.begin <= p_first && p_first <= p_last && p_last <= v_.end);
        size_type count = static_cast<size_type>(p_last - p_first);
        if (count) { helpers::erase(*this, p_first, v_.end - count, v_.end); }
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
        size_t sz = size(), delta_sz = std::max(extra, sz >> 1);
        if (delta_sz > alloc_traits::max_size(*this) - sz) {
            if (extra > alloc_traits::max_size(*this) - sz) { throw std::length_error("too much to reserve"); }
            delta_sz = extra;
        }
        return std::max<size_type>(sz + delta_sz, kStartCapacity);
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
        helpers::truncate(*this, v.begin, v.end, std::is_trivially_destructible<Ty>());
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
            try {
                init(other.size(), std::make_move_iterator(other.begin()));
            } catch (...) {
                tidy();
                throw;
            }
        }
    }

    void assign_impl(const vector& other, std::true_type) {
        assign_impl(other.size(), other.begin(), std::is_copy_assignable<Ty>());
    }

    void assign_impl(const vector& other, std::false_type) {
        if (is_same_alloc(other)) {
            assign_impl(other.size(), other.begin(), std::is_copy_assignable<Ty>());
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
            assign_impl(other.size(), std::make_move_iterator(other.begin()), std::is_move_assignable<Ty>());
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
        v_ = alloc_new(sz);
        helpers::append_default(*this, v_.end, v_.end + sz);
    }

    template<typename RandIt>
    void init(size_type sz, RandIt src) {
        assert(v_.begin == nullptr);
        if (!sz) { return; }
        v_ = alloc_new(sz);
        helpers::append_copy(*this, v_.end, v_.end + sz, src);
    }

    template<typename RandIt>
    void init_from_range(RandIt first, RandIt last, std::true_type /* random access iterator */) {
        assert(first <= last);
        init(static_cast<size_type>(last - first), first);
    }

    template<typename InputIt>
    void init_from_range(InputIt first, InputIt last, std::false_type /* random access iterator */) {
        assert(v_.begin == nullptr);
        for (; first != last; ++first) { emplace_back(*first); }
    }

    void relocate_to(vector_ptrs_t& v, std::true_type /* nothrow move constructible */) NOEXCEPT {
        helpers::append_relocate(*this, v.end, v_.begin, v_.end);
    }

    void relocate_to(vector_ptrs_t& v, std::false_type /* nothrow move constructible */) {
        try {
            helpers::append_relocate(*this, v.end, v_.begin, v_.end, std::is_copy_constructible<Ty>());
        } catch (...) {
            tidy(v);
            throw;
        }
    }

    void relocate_to_and_append_default(vector_ptrs_t& v, size_type count,
                                        std::true_type /* nothrow move constructible */,
                                        std::true_type /* nothrow default constructible */) NOEXCEPT {
        helpers::append_relocate(*this, v.end, v_.begin, v_.end);
        helpers::append_default(*this, v.end, v.end + count);
    }

    void relocate_to_and_append_default(vector_ptrs_t& v, size_type count,
                                        std::true_type /* nothrow move constructible */,
                                        std::false_type /* nothrow default constructible */
    ) {
        auto end = v.begin + size();
        try {
            helpers::append_default(*this, end, end + count);
        } catch (...) {
            helpers::truncate(*this, v.begin + size(), end, std::is_trivially_destructible<Ty>());
            alloc_traits::deallocate(*this, v.begin, static_cast<size_type>(v.boundary - v.begin));
            throw;
        }
        helpers::append_relocate(*this, v.end, v_.begin, v_.end);
        v.end = end;
    }

    template<typename Bool>
    void relocate_to_and_append_default(vector_ptrs_t& v, size_type count,
                                        std::false_type /* nothrow move constructible */,
                                        Bool /* nothrow default constructible */) {
        try {
            helpers::append_relocate(*this, v.end, v_.begin, v_.end, std::is_copy_constructible<Ty>());
            helpers::append_default(*this, v.end, v.end + count);
        } catch (...) {
            tidy(v);
            throw;
        }
    }

    void relocate_to_and_append(vector_ptrs_t& v, size_type count, const value_type& val,
                                std::true_type /* nothrow move constructible */,
                                std::true_type /* nothrow copy constructible */) NOEXCEPT {
        helpers::append_relocate(*this, v.end, v_.begin, v_.end);
        helpers::append_copy(*this, v.end, v.end + count, const_value(val));
    }

    void relocate_to_and_append(vector_ptrs_t& v, size_type count, const value_type& val,
                                std::true_type /* nothrow move constructible */,
                                std::false_type /* nothrow copy constructible */) {
        auto end = v.begin + size();
        try {
            helpers::append_copy(*this, end, end + count, const_value(val));
        } catch (...) {
            helpers::truncate(*this, v.begin + size(), end, std::is_trivially_destructible<Ty>());
            alloc_traits::deallocate(*this, v.begin, static_cast<size_type>(v.boundary - v.begin));
            throw;
        }
        helpers::append_relocate(*this, v.end, v_.begin, v_.end);
        v.end = end;
    }

    template<typename Bool>
    void relocate_to_and_append(vector_ptrs_t& v, size_type count, const value_type& val,
                                std::false_type /* nothrow move constructible */,
                                Bool /* nothrow copy constructible */) {
        try {
            helpers::append_relocate(*this, v.end, v_.begin, v_.end, std::is_copy_constructible<Ty>());
            helpers::append_copy(*this, v.end, v.end + count, const_value(val));
        } catch (...) {
            tidy(v);
            throw;
        }
    }

    template<typename... Args>
    void relocate_to_and_append(vector_ptrs_t& v, std::true_type /* nothrow move constructible */,
                                std::true_type /* nothrow constructible */, Args&&... args) NOEXCEPT {
        helpers::append_relocate(*this, v.end, v_.begin, v_.end);
        helpers::append(*this, v.end, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void relocate_to_and_append(vector_ptrs_t& v, std::true_type /* nothrow move constructible */,
                                std::false_type /* nothrow constructible */, Args&&... args) {
        auto end = v.begin + size();
        try {
            helpers::append(*this, end, std::forward<Args>(args)...);
        } catch (...) {
            alloc_traits::deallocate(*this, v.begin, static_cast<size_type>(v.boundary - v.begin));
            throw;
        }
        helpers::append_relocate(*this, v.end, v_.begin, v_.end);
        v.end = end;
    }

    template<typename Bool, typename... Args>
    void relocate_to_and_append(vector_ptrs_t& v, std::false_type /* nothrow move constructible */,
                                Bool /* nothrow constructible */, Args&&... args) {
        try {
            helpers::append_relocate(*this, v.end, v_.begin, v_.end, std::is_copy_constructible<Ty>());
            helpers::append(*this, v.end, std::forward<Args>(args)...);
        } catch (...) {
            tidy(v);
            throw;
        }
    }

    template<typename... Args>
    pointer relocate_to_and_emplace(vector_ptrs_t& v, pointer p, std::true_type /* nothrow move constructible */,
                                    std::true_type /* nothrow constructible */, Args&&... args) NOEXCEPT {
        helpers::append_relocate(*this, v.end, v_.begin, p);
        helpers::append(*this, v.end, std::forward<Args>(args)...);
        helpers::append_relocate(*this, v.end, p, v_.end);
        return v.begin + static_cast<size_type>(p - v_.begin);
    }

    template<typename... Args>
    pointer relocate_to_and_emplace(vector_ptrs_t& v, pointer p, std::true_type /* nothrow move constructible */,
                                    std::false_type /* nothrow constructible */, Args&&... args) {
        size_type n = static_cast<size_type>(p - v_.begin);
        auto mid = v.begin + n;
        try {
            helpers::append(*this, mid, std::forward<Args>(args)...);
        } catch (...) {
            alloc_traits::deallocate(*this, v.begin, static_cast<size_type>(v.boundary - v.begin));
            throw;
        }
        helpers::append_relocate(*this, v.end, v_.begin, p);
        helpers::append_relocate(*this, mid, p, v_.end);
        v.end = mid;
        return v.begin + n;
    }

    template<typename Bool, typename... Args>
    pointer relocate_to_and_emplace(vector_ptrs_t& v, pointer p, std::false_type /* nothrow move constructible */,
                                    Bool /* nothrow constructible */, Args&&... args) {
        try {
            helpers::append_relocate(*this, v.end, v_.begin, p, std::is_copy_constructible<Ty>());
            helpers::append(*this, v.end, std::forward<Args>(args)...);
            helpers::append_relocate(*this, v.end, p, v_.end, std::is_copy_constructible<Ty>());
        } catch (...) {
            tidy(v);
            throw;
        }
        return v.begin + static_cast<size_type>(p - v_.begin);
    }

    template<typename RandIt>
    void assign_impl(size_type sz, RandIt src, std::true_type /* assignable */) {
        if (sz > capacity()) {
            assign_relocate(sz, src, std::is_nothrow_constructible<Ty, decltype(*src)>());
        } else if (sz > size()) {
            for (auto p = v_.begin; p != v_.end; ++src) { *p++ = *src; }
            helpers::append_copy(*this, v_.end, v_.begin + sz, src);
        } else {
            auto end_new = v_.begin + sz;
            for (auto p = v_.begin; p != end_new; ++src) { *p++ = *src; }
            helpers::truncate(*this, end_new, v_.end, std::is_trivially_destructible<Ty>());
        }
    }

    template<typename RandIt>
    void assign_impl(size_type sz, RandIt src, std::false_type /* assignable */) {
        if (sz > capacity()) {
            assign_relocate(sz, src, std::is_nothrow_constructible<Ty, decltype(*src)>());
        } else {
            helpers::truncate(*this, v_.begin, v_.end, std::is_trivially_destructible<Ty>());
            helpers::append_copy(*this, v_.end, v_.end + sz, src);
        }
    }

    template<typename RandIt>
    void assign_relocate(size_type sz, RandIt src, std::true_type /* nothrow constructible */) NOEXCEPT {
        auto v = alloc_new(sz);
        helpers::append_copy(*this, v.end, v.end + sz, src);
        reset(v);
    }

    template<typename RandIt>
    void assign_relocate(size_type sz, RandIt src, std::false_type /* nothrow constructible */) {
        auto v = alloc_new(sz);
        try {
            helpers::append_copy(*this, v.end, v.end + sz, src);
        } catch (...) {
            tidy(v);
            throw;
        }
        reset(v);
    }

    template<typename RandIt, typename Bool>
    void assign_from_range(RandIt first, RandIt last, std::true_type /* random access iterator */,
                           Bool /* assignable */) {
        assert(first <= last);
        assign_impl(static_cast<size_type>(last - first), first, Bool());
    }

    template<typename InputIt>
    void assign_from_range(InputIt first, InputIt last, std::false_type /* random access iterator */,
                           std::true_type /* assignable */) {
        auto end_new = v_.begin;
        for (; end_new != v_.end && first != last; ++first) { *end_new++ = *first; }
        if (end_new != v_.end) {
            helpers::truncate(*this, end_new, v_.end, std::is_trivially_destructible<Ty>());
        } else {
            for (; first != last; ++first) { emplace_back(*first); }
        }
    }

    template<typename InputIt>
    void assign_from_range(InputIt first, InputIt last, std::false_type /* random access iterator */,
                           std::false_type /* assignable */) {
        helpers::truncate(*this, v_.begin, v_.end, std::is_trivially_destructible<Ty>());
        for (; first != last; ++first) { emplace_back(*first); }
    }

    template<typename RandIt, typename Bool>
    pointer insert_impl(pointer p, size_type count, RandIt src, Bool /* assignable */) {
        if (count > static_cast<size_type>(v_.boundary - v_.end)) {
            return insert_relocate(p, count, src, std::is_nothrow_move_constructible<Ty>(),
                                   std::is_nothrow_constructible<Ty, decltype(*src)>());
        } else if (count) {
            insert_no_relocate(p, count, src, Bool());
        }
        return p;
    }

    template<typename Bool>
    pointer insert_from_const(pointer p, size_type count, const value_type& val, Bool /* assignable */) {
        if (count > static_cast<size_type>(v_.boundary - v_.end)) {
            return insert_relocate(p, count, const_value(val), std::is_nothrow_move_constructible<Ty>(),
                                   std::is_nothrow_copy_constructible<Ty>());
        } else if (count) {
            typename std::aligned_storage<sizeof(Ty), std::alignment_of<Ty>::value>::type buf;
            auto* val_copy = reinterpret_cast<value_type*>(std::addressof(buf));
            alloc_traits::construct(*this, val_copy, val);
            try {
                insert_no_relocate(p, count, const_value(*val_copy), Bool());
            } catch (...) {
                alloc_traits::destroy(*this, val_copy);
                throw;
            }
            alloc_traits::destroy(*this, val_copy);
        }
        return p;
    }

    template<typename RandIt>
    pointer insert_relocate(pointer p, size_type count, RandIt src, std::true_type /* nothrow move constructible */,
                            std::true_type /* nothrow constructible */) {
        size_type n = static_cast<size_type>(p - v_.begin);
        auto v = alloc_new(grow_capacity(count));
        helpers::append_relocate(*this, v.end, v_.begin, p);
        helpers::append_copy(*this, v.end, v.end + count, src);
        helpers::append_relocate(*this, v.end, p, v_.end);
        reset(v);
        return v_.begin + n;
    }

    template<typename RandIt>
    pointer insert_relocate(pointer p, size_type count, RandIt src, std::true_type /* nothrow move constructible */,
                            std::false_type /* nothrow constructible */) {
        size_type n = static_cast<size_type>(p - v_.begin);
        auto v = alloc_new(grow_capacity(count));
        auto mid = v.begin + n;
        try {
            helpers::append_copy(*this, mid, mid + count, src);
        } catch (...) {
            helpers::truncate(*this, v.begin + n, mid, std::is_trivially_destructible<Ty>());
            alloc_traits::deallocate(*this, v.begin, static_cast<size_type>(v.boundary - v.begin));
            throw;
        }
        helpers::append_relocate(*this, v.end, v_.begin, p);
        helpers::append_relocate(*this, mid, p, v_.end);
        v.end = mid;
        reset(v);
        return v_.begin + n;
    }

    template<typename RandIt, typename Bool>
    pointer insert_relocate(pointer p, size_type count, RandIt src, std::false_type /* nothrow move constructible */,
                            Bool /* nothrow constructible */) {
        size_type n = static_cast<size_type>(p - v_.begin);
        auto v = alloc_new(grow_capacity(count));
        try {
            helpers::append_relocate(*this, v.end, v_.begin, p, std::is_copy_constructible<Ty>());
            helpers::append_copy(*this, v.end, v.end + count, src);
            helpers::append_relocate(*this, v.end, p, v_.end, std::is_copy_constructible<Ty>());
        } catch (...) {
            tidy(v);
            throw;
        }
        reset(v);
        return v_.begin + n;
    }

    template<typename RandIt>
    void insert_no_relocate(pointer p, size_type count, RandIt src, std::true_type /* assignable */) {
        if (p != v_.end) {
            helpers::insert_copy(*this, p, v_.end, v_.end + count, src);
        } else {
            helpers::append_copy(*this, v_.end, v_.end + count, src);
        }
    }

    template<typename RandIt>
    void insert_no_relocate(pointer p, size_type count, RandIt src, std::false_type /* assignable */) {
        helpers::append_copy(*this, v_.end, v_.end + count, src);
        std::rotate(p, v_.end - count, v_.end);
    }

    template<typename RandIt, typename Bool>
    pointer insert_from_range(pointer p, RandIt first, RandIt last, std::true_type /* random access iterator */,
                              Bool /* assignable */) {
        assert(first <= last);
        return insert_impl(p, static_cast<size_type>(last - first), first, Bool());
    }

    template<typename InputIt, typename Bool>
    pointer insert_from_range(pointer p, InputIt first, InputIt last, std::false_type /* random access iterator */,
                              Bool /* assignable */) {
        size_type count = 0;
        size_type n = static_cast<size_type>(p - v_.begin);
        for (; first != last; ++first) {
            emplace_back(*first);
            ++count;
        }
        std::rotate(v_.begin + n, v_.end - count, v_.end);
        return v_.begin + n;
    }

    struct helpers {
        template<typename... Args>
        static void append(alloc_type& alloc, pointer& end, Args&&... args)
            NOEXCEPT_IF((std::is_nothrow_constructible<Ty, Args...>::value)) {
            alloc_traits::construct(alloc, std::addressof(*end), std::forward<Args>(args)...);
            ++end;
        }

        static void append_default(alloc_type& alloc, pointer& end, pointer end_new)
            NOEXCEPT_IF(std::is_nothrow_default_constructible<Ty>::value) {
            for (; end != end_new; ++end) { alloc_traits::construct(alloc, std::addressof(*end)); }
        }

        template<typename InputIt>
        static void append_copy(alloc_type& alloc, pointer& end, pointer end_new, InputIt src)
            NOEXCEPT_IF((std::is_nothrow_constructible<Ty, decltype(*src)>::value)) {
            for (; end != end_new; ++end) {
                alloc_traits::construct(alloc, std::addressof(*end), *src);
                ++src;
            }
        }

        static void append_relocate(alloc_type& alloc, pointer& end, pointer src_first, pointer src_last,
                                    std::false_type = std::false_type() /* use copy constructor */)
            NOEXCEPT_IF(std::is_nothrow_move_constructible<Ty>::value) {
            for (; src_first != src_last; ++end, ++src_first) {
                alloc_traits::construct(alloc, std::addressof(*end), std::move(*src_first));
            }
        }

        static void append_relocate(alloc_type& alloc, pointer& end, pointer src_first, pointer src_last,
                                    std::true_type /* use copy constructor */)
            NOEXCEPT_IF(std::is_nothrow_copy_constructible<Ty>::value) {
            for (; src_first != src_last; ++end, ++src_first) {
                alloc_traits::construct(alloc, std::addressof(*end), static_cast<const value_type&>(*src_first));
            }
        }

        static void truncate(alloc_type& alloc, pointer& end) {
            alloc_traits::destroy(alloc, std::addressof(*(--end)));
        }

        static void truncate(std::allocator<Ty>& alloc, pointer end_new, pointer& end,
                             std::true_type /* trivially destructible */) {
            end = end_new;
        }

        template<typename Alloc_ = alloc_type>
        static void truncate(std::enable_if_t<!std::is_same<Alloc_, std::allocator<Ty>>::value, alloc_type>& alloc,
                             pointer end_new, pointer& end, std::true_type /* trivially destructible */) {
            helpers::truncate(alloc, end_new, end, std::false_type());
        }

        static void truncate(alloc_type& alloc, pointer end_new, pointer& end,
                             std::false_type /* trivially destructible */) {
            for (auto p = end_new; p != end; ++p) { alloc_traits::destroy(alloc, std::addressof(*p)); }
            end = end_new;
        }

        static void emplace(alloc_type& alloc, pointer pos, pointer& end, value_type&& val) {
            assert(pos != end);
            helpers::append(alloc, end, std::move(*(end - 1)));
            for (auto p = end - 2; pos != p; --p) { *p = std::move(*(p - 1)); }
            *pos = std::move(val);
        }

        template<typename... Args>
        static void emplace(alloc_type& alloc, pointer pos, pointer& end, Args&&... args) {
            typename std::aligned_storage<sizeof(Ty), std::alignment_of<Ty>::value>::type buf;
            auto* val = reinterpret_cast<value_type*>(std::addressof(buf));
            alloc_traits::construct(alloc, val, std::forward<Args>(args)...);
            try {
                helpers::emplace(alloc, pos, end, std::move(*val));
            } catch (...) {
                alloc_traits::destroy(alloc, val);
                throw;
            }
            alloc_traits::destroy(alloc, val);
        }

        template<typename RandIt>
        static void insert_copy(alloc_type& alloc, pointer pos, pointer& end, pointer end_new, RandIt src) {
            assert(pos != end && end != end_new);
            size_type count = static_cast<size_type>(end_new - end);
            size_type tail = static_cast<size_type>(end - pos);
            if (count > tail) {
                auto p = end;
                helpers::append_copy(alloc, end, end + count - tail,
                                     src + static_cast<typename std::iterator_traits<RandIt>::difference_type>(tail));
                helpers::append_relocate(alloc, end, p - tail, p);
                for (p = pos; p != pos + tail; ++src) { *p++ = *src; };
            } else {
                auto p = end - count;
                helpers::append_relocate(alloc, end, end - count, end);
                for (; pos != p; --p) { *(p + count - 1) = std::move(*(p - 1)); }
                for (; p != pos + count; ++src) { *p++ = *src; };
            }
        }

        static void erase(alloc_type& alloc, pointer p, pointer& end) {
            for (; p != end - 1; ++p) { *p = std::move(*(p + 1)); }
            helpers::truncate(alloc, end);
        }

        static void erase(alloc_type& alloc, pointer p, pointer end_new, pointer& end) {
            assert(end_new != end);
            size_type count = static_cast<size_type>(end - end_new);
            for (; p != end_new; ++p) { *p = std::move(*(p + count)); }
            helpers::truncate(alloc, end_new, end, std::is_trivially_destructible<Ty>());
        }
    };
};

#if __cplusplus >= 201703L
template<typename InputIt, typename Alloc = std::allocator<typename std::iterator_traits<InputIt>::value_type>>
vector(InputIt, InputIt, Alloc = Alloc()) -> vector<typename std::iterator_traits<InputIt>::value_type, Alloc>;

template<typename Ty, typename Alloc = std::allocator<Ty>>
vector(typename vector<Ty>::size_type, Ty, Alloc = Alloc()) -> vector<Ty, Alloc>;
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

}  // namespace util

namespace std {
template<typename Ty, typename Alloc>
void swap(util::vector<Ty, Alloc>& v1, util::vector<Ty, Alloc>& v2) NOEXCEPT_IF(NOEXCEPT_IF(v1.swap(v2))) {
    v1.swap(v2);
}
}  // namespace std
