#pragma once

#include "except.h"
#include "iterator.h"

#include <algorithm>
#include <cstring>
#include <initializer_list>

namespace uxs {

namespace detail {

template<typename Ty, typename Alloc>
class dynarray_base : protected std::allocator_traits<Alloc>::template rebind_alloc<Ty> {
    static_assert(std::is_same<std::remove_cv_t<Ty>, Ty>::value,
                  "uxs::dynarray must have a non-const, non-volatile value type");

 protected:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<Ty>;
    using alloc_traits = std::allocator_traits<alloc_type>;

 private:
    struct iterator_traits {
        using value_type = Ty;
        using difference_type = typename alloc_traits::difference_type;
        using pointer = typename alloc_traits::pointer;
        using const_pointer = typename alloc_traits::const_pointer;
        using reference = value_type&;
        using const_reference = const value_type&;
        using underlying_ptr_t = pointer;
    };

 public:
    using value_type = Ty;
    using allocator_type = Alloc;
    using size_type = typename alloc_traits::size_type;
    using difference_type = typename alloc_traits::difference_type;
    using pointer = typename alloc_traits::pointer;
    using const_pointer = typename alloc_traits::const_pointer;
    using reference = value_type&;
    using const_reference = const value_type&;
    using iterator = est::array_iterator<iterator_traits, false>;
    using const_iterator = est::array_iterator<iterator_traits, true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    void assign(size_type sz, const value_type& val);

    void assign(std::initializer_list<value_type> init_list) {
        assign(init_list.begin(), init_list.begin() + init_list.size());
    }

    template<typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>>
    void assign(InputIt first, InputIt last) {
        assign_dispatch(first, last, est::is_random_access_iterator<InputIt>());
    }

    ~dynarray_base() { tidy(); }

    bool empty() const noexcept { return data_ == endp_; }
    size_type size() const noexcept { return endp_ - data_; }
    size_type capacity() const noexcept { return capacity_; }
    size_type max_size() const noexcept { return ((alloc_traits::max_size(*this) - 1) & ~size_type(1)) + 1; }
    allocator_type get_allocator() const noexcept { return allocator_type(*this); }

    iterator begin() noexcept { return iterator(data_, data_, endp_); }
    const_iterator begin() const noexcept { return const_iterator(data_, data_, endp_); }
    const_iterator cbegin() const noexcept { return begin(); }

    iterator end() noexcept { return iterator(endp_, data_, endp_); }
    const_iterator end() const noexcept { return const_iterator(endp_, data_, endp_); }
    const_iterator cend() const noexcept { return end(); }

    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }

    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return rend(); }

    pointer data() noexcept { return data_; }
    const_pointer data() const noexcept { return data_; }

    reference operator[](size_type i) {
        assert(i < size());
        return data_[i];
    }
    const_reference operator[](size_type i) const {
        assert(i < size());
        return data_[i];
    }

    reference at(size_type i) {
        if (i >= size()) { report_index_out_of_range_error(); }
        return data_[i];
    }
    const_reference at(size_type i) const {
        if (i >= size()) { report_index_out_of_range_error(); }
        return data_[i];
    }

    reference front() {
        assert(!empty());
        return *data_;
    }
    const_reference front() const {
        assert(!empty());
        return *data_;
    }

    reference back() {
        assert(!empty());
        return *(endp_ - 1);
    }
    const_reference back() const {
        assert(!empty());
        return *(endp_ - 1);
    }

    void clear() noexcept;

    void reserve(size_type count);
    void resize(size_type count) { resize_impl(count); }
    void resize(size_type count, const value_type& val) { resize_impl(count, val); }

    template<typename... Args>
    reference emplace_back(Args&&... args);
    void push_back(const value_type& val) { emplace_back(val); }
    void push_back(value_type&& val) { emplace_back(std::move(val)); }
    void pop_back() noexcept;

    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args);
    iterator insert(const_iterator pos, const value_type& val) { return emplace(pos, val); }
    iterator insert(const_iterator pos, value_type&& val) { return emplace(pos, std::move(val)); }

    iterator insert(const_iterator pos, size_type count, const value_type& val);
    iterator insert(const_iterator pos, std::initializer_list<value_type> init_list) {
        return insert(pos, init_list.begin(), init_list.begin() + init_list.size());
    }
    template<typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>>
    iterator insert(const_iterator pos, InputIt first, InputIt last);

    iterator erase(const_iterator pos);
    iterator erase(const_iterator first, const_iterator last);

 protected:
    dynarray_base(pointer inplace_data,
                  size_type inplace_capacity) noexcept(std::is_nothrow_default_constructible<Alloc>::value)
        : alloc_type(), data_(inplace_data), endp_(inplace_data), capacity_(inplace_capacity) {}
    dynarray_base(const alloc_type& al, pointer inplace_data, size_type inplace_capacity) noexcept
        : alloc_type(al), data_(inplace_data), endp_(inplace_data), capacity_(inplace_capacity) {}
    dynarray_base(alloc_type&& al, pointer inplace_data, size_type inplace_capacity) noexcept
        : alloc_type(std::move(al)), data_(inplace_data), endp_(inplace_data), capacity_(inplace_capacity) {}

    bool is_same_alloc(const alloc_type& al) { return static_cast<alloc_type&>(*this) == al; }

    size_type eval_min_capacity(size_type inplace_capacity) const {
        return size() > inplace_capacity ? (size() & ~size_type(1)) + 1 : inplace_capacity;
    }

    void reset(pointer inplace_data, size_type inplace_capacity) noexcept {
        tidy();
        endp_ = data_ = inplace_data;
        capacity_ = inplace_capacity;
    }

    void move_allocated_data(dynarray_base& other, pointer other_inplace_data,
                             size_type other_inplace_capacity) noexcept {
        data_ = other.data_, endp_ = other.endp_;
        capacity_ = other.capacity_;
        other.endp_ = other.data_ = other_inplace_data;
        other.capacity_ = other_inplace_capacity;
    }

    void swap_allocated_data(dynarray_base& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(endp_, other.endp_);
        std::swap(capacity_, other.capacity_);
    }

    template<typename... Args>
    void init(size_type count, Args&&... args);
    template<typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>>
    void init(InputIt first, InputIt last) {
        init_dispatch(first, last, est::is_random_access_iterator<InputIt>());
    }

    void relocate(size_type capacity) { relocate_append(capacity, est::identity{}); }

    template<typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>>
    void append_no_realloc(size_type count, InputIt first) {
        endp_ = construct_copy(data_, count, first);
    }

    void relocate_to_buffer(pointer dst, size_type capacity) noexcept(std::is_nothrow_move_constructible<Ty>::value) {
        pointer endp = relocate_data(dst);
        tidy();
        data_ = dst, endp_ = endp;
        capacity_ = capacity;
    }

 private:
    pointer data_;
    pointer endp_;
    size_type capacity_;

    void tidy() noexcept;

    void destruct_items(pointer first, pointer last) noexcept {
        destruct_items_dispatch(first, last,
                                std::bool_constant<std::is_trivially_destructible<Ty>::value &&
                                                   std::is_same<alloc_type, std::allocator<Ty>>::value>());
    }

    template<typename PtrTy>
    void destruct_items_dispatch(PtrTy /*first*/, PtrTy /*last*/,
                                 std::true_type /* trivially destructible */) noexcept {}

    template<typename PtrTy>
    void destruct_items_dispatch(PtrTy first, PtrTy last, std::false_type /* trivially destructible */) noexcept {
        for (; first != last; ++first) { alloc_traits::destroy(*this, std::addressof(*first)); };
    }

    void init_reserve(size_type count);

    size_type eval_new_capacity(size_type extra) const;

    template<typename AppendFn, typename... Args>
    void relocate_append(size_type capacity, AppendFn&& append_fn, Args&&... args);

    template<typename AssignFn, typename... Args>
    void relocate_assign(size_type capacity, AssignFn&& assign_fn, Args&&... args);

    template<typename... Args>
    void resize_impl(size_type count, Args&&... args);

    template<typename InputIt>
    void init_dispatch(InputIt first, InputIt last, std::true_type /* random access iterator */);

    template<typename InputIt>
    void init_dispatch(InputIt first, InputIt last, std::false_type /* random access iterator */);

    void append(size_type count, const value_type& val);

    template<typename InputIt>
    void append_dispatch(InputIt first, InputIt last, std::true_type /* random access iterator */);

    template<typename InputIt>
    void append_dispatch(InputIt first, InputIt last, std::false_type /* random access iterator */);

    template<typename InputIt>
    void assign_dispatch(InputIt first, InputIt last, std::true_type /* random access iterator */);

    template<typename InputIt>
    void assign_dispatch(InputIt first, InputIt last, std::false_type /* random access iterator */);

    template<typename... Args>
    pointer construct_fill(pointer dst, size_type count, Args&&... args) {
        return construct_fill_dispatch(dst, count, std::is_same<alloc_type, std::allocator<Ty>>(),
                                       std::forward<Args>(args)...);
    }

    template<typename PtrTy>
    PtrTy construct_fill_dispatch(PtrTy dst, size_type count, std::true_type /* default allocator */) {
        return construct_fill_default_dispatch(dst, count, std::is_trivially_default_constructible<Ty>());
    }

    template<typename PtrTy>
    PtrTy construct_fill_default_dispatch(PtrTy dst, size_type count,
                                          std::true_type /* trivially default constructible */) {
        if (count) { std::memset(dst, 0, count * sizeof(Ty)); }
        return dst + count;
    }

    template<typename PtrTy>
    PtrTy construct_fill_default_dispatch(PtrTy dst, size_type count,
                                          std::false_type /* trivially default constructible */) {
        return construct_fill_dispatch(dst, count, std::false_type());
    }

    template<typename PtrTy>
    PtrTy construct_fill_dispatch(PtrTy dst, size_type count, std::true_type /* default allocator */,
                                  const value_type& val) {
        return std::uninitialized_fill_n(dst, count, val);
    }

    template<typename... Args>
    pointer construct_fill_dispatch(pointer dst, size_type count, std::false_type /* default allocator */,
                                    Args&&... args);

    template<typename InputIt>
    pointer construct_copy(pointer dst, size_type count, InputIt first) {
        return construct_copy_dispatch(dst, count, first, std::is_same<alloc_type, std::allocator<Ty>>());
    }

    template<typename InputIt>
    pointer construct_copy_dispatch(pointer dst, size_type count, InputIt first,
                                    std::true_type /* default allocator */) {
        return std::uninitialized_copy_n(first, count, dst);
    }

    template<typename InputIt>
    pointer construct_copy_dispatch(pointer dst, size_type count, InputIt first,
                                    std::false_type /* default allocator */);

    pointer relocate_data(pointer dst) noexcept(std::is_nothrow_move_constructible<Ty>::value) {
        return relocate_data_dispatch(
            dst, std::is_same<alloc_type, std::allocator<Ty>>(),
            std::bool_constant<std::is_nothrow_move_constructible<Ty>::value || !std::is_copy_constructible<Ty>::value>(),
            std::is_nothrow_move_constructible<Ty>());
    }

    template<typename PtrTy, typename Bool>
    PtrTy relocate_data_dispatch(
        PtrTy dst, std::true_type /* default allocator */, std::true_type /* use move relocation */,
        Bool /* nothrow move constructible */) noexcept(std::is_nothrow_move_constructible<Ty>::value) {
        return std::uninitialized_copy(std::make_move_iterator(data_), std::make_move_iterator(endp_), dst);
    }

    template<typename PtrTy>
    PtrTy relocate_data_dispatch(PtrTy dst, std::true_type /* default allocator */,
                                 std::false_type /* use move relocation */,
                                 std::false_type /* nothrow move constructible */) {
        return std::uninitialized_copy(const_pointer(data_), const_pointer(endp_), dst);
    }

    template<typename PtrTy>
    PtrTy relocate_data_dispatch(PtrTy dst, std::false_type /* default allocator */,
                                 std::true_type /* use move relocation */,
                                 std::true_type /* nothrow move constructible */) noexcept;

    template<typename PtrTy>
    PtrTy relocate_data_dispatch(PtrTy dst, std::false_type /* default allocator */,
                                 std::true_type /* use move relocation */,
                                 std::false_type /* nothrow move constructible */);

    template<typename PtrTy>
    PtrTy relocate_data_dispatch(PtrTy dst, std::false_type /* default allocator */,
                                 std::false_type /* use move relocation */,
                                 std::false_type /* nothrow move constructible */);
};

template<typename Ty, typename Alloc>
void dynarray_base<Ty, Alloc>::clear() noexcept {
    destruct_items(data_, endp_);
    endp_ = data_;
}

template<typename Ty, typename Alloc>
void dynarray_base<Ty, Alloc>::reserve(size_type count) {
    if (count > capacity_) { relocate(eval_new_capacity(count - size())); }
}

template<typename Ty, typename Alloc>
template<typename... Args>
auto dynarray_base<Ty, Alloc>::emplace_back(Args&&... args) -> reference {
    if (size() != capacity_) {
        alloc_traits::construct(*this, std::addressof(*endp_), std::forward<Args>(args)...);
        ++endp_;
    } else {
        relocate_append(
            eval_new_capacity(1),
            [this](pointer endp, Args&&... args) {
                alloc_traits::construct(*this, std::addressof(*endp), std::forward<Args>(args)...);
                return endp + 1;
            },
            std::forward<Args>(args)...);
    }
    return *(endp_ - 1);
}

template<typename Ty, typename Alloc>
template<typename... Args>
auto dynarray_base<Ty, Alloc>::emplace(const_iterator pos, Args&&... args) -> iterator {
    uxs_iterator_assert(pos.debug_begin() == data_ && pos.debug_end() == endp_);
    assert(data_ <= pos.ptr() && pos.ptr() <= endp_);
    const size_type n_pos = pos.ptr() - data_;
    emplace_back(std::forward<Args>(args)...);
    if (n_pos != size() - 1) {
        pointer last = endp_;
        value_type t(std::move(*(last - 1)));
        std::move_backward(data_ + n_pos, last - 1, last);
        data_[n_pos] = std::move(t);
    }
    return iterator(data_ + n_pos, data_, endp_);
}

template<typename Ty, typename Alloc>
auto dynarray_base<Ty, Alloc>::insert(const_iterator pos, size_type count, const value_type& val) -> iterator {
    uxs_iterator_assert(pos.debug_begin() == data_ && pos.debug_end() == endp_);
    assert(data_ <= pos.ptr() && pos.ptr() <= endp_);
    const size_type prev_sz = size();
    const size_type n_pos = pos.ptr() - data_;
    append(count, val);
    if (n_pos != size() - 1) { std::rotate(data_ + n_pos, data_ + prev_sz, data_ + size()); }
    return iterator(data_ + n_pos, data_, endp_);
}

template<typename Ty, typename Alloc>
template<typename InputIt, typename>
auto dynarray_base<Ty, Alloc>::insert(const_iterator pos, InputIt first, InputIt last) -> iterator {
    uxs_iterator_assert(pos.debug_begin() == data_ && pos.debug_end() == endp_);
    assert(data_ <= pos.ptr() && pos.ptr() <= endp_);
    const size_type prev_sz = size();
    const size_type n_pos = pos.ptr() - data_;
    append_dispatch(first, last, est::is_random_access_iterator<InputIt>());
    if (n_pos != size() - 1) { std::rotate(data_ + n_pos, data_ + prev_sz, data_ + size()); }
    return iterator(data_ + n_pos, data_, endp_);
}

template<typename Ty, typename Alloc>
void dynarray_base<Ty, Alloc>::pop_back() noexcept {
    assert(!empty());
    --endp_;
    alloc_traits::destroy(*this, std::addressof(*endp_));
}

template<typename Ty, typename Alloc>
auto dynarray_base<Ty, Alloc>::erase(const_iterator pos) -> iterator {
    uxs_iterator_assert(pos.debug_begin() == data_ && pos.debug_end() == endp_);
    assert(data_ <= pos.ptr() && pos.ptr() < endp_);
    std::move(pos.ptr() + 1, endp_, pos.ptr());
    --endp_;
    alloc_traits::destroy(*this, std::addressof(*endp_));
    return iterator(pos.ptr(), data_, endp_);
}

template<typename Ty, typename Alloc>
auto dynarray_base<Ty, Alloc>::erase(const_iterator first, const_iterator last) -> iterator {
    uxs_iterator_assert(first.debug_begin() == data_ && first.debug_end() == endp_);
    uxs_iterator_assert(last.debug_begin() == data_ && last.debug_end() == endp_);
    assert(data_ <= first.ptr() && first.ptr() <= last.ptr() && last.ptr() <= endp_);
    if (first.ptr() != last.ptr()) {
        std::move(last.ptr(), endp_, first.ptr());
        const auto count = last.ptr() - first.ptr();
        endp_ -= count;
        destruct_items(endp_, endp_ + count);
    }
    return iterator(first.ptr(), data_, endp_);
}

template<typename Ty, typename Alloc>
void dynarray_base<Ty, Alloc>::tidy() noexcept {
    destruct_items(data_, endp_);
    if (capacity_ & 1) { alloc_traits::deallocate(*this, data_, capacity_); }
}

template<typename Ty, typename Alloc>
void dynarray_base<Ty, Alloc>::init_reserve(size_type count) {
    if (count > capacity_) {
        if (count > max_size()) { report_too_much_to_allocate_error(); }
        capacity_ = (count & ~size_type(1)) + 1;  // Make new dynamic odd capacity
        endp_ = data_ = alloc_traits::allocate(*this, capacity_);
    }
}

template<typename Ty, typename Alloc>
auto dynarray_base<Ty, Alloc>::eval_new_capacity(size_type extra) const -> size_type {
    const size_type size = this->size();
    size_type delta_sz = std::max(extra, size >> 1);
    const size_type max_avail = max_size() - size;
    if (delta_sz > max_avail) {
        if (extra > max_avail) { report_too_much_to_allocate_error(); }
        delta_sz = std::max(extra, max_avail >> 1);
    }
    return ((size + delta_sz) & ~size_type(1)) + 1;  // Make new dynamic odd capacity
}

template<typename Ty, typename Alloc>
template<typename AppendFn, typename... Args>
void dynarray_base<Ty, Alloc>::relocate_append(size_type capacity, AppendFn&& append_fn, Args&&... args) {
    pointer new_data = alloc_traits::allocate(*this, capacity);
    try {
        pointer endp = append_fn(new_data + size(), std::forward<Args>(args)...);
        relocate_data(new_data);
        tidy();
        data_ = new_data, endp_ = endp;
        capacity_ = capacity;
    } catch (...) {
        alloc_traits::deallocate(*this, new_data, capacity);
        throw;
    }
}

template<typename Ty, typename Alloc>
template<typename AssignFn, typename... Args>
void dynarray_base<Ty, Alloc>::relocate_assign(size_type capacity, AssignFn&& assign_fn, Args&&... args) {
    pointer new_data = alloc_traits::allocate(*this, capacity);
    try {
        pointer endp = assign_fn(new_data, std::forward<Args>(args)...);
        tidy();
        data_ = new_data, endp_ = endp;
        capacity_ = capacity;
    } catch (...) {
        alloc_traits::deallocate(*this, new_data, capacity);
        throw;
    }
}

template<typename Ty, typename Alloc>
template<typename... Args>
void dynarray_base<Ty, Alloc>::resize_impl(size_type count, Args&&... args) {
    if (count <= size()) {
        destruct_items(data_ + count, endp_);
        endp_ = data_ + count;
    } else {
        const size_type extra = count - size();
        if (count <= capacity_) {
            endp_ = construct_fill(endp_, extra, std::forward<Args>(args)...);
        } else {
            relocate_append(
                eval_new_capacity(extra),
                [this](pointer endp, size_type count, Args&&... args) {
                    return construct_fill(endp, count, std::forward<Args>(args)...);
                },
                extra, std::forward<Args>(args)...);
        }
    }
}

template<typename Ty, typename Alloc>
template<typename... Args>
void dynarray_base<Ty, Alloc>::init(size_type count, Args&&... args) {
    init_reserve(count);
    endp_ = construct_fill(data_, count, std::forward<Args>(args)...);
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void dynarray_base<Ty, Alloc>::init_dispatch(InputIt first, InputIt last, std::true_type /* random access iterator */) {
    const size_type count = static_cast<size_type>(last - first);
    init_reserve(count);
    endp_ = construct_copy(data_, count, first);
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void dynarray_base<Ty, Alloc>::init_dispatch(InputIt first, InputIt last,
                                             std::false_type /* random access iterator */) {
    for (; first != last; ++first) { emplace_back(*first); }
}

template<typename Ty, typename Alloc>
void dynarray_base<Ty, Alloc>::append(size_type count, const value_type& val) {
    if (size() + count <= capacity_) {
        endp_ = construct_fill(endp_, count, val);
    } else {
        relocate_append(
            eval_new_capacity(count),
            [this](pointer endp, size_type count, const value_type& val) { return construct_fill(endp, count, val); },
            count, val);
    }
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void dynarray_base<Ty, Alloc>::append_dispatch(InputIt first, InputIt last,
                                               std::true_type /* random access iterator */) {
    const size_type count = static_cast<size_type>(last - first);
    if (size() + count <= capacity_) {
        endp_ = construct_copy(endp_, count, first);
    } else {
        relocate_append(
            eval_new_capacity(count),
            [this](pointer endp, size_type count, InputIt first) { return construct_copy(endp, count, first); }, count,
            first);
    }
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void dynarray_base<Ty, Alloc>::append_dispatch(InputIt first, InputIt last,
                                               std::false_type /* random access iterator */) {
    for (; first != last; ++first) { emplace_back(*first); }
}

template<typename Ty, typename Alloc>
void dynarray_base<Ty, Alloc>::assign(size_type count, const value_type& val) {
    if (count <= capacity_) {
        pointer dst = std::fill_n(data_, std::min(count, size()), val);
        if (dst != endp_) {
            destruct_items(dst, endp_);
        } else {
            dst = construct_fill(dst, count - size(), val);
        }
        endp_ = dst;
    } else {
        relocate_assign(
            eval_new_capacity(count - size()),
            [this](pointer endp, size_type count, const value_type& val) { return construct_fill(endp, count, val); },
            count, val);
    }
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void dynarray_base<Ty, Alloc>::assign_dispatch(InputIt first, InputIt last,
                                               std::true_type /* random access iterator */) {
    const size_type count = static_cast<size_type>(last - first);
    if (count <= capacity_) {
        pointer dst = std::copy_n(first, std::min(count, size()), data_);
        if (dst == endp_) {
            endp_ = construct_copy(dst, count - size(), first + size());
        } else {
            destruct_items(dst, endp_);
            endp_ = dst;
        }
    } else {
        relocate_assign(
            eval_new_capacity(count - size()),
            [this](pointer endp, size_type count, InputIt first) { return construct_copy(endp, count, first); }, count,
            first);
    }
}

template<typename Ty, typename Alloc>
template<typename InputIt>
void dynarray_base<Ty, Alloc>::assign_dispatch(InputIt first, InputIt last,
                                               std::false_type /* random access iterator */) {
    pointer dst = data_;
    for (; first != last && dst != endp_; (void)++first, ++dst) { *dst = *first; }
    if (dst == endp_) {
        append_dispatch(first, last, est::is_random_access_iterator<InputIt>());
    } else {
        destruct_items(dst, endp_);
        endp_ = dst;
    }
}

template<typename Ty, typename Alloc>
template<typename... Args>
auto dynarray_base<Ty, Alloc>::construct_fill_dispatch(pointer dst, size_type count,
                                                       std::false_type /* default allocator */, Args&&... args)
    -> pointer {
    pointer dst0 = dst;
    try {
        for (pointer dst_last = dst0 + count; dst != dst_last; ++dst) {
            alloc_traits::construct(*this, std::addressof(*dst), std::forward<Args>(args)...);
        }
        return dst;
    } catch (...) {
        destruct_items(dst0, dst);
        throw;
    }
}

template<typename Ty, typename Alloc>
template<typename InputIt>
auto dynarray_base<Ty, Alloc>::construct_copy_dispatch(pointer dst, size_type count, InputIt first,
                                                       std::false_type /* default allocator */) -> pointer {
    pointer dst0 = dst;
    try {
        for (pointer dst_last = dst0 + count; dst != dst_last; (void)++first, ++dst) {
            alloc_traits::construct(*this, std::addressof(*dst), *first);
        }
        return dst;
    } catch (...) {
        destruct_items(dst0, dst);
        throw;
    }
}

template<typename Ty, typename Alloc>
template<typename PtrTy>
PtrTy dynarray_base<Ty, Alloc>::relocate_data_dispatch(PtrTy dst, std::false_type /* default allocator */,
                                                       std::true_type /* use move relocation */,
                                                       std::true_type /* nothrow move constructible */) noexcept {
    for (pointer src = data_; src != endp_; ++dst, ++src) {
        alloc_traits::construct(*this, std::addressof(*dst), std::move(*src));
    };
    return dst;
}

template<typename Ty, typename Alloc>
template<typename PtrTy>
PtrTy dynarray_base<Ty, Alloc>::relocate_data_dispatch(PtrTy dst, std::false_type /* default allocator */,
                                                       std::true_type /* use move relocation */,
                                                       std::false_type /* nothrow move constructible */) {
    pointer dst0 = dst;
    try {
        for (pointer src = data_; src != endp_; ++dst, ++src) {
            alloc_traits::construct(*this, std::addressof(*dst), std::move(*src));
        };
        return dst;
    } catch (...) {
        destruct_items(dst0, dst);
        throw;
    }
}

template<typename Ty, typename Alloc>
template<typename PtrTy>
PtrTy dynarray_base<Ty, Alloc>::relocate_data_dispatch(PtrTy dst, std::false_type /* default allocator */,
                                                       std::false_type /* use move relocation */,
                                                       std::false_type /* nothrow move constructible */) {
    pointer dst0 = dst;
    try {
        for (const_pointer src = data_; src != endp_; ++dst, ++src) {
            alloc_traits::construct(*this, std::addressof(*dst), *src);
        }
        return dst;
    } catch (...) {
        destruct_items(dst0, dst);
        throw;
    }
}

}  // namespace detail

template<typename Ty, std::size_t InplaceBufSize = est::dynamic_extent, typename Alloc = std::allocator<Ty>>
class dynarray : public detail::dynarray_base<Ty, Alloc> {
 private:
    using alloc_type = typename detail::dynarray_base<Ty, Alloc>::alloc_type;
    using alloc_traits = typename detail::dynarray_base<Ty, Alloc>::alloc_traits;

 public:
    using size_type = typename detail::dynarray_base<Ty, Alloc>::size_type;

    dynarray() noexcept(std::is_nothrow_default_constructible<Alloc>::value)
        : detail::dynarray_base<Ty, Alloc>(reinterpret_cast<Ty*>(buf_), inplace_buf_size) {}
    explicit dynarray(const Alloc& al) noexcept
        : detail::dynarray_base<Ty, Alloc>(al, reinterpret_cast<Ty*>(buf_), inplace_buf_size) {}
    explicit dynarray(size_type count, const Alloc& al = Alloc()) : dynarray(al) { this->init(count); }
    dynarray(size_type count, const Ty& val, const Alloc& al = Alloc()) : dynarray(al) { this->init(count, val); }
    dynarray(std::initializer_list<Ty> init_list, const Alloc& al = Alloc())
        : dynarray(init_list.begin(), init_list.begin() + init_list.size(), al) {}
    template<typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>>
    dynarray(InputIt first, InputIt last, const Alloc& al = Alloc()) : dynarray(al) {
        this->init(first, last);
    }

    dynarray(const dynarray& other)
        : dynarray(other.begin(), other.end(), alloc_traits::select_on_container_copy_construction(other)) {}
    dynarray(const dynarray& other, const Alloc& al) : dynarray(other.begin(), other.end(), al) {}

    dynarray(dynarray&& other) noexcept(std::is_nothrow_move_constructible<Ty>::value)
        : detail::dynarray_base<Ty, Alloc>(other, reinterpret_cast<Ty*>(buf_), inplace_buf_size) {
        move_init_dispatch(std::move(other), std::true_type());
    }
    dynarray(dynarray&& other, const Alloc& al) noexcept(alloc_traits::is_always_equal::value &&
                                                         std::is_nothrow_move_constructible<Ty>::value)
        : dynarray(al) {
        move_init_dispatch(std::move(other), typename alloc_traits::is_always_equal());
    }

    dynarray& operator=(std::initializer_list<Ty> init_list) {
        this->assign(init_list.begin(), init_list.begin() + init_list.size());
        return *this;
    }

    dynarray& operator=(const dynarray& other) {
        if (&other == this) { return *this; }
        copy_assign_dispatch(other, std::bool_constant<(alloc_traits::propagate_on_container_copy_assignment::value &&
                                                        !alloc_traits::is_always_equal::value)>());
        return *this;
    }

    dynarray& operator=(dynarray&& other) noexcept((alloc_traits::propagate_on_container_move_assignment::value ||
                                                    alloc_traits::is_always_equal::value) &&
                                                   std::is_nothrow_move_constructible<Ty>::value) {
        if (&other == this) { return *this; }
        move_assign_dispatch(std::move(other),
                             std::bool_constant<(alloc_traits::propagate_on_container_move_assignment::value ||
                                                 alloc_traits::is_always_equal::value)>());
        return *this;
    }

    void swap(dynarray& other) noexcept(std::is_nothrow_move_constructible<Ty>::value) {
        if (&other == this) { return; }
        dynarray t(std::move(*this));
        move_assign_dispatch(std::move(other), std::true_type());
        other.move_assign_dispatch(std::move(t), std::true_type());
    }

    void shrink_to_fit() {
        const size_type min_capacity = this->eval_min_capacity(inplace_buf_size);
        if (min_capacity == this->capacity()) { return; }
        if (min_capacity == inplace_buf_size) {
            this->relocate_to_buffer(reinterpret_cast<Ty*>(buf_), inplace_buf_size);
        } else {
            this->relocate(min_capacity);
        }
    }

 private:
    enum : unsigned {  // Must be even
#if UXS_DEBUG_REDUCED_BUFFERS != 0
        inplace_buf_size = 2,
#else   // UXS_DEBUG_REDUCED_BUFFERS != 0
        inplace_buf_size = InplaceBufSize != est::dynamic_extent ? ((InplaceBufSize + 1) & ~size_type(1)) : 16,
#endif  // UXS_DEBUG_REDUCED_BUFFERS != 0
    };

    static_assert(!(inplace_buf_size & 1), "inplace_buf_size must be even");

    alignas(std::alignment_of<Ty>::value) std::uint8_t buf_[inplace_buf_size * sizeof(Ty)];

    void move_init_dispatch(dynarray&& other, std::true_type /* always equal allocators */) noexcept(
        std::is_nothrow_move_constructible<Ty>::value) {
        if (other.size() <= this->capacity()) {
            this->append_no_realloc(other.size(), std::make_move_iterator(other.begin()));
        } else {
            this->move_allocated_data(other, reinterpret_cast<Ty*>(other.buf_), inplace_buf_size);
        }
    }

    void move_init_dispatch(dynarray&& other, std::false_type /* always equal allocators */) {
        if (this->is_same_alloc(other)) {
            move_init_dispatch(std::move(other), std::true_type());
        } else {
            this->init(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
        }
    }

    void copy_assign_dispatch(const dynarray& other, std::true_type /* propagate allocator */) {
        if (this->is_same_alloc(other)) {
            this->assign(other.begin(), other.end());
        } else {
            this->reset(reinterpret_cast<Ty*>(buf_), inplace_buf_size);
            alloc_type::operator=(other);
            this->init(other.begin(), other.end());
        }
    }

    void copy_assign_dispatch(const dynarray& other, std::false_type /* propagate allocator */) {
        this->assign(other.begin(), other.end());
    }

    void move_assign_dispatch(dynarray&& other, std::true_type /* propagate allocator */) noexcept(
        std::is_nothrow_move_constructible<Ty>::value) {
        this->reset(reinterpret_cast<Ty*>(buf_), inplace_buf_size);
        alloc_type::operator=(other);
        move_init_dispatch(std::move(other), std::true_type());
    }

    void move_assign_dispatch(dynarray&& other, std::false_type /* propagate allocator */) {
        if (this->is_same_alloc(other)) {
            move_assign_dispatch(std::move(other), std::true_type());
        } else {
            this->assign(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
        }
    }
};

template<typename Ty, typename Alloc>
class dynarray<Ty, 0, Alloc> : public detail::dynarray_base<Ty, Alloc> {
 private:
    using alloc_type = typename detail::dynarray_base<Ty, Alloc>::alloc_type;
    using alloc_traits = typename detail::dynarray_base<Ty, Alloc>::alloc_traits;

 public:
    using size_type = typename detail::dynarray_base<Ty, Alloc>::size_type;

    dynarray() noexcept(std::is_nothrow_default_constructible<Alloc>::value)
        : detail::dynarray_base<Ty, Alloc>(nullptr, 0) {}
    explicit dynarray(const Alloc& al) noexcept : detail::dynarray_base<Ty, Alloc>(al, nullptr, 0) {}
    explicit dynarray(size_type count, const Alloc& al = Alloc()) : dynarray(al) { this->init(count); }
    dynarray(size_type count, const Ty& val, const Alloc& al = Alloc()) : dynarray(al) { this->init(count, val); }
    dynarray(std::initializer_list<Ty> init_list, const Alloc& al = Alloc())
        : dynarray(init_list.begin(), init_list.begin() + init_list.size(), al) {}
    template<typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>>
    dynarray(InputIt first, InputIt last, const Alloc& al = Alloc()) : dynarray(al) {
        this->init(first, last);
    }

    dynarray(const dynarray& other)
        : dynarray(other.begin(), other.end(), alloc_traits::select_on_container_copy_construction(other)) {}
    dynarray(const dynarray& other, const Alloc& al) : dynarray(other.begin(), other.end(), al) {}

    dynarray(dynarray&& other) noexcept : detail::dynarray_base<Ty, Alloc>(std::move(other), nullptr, 0) {
        this->move_allocated_data(other, nullptr, 0);
    }
    dynarray(dynarray&& other, const Alloc& al) noexcept(alloc_traits::is_always_equal::value) : dynarray(al) {
        move_init_dispatch(std::move(other), typename alloc_traits::is_always_equal());
    }

    dynarray& operator=(std::initializer_list<Ty> init_list) {
        this->assign(init_list.begin(), init_list.begin() + init_list.size());
        return *this;
    }

    dynarray& operator=(const dynarray& other) {
        if (&other == this) { return *this; }
        this->copy_assign_dispatch(other,
                                   std::bool_constant<(alloc_traits::propagate_on_container_copy_assignment::value &&
                                                       !alloc_traits::is_always_equal::value)>());
        return *this;
    }

    dynarray& operator=(dynarray&& other) noexcept(alloc_traits::propagate_on_container_move_assignment::value ||
                                                   alloc_traits::is_always_equal::value) {
        if (&other == this) { return *this; }
        this->move_assign_dispatch(std::move(other),
                                   std::bool_constant<(alloc_traits::propagate_on_container_move_assignment::value ||
                                                       alloc_traits::is_always_equal::value)>());
        return *this;
    }

    void swap(dynarray& other) noexcept(alloc_traits::propagate_on_container_swap::value ||
                                        alloc_traits::is_always_equal::value) {
        if (&other == this) { return; }
        swap_dispatch(other, std::bool_constant<(alloc_traits::propagate_on_container_swap::value ||
                                                 alloc_traits::is_always_equal::value)>());
    }

    void shrink_to_fit() {
        const size_type min_capacity = this->eval_min_capacity(0);
        if (min_capacity == this->capacity()) { return; }
        if (min_capacity) {
            this->relocate(min_capacity);
        } else {
            this->reset(nullptr, 0);
        }
    }

 private:
    void move_init_dispatch(dynarray&& other, std::true_type /* always equal allocators */) noexcept {
        this->move_allocated_data(other, nullptr, 0);
    }

    void move_init_dispatch(dynarray&& other, std::false_type /* always equal allocators */) {
        if (this->is_same_alloc(other)) {
            this->move_allocated_data(other, nullptr, 0);
        } else {
            this->init(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
        }
    }

    void copy_assign_dispatch(const dynarray& other, std::true_type /* propagate allocator */) {
        if (this->is_same_alloc(other)) {
            this->assign(other.begin(), other.end());
        } else {
            this->reset(nullptr, 0);
            alloc_type::operator=(other);
            this->init(other.begin(), other.end());
        }
    }

    void copy_assign_dispatch(const dynarray& other, std::false_type /* propagate allocator */) {
        this->assign(other.begin(), other.end());
    }

    void move_assign_dispatch(dynarray&& other, std::true_type /* propagate allocator */) noexcept {
        this->reset(nullptr, 0);
        alloc_type::operator=(std::move(other));
        this->move_allocated_data(other, nullptr, 0);
    }

    void move_assign_dispatch(dynarray&& other, std::false_type /* propagate allocator */) {
        if (this->is_same_alloc(other)) {
            move_assign_dispatch(std::move(other), std::true_type());
        } else {
            this->assign(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
        }
    }

    void swap_dispatch(dynarray& other, std::true_type /* propagate allocator */) noexcept {
        std::swap(static_cast<alloc_type&>(*this), static_cast<alloc_type&>(other));
        this->swap_allocated_data(other);
    }

    void swap_dispatch(dynarray& other, std::false_type /* propagate allocator */) noexcept {
        this->swap_allocated_data(other);
    }
};

}  // namespace uxs

namespace std {
template<typename Ty, std::size_t InplaceBufSize, typename Alloc>
void swap(uxs::dynarray<Ty, InplaceBufSize, Alloc>& v1,
          uxs::dynarray<Ty, InplaceBufSize, Alloc>& v2) noexcept(noexcept(v1.swap(v2))) {
    v1.swap(v2);
}
}  // namespace std
