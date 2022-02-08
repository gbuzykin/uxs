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
    using use_move_for_relocate =
        std::bool_constant<(std::is_nothrow_move_constructible<Ty>::value || !std::is_copy_constructible<Ty>::value)>;

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
        tidy_invoke([&]() { init_default(sz); });
    }

    vector(size_type sz, const value_type& val, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        tidy_invoke([&]() { init(sz, const_value(val)); });
    }

    vector(std::initializer_list<value_type> l, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        tidy_invoke([&]() { init(l.size(), l.begin()); });
    }

    vector& operator=(std::initializer_list<value_type> l) {
        assign_impl(l.size(), l.begin(), std::is_copy_assignable<Ty>());
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    vector(InputIt first, InputIt last, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        tidy_invoke([&]() { init_from_range(first, last, is_random_access_iterator<InputIt>()); });
    }

    vector(const vector& other) : alloc_type(alloc_traits::select_on_container_copy_construction(other)) {
        tidy_invoke([&]() { init(other.size(), other.begin()); });
    }

    vector(const vector& other, const allocator_type& alloc) : alloc_type(alloc) {
        tidy_invoke([&]() { init(other.size(), other.begin()); });
    }

    vector& operator=(const vector& other) {
        if (std::addressof(other) == this) { return *this; }
        assign_impl(other, std::bool_constant<(!alloc_traits::propagate_on_container_copy_assignment::value ||
                                               is_alloc_always_equal<alloc_type>::value)>());
        return *this;
    }

    vector(vector&& other) NOEXCEPT : alloc_type(std::move(other)) { steal_data(other); }

    vector(vector&& other, const allocator_type& alloc) : alloc_type(alloc) {
        if (is_alloc_always_equal<alloc_type>::value || is_same_alloc(other)) {
            steal_data(other);
        } else {
            tidy_invoke([&]() { init(other.size(), std::make_move_iterator(other.begin())); });
        }
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

    bool empty() const NOEXCEPT { return end_ == begin_; }
    size_type size() const NOEXCEPT { return static_cast<size_type>(end_ - begin_); }
    size_type capacity() const NOEXCEPT { return static_cast<size_type>(boundary_ - begin_); }
    size_type max_size() const NOEXCEPT { return alloc_traits::max_size(*this); }

    iterator begin() NOEXCEPT { return iterator(begin_, begin_, end_); }
    const_iterator begin() const NOEXCEPT { return const_iterator(begin_, begin_, end_); }
    const_iterator cbegin() const NOEXCEPT { return const_iterator(begin_, begin_, end_); }

    iterator end() NOEXCEPT { return iterator(end_, begin_, end_); }
    const_iterator end() const NOEXCEPT { return const_iterator(end_, begin_, end_); }
    const_iterator cend() const NOEXCEPT { return const_iterator(end_, begin_, end_); }

    reverse_iterator rbegin() NOEXCEPT { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const NOEXCEPT { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const NOEXCEPT { return const_reverse_iterator(end()); }

    reverse_iterator rend() NOEXCEPT { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const NOEXCEPT { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const NOEXCEPT { return const_reverse_iterator(begin()); }

    pointer data() NOEXCEPT { return begin_; }
    const_pointer data() const NOEXCEPT { return begin_; }

    reference operator[](size_type i) {
        assert(i < size());
        return begin_[i];
    }
    const_reference operator[](size_type i) const {
        assert(i < size());
        return begin_[i];
    }

    reference at(size_type i) {
        if (i >= size()) { throw std::out_of_range("invalid vector index"); }
        return begin_[i];
    }
    const_reference at(size_type i) const {
        if (i >= size()) { throw std::out_of_range("invalid vector index"); }
        return begin_[i];
    }

    reference front() {
        assert(begin_ != end_);
        return *begin();
    }
    const_reference front() const {
        assert(begin_ != end_);
        return *begin();
    }

    reference back() {
        assert(begin_ != end_);
        return *rbegin();
    }
    const_reference back() const {
        assert(begin_ != end_);
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
                          std::is_assignable<Ty&, decltype(*std::declval<InputIt>())>());
    }

    void clear() NOEXCEPT { helpers::truncate(*this, begin_, end_); }

    void reserve(size_type reserve_sz) {
        if (reserve_sz <= capacity()) { return; }
        temp_buf_t buf(*this, alloc_new(reserve_sz));
        helpers::append_move(*this, buf.end, begin_, end_, use_move_for_relocate());
        swap_data(buf.begin, buf.end, buf.boundary);
    }

    void shrink_to_fit() {
        if (end_ == boundary_) { return; }
        if (begin_ != end_) {
            temp_buf_t buf(*this, alloc_new(size()));
            helpers::append_move(*this, buf.end, begin_, end_, use_move_for_relocate());
            swap_data(buf.begin, buf.end, buf.boundary);
        } else {
            tidy();
        }
    }

    void resize(size_type sz) {
        if (sz > size()) {
            auto count = sz - size();
            if (count > static_cast<size_type>(boundary_ - end_)) {
                temp_buf_t buf(*this, alloc_new(grow_capacity(count)));
                destroy_guard_t g(*this, buf.begin + size(), buf.begin + size());
                helpers::append_default(*this, g.end, g.end + count);
                helpers::append_move(*this, buf.end, begin_, end_, use_move_for_relocate());
                buf.end = g.begin = g.end;
                swap_data(buf.begin, buf.end, buf.boundary);
            } else {
                helpers::append_default(*this, end_, end_ + count);
            }
        } else {
            helpers::truncate(*this, begin_ + sz, end_);
        }
    }

    void resize(size_type sz, const value_type& val) {
        if (sz > size()) {
            auto count = sz - size();
            if (count > static_cast<size_type>(boundary_ - end_)) {
                temp_buf_t buf(*this, alloc_new(grow_capacity(count)));
                destroy_guard_t g(*this, buf.begin + size(), buf.begin + size());
                helpers::append_copy(*this, g.end, g.end + count, const_value(val));
                helpers::append_move(*this, buf.end, begin_, end_, use_move_for_relocate());
                buf.end = g.begin = g.end;
                swap_data(buf.begin, buf.end, buf.boundary);
            } else {
                helpers::append_copy(*this, end_, end_ + count, const_value(val));
            }
        } else {
            helpers::truncate(*this, begin_ + sz, end_);
        }
    }

    iterator insert(const_iterator pos, size_type count, const value_type& val) {
        auto p = insert_const(to_ptr(pos), count, val, std::is_copy_assignable<Ty>());
        return iterator(p, begin_, end_);
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        auto p = insert_from_range(to_ptr(pos), first, last, is_random_access_iterator<InputIt>(),
                                   std::is_assignable<Ty&, decltype(*std::declval<InputIt>())>());
        return iterator(p, begin_, end_);
    }

    iterator insert(const_iterator pos, std::initializer_list<value_type> l) {
        auto p = insert_impl(to_ptr(pos), l.size(), l.begin(), std::is_copy_assignable<Ty>());
        return iterator(p, begin_, end_);
    }

    iterator insert(const_iterator pos, const value_type& val) { return emplace(pos, val); }
    iterator insert(const_iterator pos, value_type&& val) { return emplace(pos, std::move(val)); }
    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        auto p = emplace_impl(to_ptr(pos), std::forward<Args>(args)...);
        return iterator(p, begin_, end_);
    }

    void push_back(const value_type& val) { emplace_back(val); }
    void push_back(value_type&& val) { emplace_back(std::move(val)); }
    template<typename... Args>
    reference emplace_back(Args&&... args) {
        if (end_ == boundary_) {
            temp_buf_t buf(*this, alloc_new(grow_capacity(1)));
            destroy_guard_t g(*this, buf.begin + size(), buf.begin + size());
            helpers::append(*this, g.end, std::forward<Args>(args)...);
            helpers::append_move(*this, buf.end, begin_, end_, use_move_for_relocate());
            buf.end = g.begin = g.end;
            swap_data(buf.begin, buf.end, buf.boundary);
        } else {
            helpers::append(*this, end_, std::forward<Args>(args)...);
        }
        return *(end_ - 1);
    }

    void pop_back() {
        assert(begin_ != end_);
        helpers::truncate(*this, end_);
    }

    iterator erase(const_iterator pos) {
        auto p = to_ptr(pos);
        assert(p != end_);
        helpers::erase(*this, p, end_);
        return iterator(p, begin_, end_);
    }

    iterator erase(const_iterator first, const_iterator last) {
        auto p_first = to_ptr(first);
        auto p_last = to_ptr(last);
        assert((begin_ <= p_first) && (p_first <= p_last) && (p_last <= end_));
        auto count = static_cast<size_type>(p_last - p_first);
        if (count) { helpers::erase(*this, p_first, end_ - count, end_); }
        return iterator(p_first, begin_, end_);
    }

 private:
    pointer begin_{nullptr};
    pointer end_{nullptr};
    pointer boundary_{nullptr};

    enum : size_type { kStartCapacity = 8 };

    struct value_instance_t {
        alloc_type& alloc;
        typename std::aligned_storage<sizeof(value_type), std::alignment_of<value_type>::value>::type storage;
        template<typename... Args>
        explicit value_instance_t(alloc_type& alloc_, Args&&... args) : alloc(alloc_) {
            alloc_traits::construct(alloc, reinterpret_cast<value_type*>(std::addressof(storage)),
                                    std::forward<Args>(args)...);
        }
        value_type& val() { return *reinterpret_cast<value_type*>(std::addressof(storage)); }
        ~value_instance_t() { alloc_traits::destroy(alloc, reinterpret_cast<value_type*>(std::addressof(storage))); }
        value_instance_t(const value_instance_t&) = delete;
        value_instance_t& operator=(const value_instance_t&) = delete;
    };

    struct destroy_guard_t {
        alloc_type& alloc;
        pointer begin, end;
        destroy_guard_t(alloc_type& alloc_, pointer begin_, pointer end_) : alloc(alloc_), begin(begin_), end(end_) {}
        ~destroy_guard_t() {
            for (; begin != end; ++begin) { alloc_traits::destroy(alloc, std::addressof(*begin)); }
        }
        destroy_guard_t(const destroy_guard_t&) = delete;
        destroy_guard_t& operator=(const destroy_guard_t&) = delete;
    };

    struct temp_buf_t {
        alloc_type& alloc;
        pointer begin, end, boundary;
        temp_buf_t(alloc_type& alloc_, const std::tuple<pointer, pointer, pointer>& v)
            : alloc(alloc_), begin(std::get<0>(v)), end(std::get<1>(v)), boundary(std::get<2>(v)) {}
        temp_buf_t(vector&& v) : alloc(v), begin(v.begin_), end(v.end_), boundary(v.boundary_) {
            v.begin_ = v.end_ = v.boundary_ = nullptr;
        }
        ~temp_buf_t() {
            if (begin == boundary) { return; }
            for (auto p = begin; p != end; ++p) { alloc_traits::destroy(alloc, std::addressof(*p)); }
            alloc_traits::deallocate(alloc, begin, static_cast<size_type>(boundary - begin));
        }
        temp_buf_t(const temp_buf_t&) = delete;
        temp_buf_t& operator=(const temp_buf_t&) = delete;
    };

    template<typename Func>
    void tidy_invoke(Func fn) {
        try {
            fn();
        } catch (...) {
            tidy();
            throw;
        }
    }

    bool is_same_alloc(const alloc_type& alloc) { return static_cast<alloc_type&>(*this) == alloc; }

    pointer to_ptr(const_iterator it) const {
        auto p = std::addressof(const_cast<reference>(*it.ptr(begin_, end_)));
        assert((begin_ <= p) && (p <= end_));
        return p;
    }

    size_type grow_capacity(size_type extra) const {
        return std::max(size() + extra, std::max<size_type>(kStartCapacity, (3 * capacity()) >> 1));
    }

    std::tuple<pointer, pointer, pointer> alloc_new(size_type sz) {
        assert(sz);
        auto p = alloc_traits::allocate(*this, sz);
        return std::make_tuple(p, p, p + sz);
    }

    void swap_data(pointer& begin, pointer& end, pointer& boundary) {
        std::swap(begin_, begin);
        std::swap(end_, end);
        std::swap(boundary_, boundary);
    }

    void tidy() { temp_buf_t buf(std::move(*this)); }

    void steal_data(vector& other) {
        begin_ = other.begin_;
        end_ = other.end_;
        boundary_ = other.boundary_;
        other.begin_ = other.end_ = other.boundary_ = nullptr;
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
        tidy();
        if (alloc_traits::propagate_on_container_move_assignment::value) { alloc_type::operator=(std::move(other)); }
        steal_data(other);
    }

    void assign_impl(vector&& other, std::false_type) {
        if (is_same_alloc(other)) {
            tidy();
            steal_data(other);
        } else {
            assign_impl(other.size(), std::make_move_iterator(other.begin()), std::is_move_assignable<Ty>());
        }
    }

    void swap_impl(vector& other, std::true_type) NOEXCEPT {
        std::swap(static_cast<alloc_type&>(*this), static_cast<alloc_type&>(other));
        swap_data(other.begin_, other.end_, other.boundary_);
    }

    void swap_impl(vector& other, std::false_type) NOEXCEPT { swap_data(other.begin_, other.end_, other.boundary_); }

    void init_default(size_type sz) {
        assert(begin_ == nullptr);
        if (!sz) { return; }
        std::tie(begin_, end_, boundary_) = alloc_new(sz);
        helpers::append_default(*this, end_, end_ + sz);
    }

    template<typename RandIt>
    void init(size_type sz, RandIt src) {
        assert(begin_ == nullptr);
        if (!sz) { return; }
        std::tie(begin_, end_, boundary_) = alloc_new(sz);
        helpers::append_copy(*this, end_, end_ + sz, src);
    }

    template<typename RandIt>
    void init_from_range(RandIt first, RandIt last, std::true_type) {
        assert(first <= last);
        init(static_cast<size_type>(last - first), first);
    }

    template<typename InputIt>
    void init_from_range(InputIt first, InputIt last, std::false_type) {
        assert(begin_ == nullptr);
        for (; first != last; ++first) { emplace_back(*first); }
    }

    template<typename RandIt>
    void assign_impl(size_type sz, RandIt src, std::true_type) {
        if (sz > capacity()) {
            temp_buf_t buf(*this, alloc_new(sz));
            helpers::append_copy(*this, buf.end, buf.end + sz, src);
            swap_data(buf.begin, buf.end, buf.boundary);
        } else {
            if (sz > size()) {
                for (auto p = begin_; p != end_; ++src) { *p++ = *src; }
                helpers::append_copy(*this, end_, begin_ + sz, src);
            } else {
                auto new_end = begin_ + sz;
                for (auto p = begin_; p != new_end; ++src) { *p++ = *src; }
                helpers::truncate(*this, new_end, end_);
            }
        }
    }

    template<typename RandIt>
    void assign_impl(size_type sz, RandIt src, std::false_type) {
        if (sz > capacity()) {
            temp_buf_t buf(*this, alloc_new(sz));
            helpers::append_copy(*this, buf.end, buf.end + sz, src);
            swap_data(buf.begin, buf.end, buf.boundary);
        } else {
            helpers::truncate(*this, begin_, end_);
            helpers::append_copy(*this, end_, end_ + sz, src);
        }
    }

    template<typename RandIt, typename Bool>
    void assign_from_range(RandIt first, RandIt last, std::true_type, Bool) {
        assert(first <= last);
        assign_impl(static_cast<size_type>(last - first), first, Bool());
    }

    template<typename InputIt>
    void assign_from_range(InputIt first, InputIt last, std::false_type, std::true_type) {
        auto new_end = begin_;
        for (; (new_end != end_) && (first != last); ++first) { *new_end++ = *first; }
        if (new_end != end_) {
            helpers::truncate(*this, new_end, end_);
        } else {
            for (; first != last; ++first) { emplace_back(*first); }
        }
    }

    template<typename InputIt>
    void assign_from_range(InputIt first, InputIt last, std::false_type, std::false_type) {
        helpers::truncate(*this, begin_, end_);
        for (; first != last; ++first) { emplace_back(*first); }
    }

    template<typename... Args>
    pointer emplace_impl(pointer p, Args&&... args) {
        if (end_ == boundary_) {
            auto n = static_cast<size_type>(p - begin_);
            temp_buf_t buf(*this, alloc_new(grow_capacity(1)));
            destroy_guard_t g(*this, buf.begin + n, buf.begin + n);
            helpers::append(*this, g.end, std::forward<Args>(args)...);
            helpers::append_move(*this, g.end, p, end_, use_move_for_relocate());
            helpers::append_move(*this, buf.end, begin_, p, use_move_for_relocate());
            buf.end = g.begin = g.end;
            swap_data(buf.begin, buf.end, buf.boundary);
            return begin_ + n;
        } else if (p != end_) {
            helpers::emplace(*this, p, end_, std::forward<Args>(args)...);
        } else {
            helpers::append(*this, end_, std::forward<Args>(args)...);
        }
        return p;
    }

    template<typename RandIt, typename Bool>
    pointer insert_impl(pointer p, size_type count, RandIt src, Bool) {
        if (count > static_cast<size_type>(boundary_ - end_)) {
            return insert_realloc(p, count, src);
        } else if (count) {
            insert_no_realloc(p, count, src, Bool());
        }
        return p;
    }

    template<typename Bool>
    pointer insert_const(pointer p, size_type count, const value_type& val, Bool) {
        if (count > static_cast<size_type>(boundary_ - end_)) {
            return insert_realloc(p, count, const_value(val));
        } else if (count) {
            value_instance_t val_copy(*this, val);
            insert_no_realloc(p, count, const_value(val_copy.val()), Bool());
        }
        return p;
    }

    template<typename RandIt>
    pointer insert_realloc(pointer p, size_type count, RandIt src) {
        auto n = static_cast<size_type>(p - begin_);
        temp_buf_t buf(*this, alloc_new(grow_capacity(count)));
        destroy_guard_t g(*this, buf.begin + n, buf.begin + n);
        helpers::append_copy(*this, g.end, g.end + count, src);
        helpers::append_move(*this, g.end, p, end_, use_move_for_relocate());
        helpers::append_move(*this, buf.end, begin_, p, use_move_for_relocate());
        buf.end = g.begin = g.end;
        swap_data(buf.begin, buf.end, buf.boundary);
        return begin_ + n;
    }

    template<typename RandIt>
    void insert_no_realloc(pointer p, size_type count, RandIt src, std::true_type) {
        if (p != end_) {
            helpers::insert_copy(*this, p, end_, end_ + count, src);
        } else {
            helpers::append_copy(*this, end_, end_ + count, src);
        }
    }

    template<typename RandIt>
    void insert_no_realloc(pointer p, size_type count, RandIt src, std::false_type) {
        helpers::append_copy(*this, end_, end_ + count, src);
        std::rotate(p, end_ - count, end_);
    }

    template<typename RandIt, typename Bool>
    pointer insert_from_range(pointer p, RandIt first, RandIt last, std::true_type, Bool) {
        assert(first <= last);
        return insert_impl(p, static_cast<size_type>(last - first), first, Bool());
    }

    template<typename InputIt, typename Bool>
    pointer insert_from_range(pointer p, InputIt first, InputIt last, std::false_type, Bool) {
        size_type count = 0;
        auto n = static_cast<size_type>(p - begin_);
        for (; first != last; ++first) {
            emplace_back(*first);
            ++count;
        }
        std::rotate(begin_ + n, end_ - count, end_);
        return begin_ + n;
    }

    struct helpers {
        template<typename... Args>
        static void append(alloc_type& alloc, pointer& end, Args&&... args) {
            alloc_traits::construct(alloc, std::addressof(*end), std::forward<Args>(args)...);
            ++end;
        }

        static void append_default(alloc_type& alloc, pointer& end, pointer new_end) {
            for (; end != new_end; ++end) { alloc_traits::construct(alloc, std::addressof(*end)); }
        }

        template<typename InputIt>
        static void append_copy(alloc_type& alloc, pointer& end, pointer new_end, InputIt src) {
            for (; end != new_end; ++end) {
                alloc_traits::construct(alloc, std::addressof(*end), *src);
                ++src;
            }
        }

        static void append_move(alloc_type& alloc, pointer& end, pointer src_first, pointer src_last, std::true_type) {
            for (; src_first != src_last; ++end, ++src_first) {
                alloc_traits::construct(alloc, std::addressof(*end), std::move(*src_first));
            }
        }

        static void append_move(alloc_type& alloc, pointer& end, pointer src_first, pointer src_last, std::false_type) {
            for (; src_first != src_last; ++end, ++src_first) {
                alloc_traits::construct(alloc, std::addressof(*end), *static_cast<const_pointer>(src_first));
            }
        }

        static void truncate(alloc_type& alloc, pointer& end) {
            alloc_traits::destroy(alloc, std::addressof(*(--end)));
        }

        static void truncate(alloc_type& alloc, pointer new_end, pointer& end) {
            for (auto p = new_end; p != end; ++p) { alloc_traits::destroy(alloc, std::addressof(*p)); }
            end = new_end;
        }

        static void emplace(alloc_type& alloc, pointer pos, pointer& end, value_type&& val) {
            assert(pos != end);
            append(alloc, end, std::move(*(end - 1)));
            for (auto p = end - 2; pos != p; --p) { *p = std::move(*(p - 1)); }
            *pos = std::move(val);
        }

        template<typename... Args>
        static void emplace(alloc_type& alloc, pointer pos, pointer& end, Args&&... args) {
            value_instance_t val_inst(alloc, std::forward<Args>(args)...);
            emplace(alloc, pos, end, std::move(val_inst.val()));
        }

        template<typename RandIt>
        static void insert_copy(alloc_type& alloc, pointer pos, pointer& end, pointer new_end, RandIt src) {
            assert((pos != end) && (end != new_end));
            auto count = static_cast<size_t>(new_end - end);
            auto tail = static_cast<size_t>(end - pos);
            if (count > tail) {
                auto p = end;
                append_copy(alloc, end, end + count - tail,
                            src + static_cast<typename std::iterator_traits<RandIt>::difference_type>(tail));
                append_move(alloc, end, p - tail, p, std::true_type());
                for (p = pos; p != pos + tail; ++src) { *p++ = *src; };
            } else {
                auto p = end - count;
                append_move(alloc, end, end - count, end, std::true_type());
                for (; pos != p; --p) { *(p + count - 1) = std::move(*(p - 1)); }
                for (; p != pos + count; ++src) { *p++ = *src; };
            }
        }

        static void erase(alloc_type& alloc, pointer p, pointer& end) {
            for (; p != end - 1; ++p) { *p = std::move(*(p + 1)); }
            truncate(alloc, end);
        }

        static void erase(alloc_type& alloc, pointer p, pointer new_end, pointer& end) {
            assert(new_end != end);
            auto count = static_cast<size_t>(end - new_end);
            for (; p != new_end; ++p) { *p = std::move(*(p + count)); }
            truncate(alloc, new_end, end);
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
