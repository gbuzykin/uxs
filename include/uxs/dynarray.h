#pragma once

#include "except.h"
#include "iterator.h"

#include <algorithm>
#include <cstring>

namespace uxs {

namespace detail {

template<typename Ty, typename Alloc>
class dynarray_base : protected std::allocator_traits<Alloc>::template rebind_alloc<Ty> {
 private:
    static_assert(std::is_same<std::remove_cv_t<Ty>, Ty>::value,
                  "uxs::dynarray must have a non-const, non-volatile value type");

    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<Ty>;
    using alloc_traits = std::allocator_traits<alloc_type>;

    struct iterator_traits {
        using value_type = Ty;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;
        using underlying_ptr_t = pointer;
    };

 public:
    using value_type = Ty;
    using allocator_type = Alloc;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using reference = value_type&;
    using const_reference = const value_type&;
    using iterator = est::array_iterator<iterator_traits, false>;
    using const_iterator = est::array_iterator<iterator_traits, true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    ~dynarray_base() { tidy(); }

    dynarray_base(const dynarray_base&) = delete;
    dynarray_base& operator=(const dynarray_base&) = delete;

    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    size_type capacity() const noexcept { return capacity_; }
    size_type max_size() const noexcept { return ((alloc_traits::max_size(*this) - 1) & ~size_type(1)) + 1; }
    allocator_type get_allocator() const noexcept { return allocator_type(*this); }

    iterator begin() noexcept { return iterator(data_, data_, data_ + size_); }
    const_iterator begin() const noexcept { return const_iterator(data_, data_, data_ + size_); }
    const_iterator cbegin() const noexcept { return begin(); }

    iterator end() noexcept { return iterator(data_ + size_, data_, data_ + size_); }
    const_iterator end() const noexcept { return const_iterator(data_ + size_, data_, data_ + size_); }
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
        assert(i < size_);
        return data_[i];
    }
    const_reference operator[](size_type i) const {
        assert(i < size_);
        return data_[i];
    }

    reference front() {
        assert(size_);
        return data_[0];
    }
    const_reference front() const {
        assert(size_);
        return data_[0];
    }

    reference back() {
        assert(size_);
        return data_[size_ - 1];
    }
    const_reference back() const {
        assert(size_);
        return data_[size_ - 1];
    }

    void clear() noexcept {
        destruct_items(data_, data_ + size_);
        size_ = 0;
    }

    void reserve(size_type size) {
        if (size > capacity_) { append_relocated(eval_new_capacity(size - size_)); }
    }

    void resize(size_type size) { resize_impl(size); }
    void resize(size_type size, const value_type& val) { resize_impl(size, val); }

    template<typename... Args>
    reference emplace_back(Args&&... args) {
        if (size_ != capacity_) {
            alloc_traits::construct(*this, data_ + size_, std::forward<Args>(args)...);
        } else {
            append_relocated(
                eval_new_capacity(1),
                [this](Ty* first, Args&&... args) {
                    alloc_traits::construct(*this, first, std::forward<Args>(args)...);
                    return first + 1;
                },
                std::forward<Args>(args)...);
        }
        size_++;
        return data_[size_ - 1];
    }

    void push_back(const value_type& val) { emplace_back(val); }
    void push_back(value_type&& val) { emplace_back(std::move(val)); }

    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        uxs_iterator_assert(pos.debug_begin() == data_ && pos.debug_end() == data_ + size_);
        assert(data_ <= pos.ptr() && pos.ptr() <= data_ + size_);
        const std::size_t n_pos = pos.ptr() - data_;
        emplace_back(std::forward<Args>(args)...);
        if (n_pos != size_ - 1) {
            Ty* last = data_ + size_;
            Ty t(std::move(*(last - 1)));
            std::move_backward(data_ + n_pos, last - 1, last);
            data_[n_pos] = std::move(t);
        }
        return iterator(data_ + n_pos, data_, data_ + size_);
    }

    iterator insert(const_iterator pos, const value_type& val) { return emplace(pos, val); }
    iterator insert(const_iterator pos, value_type&& val) { return emplace(pos, std::move(val)); }

    void pop_back() noexcept {
        assert(size_);
        --size_;
        alloc_traits::destroy(*this, data_ + size_);
    }

    iterator erase(const_iterator pos) {
        uxs_iterator_assert(pos.debug_begin() == data_ && pos.debug_end() == data_ + size_);
        assert(data_ <= pos.ptr() && pos.ptr() < data_ + size_);
        std::move(pos.ptr() + 1, data_ + size_, pos.ptr());
        --size_;
        alloc_traits::destroy(*this, data_ + size_);
        return iterator(pos.ptr(), data_, data_ + size_);
    }

    iterator erase(const_iterator first, const_iterator last) {
        uxs_iterator_assert(first.debug_begin() == data_ && first.debug_end() == data_ + size_);
        uxs_iterator_assert(last.debug_begin() == data_ && last.debug_end() == data_ + size_);
        assert(data_ <= first.ptr() && first.ptr() <= last.ptr() && last.ptr() <= data_ + size_);
        std::move(last.ptr(), data_ + size_, first.ptr());
        const auto count = last.ptr() - first.ptr();
        size_ -= count;
        destruct_items(data_ + size_, data_ + size_ + count);
        return iterator(first.ptr(), data_, data_ + size_);
    }

 protected:
    dynarray_base(Ty* data, size_type capacity) noexcept(std::is_nothrow_default_constructible<Alloc>::value)
        : alloc_type(), data_(data), size_(0), capacity_(capacity) {
        assert(!(capacity & 1));
    }
    dynarray_base(const Alloc& al, Ty* data, size_type capacity) noexcept
        : alloc_type(al), data_(data), size_(0), capacity_(capacity) {
        assert(!(capacity & 1));
    }

    template<typename... Args>
    void init(size_type count, Args&&... args);

    template<typename... Args>
    void resize_impl(size_type size, Args&&... args);

    size_type eval_new_capacity(size_type extra) const;

    void destruct_items(Ty* first, Ty* last) noexcept {
        destruct_items_dispatch(first, last,
                                std::bool_constant<std::is_trivially_destructible<Ty>::value &&
                                                   std::is_same<alloc_type, std::allocator<Ty>>::value>());
    }

    template<typename Ty_>
    void destruct_items_dispatch(Ty_* /*first*/, Ty_* /*last*/, std::true_type /* trivially destructible */) noexcept {}

    template<typename Ty_>
    void destruct_items_dispatch(Ty_* first, Ty_* last, std::false_type /* trivially destructible */) noexcept {
        for (; first != last; ++first) { alloc_traits::destroy(*this, first); };
    }

    template<typename... Args>
    void append_relocated(size_type capacity, Args&&... args) {
        Ty* new_data = append_relocated_dispatch(capacity, std::is_trivially_copyable<Ty>(),
                                                 std::is_nothrow_move_constructible<Ty>(), std::forward<Args>(args)...);
        tidy();
        data_ = new_data;
        capacity_ = capacity;
    }

    template<typename Ty_ = Ty>
    Ty_* append_relocated_dispatch(size_type capacity, std::true_type /* trivially copyable */,
                                   std::true_type /* nothrow move constructible */);

    template<typename Ty_ = Ty>
    Ty_* append_relocated_dispatch(size_type capacity, std::false_type /* trivially copyable */,
                                   std::true_type /* nothrow move constructible */);

    template<typename AppendFn, typename... Args>
    Ty* append_relocated_dispatch(size_type capacity, std::true_type /* trivially copyable */,
                                  std::true_type /* nothrow move constructible */, AppendFn&& append_fn,
                                  Args&&... args);

    template<typename AppendFn, typename... Args>
    Ty* append_relocated_dispatch(size_type capacity, std::false_type /* trivially copyable */,
                                  std::true_type /* nothrow move constructible */, AppendFn&& append_fn,
                                  Args&&... args);

    template<typename AppendFn = est::identity, typename... Args>
    Ty* append_relocated_dispatch(size_type capacity, std::false_type /* trivially copyable */,
                                  std::false_type /* nothrow move constructible */, AppendFn&& append_fn = AppendFn{},
                                  Args&&... args);

    void tidy() noexcept {
        destruct_items(data_, data_ + size_);
        if (capacity_ & 1) { alloc_traits::deallocate(*this, data_, capacity_); }
    }

 private:
    Ty* data_;
    size_type size_;
    size_type capacity_;
};

template<typename Ty, typename Alloc>
template<typename... Args>
void dynarray_base<Ty, Alloc>::init(size_type count, Args&&... args) {
    if (count > capacity_) {
        if (count > max_size()) { report_too_much_to_allocate_error(); }
        capacity_ = (count & ~size_type(1)) + 1;  // Make new dynamic odd capacity
        data_ = alloc_traits::allocate(*this, capacity_);
    }
    Ty* dst = data_;
    try {
        for (; dst != data_ + count; ++dst) { alloc_traits::construct(*this, dst, std::forward<Args>(args)...); }
        size_ = count;
    } catch (...) {
        destruct_items(data_, dst);
        if (capacity_ & 1) { alloc_traits::deallocate(*this, data_, capacity_); }
        throw;
    }
}

template<typename Ty, typename Alloc>
template<typename... Args>
void dynarray_base<Ty, Alloc>::resize_impl(size_type size, Args&&... args) {
    if (size <= size_) {
        destruct_items(data_ + size, data_ + size_);
    } else if (size <= capacity_) {
        Ty* dst = data_ + size_;
        try {
            for (; dst != data_ + size; ++dst) { alloc_traits::construct(*this, dst, std::forward<Args>(args)...); }
        } catch (...) {
            destruct_items(data_ + size_, dst);
            throw;
        }
    } else {
        append_relocated(
            eval_new_capacity(size - size_),
            [this](Ty* first, std::size_t count, Args&&... args) {
                for (Ty* last = first + count; first != last; ++first) {
                    alloc_traits::construct(*this, first, std::forward<Args>(args)...);
                }
                return first;
            },
            size - size_, std::forward<Args>(args)...);
    }
    size_ = size;
}

template<typename Ty, typename Alloc>
auto dynarray_base<Ty, Alloc>::eval_new_capacity(size_type extra) const -> size_type {
    const size_type size = size_;
    size_type delta_sz = std::max(extra, size >> 1);
    const size_type max_avail = max_size() - size;
    if (delta_sz > max_avail) {
        if (extra > max_avail) { report_too_much_to_allocate_error(); }
        delta_sz = std::max(extra, max_avail >> 1);
    }
    return ((size + delta_sz) & ~size_type(1)) + 1;  // Make new dynamic odd capacity
}

template<typename Ty, typename Alloc>
template<typename Ty_>
Ty_* dynarray_base<Ty, Alloc>::append_relocated_dispatch(size_type capacity, std::true_type /* trivially copyable */,
                                                         std::true_type /* nothrow move constructible */) {
    Ty* new_data = alloc_traits::allocate(*this, capacity);
    if (size_) { std::memcpy(new_data, data_, size_ * sizeof(Ty)); }
    return new_data;
}

template<typename Ty, typename Alloc>
template<typename Ty_>
Ty_* dynarray_base<Ty, Alloc>::append_relocated_dispatch(size_type capacity, std::false_type /* trivially copyable */,
                                                         std::true_type /* nothrow move constructible */) {
    Ty* new_data = alloc_traits::allocate(*this, capacity);
    Ty* dst = new_data;
    for (Ty* src = data_; src != data_ + size_; ++dst, ++src) { alloc_traits::construct(*this, dst, std::move(*src)); };
    return new_data;
}

template<typename Ty, typename Alloc>
template<typename AppendFn, typename... Args>
Ty* dynarray_base<Ty, Alloc>::append_relocated_dispatch(size_type capacity, std::true_type /* trivially copyable */,
                                                        std::true_type /* nothrow move constructible */,
                                                        AppendFn&& append_fn, Args&&... args) {
    Ty* new_data = alloc_traits::allocate(*this, capacity);
    Ty* dst_last = new_data + size_;
    try {
        dst_last = append_fn(dst_last, std::forward<Args>(args)...);
        if (size_) { std::memcpy(new_data, data_, size_ * sizeof(Ty)); }
        return new_data;
    } catch (...) {
        destruct_items(new_data + size_, dst_last);
        alloc_traits::deallocate(*this, new_data, capacity);
        throw;
    }
}

template<typename Ty, typename Alloc>
template<typename AppendFn, typename... Args>
Ty* dynarray_base<Ty, Alloc>::append_relocated_dispatch(size_type capacity, std::false_type /* trivially copyable */,
                                                        std::true_type /* nothrow move constructible */,
                                                        AppendFn&& append_fn, Args&&... args) {
    Ty* new_data = alloc_traits::allocate(*this, capacity);
    Ty* dst_last = new_data + size_;
    try {
        dst_last = append_fn(dst_last, std::forward<Args>(args)...);
        Ty* dst = new_data;
        for (Ty* src = data_; src != data_ + size_; ++dst, ++src) {
            alloc_traits::construct(*this, dst, std::move(*src));
        };
        return new_data;
    } catch (...) {
        destruct_items(new_data + size_, dst_last);
        alloc_traits::deallocate(*this, new_data, capacity);
        throw;
    }
}

template<typename Ty, typename Alloc>
template<typename AppendFn, typename... Args>
Ty* dynarray_base<Ty, Alloc>::append_relocated_dispatch(size_type capacity, std::false_type /* trivially copyable */,
                                                        std::false_type /* nothrow move constructible */,
                                                        AppendFn&& append_fn, Args&&... args) {
    Ty* new_data = alloc_traits::allocate(*this, capacity);
    Ty* dst = new_data;
    Ty* dst_last = new_data + size_;
    try {
        dst_last = append_fn(dst_last, std::forward<Args>(args)...);
        for (const Ty* src = data_; src != data_ + size_; ++dst, ++src) { alloc_traits::construct(*this, dst, *src); };
        return new_data;
    } catch (...) {
        destruct_items(new_data, dst);
        destruct_items(new_data + size_, dst_last);
        alloc_traits::deallocate(*this, new_data, capacity);
        throw;
    }
}

}  // namespace detail

template<typename Ty, std::size_t InStackBufSize = est::dynamic_extent, typename Alloc = std::allocator<Ty>>
class dynarray : public detail::dynarray_base<Ty, Alloc> {
 public:
    using size_type = typename detail::dynarray_base<Ty, Alloc>::size_type;
    using value_type = typename detail::dynarray_base<Ty, Alloc>::value_type;

    dynarray() noexcept(std::is_nothrow_default_constructible<Alloc>::value)
        : detail::dynarray_base<Ty, Alloc>(reinterpret_cast<Ty*>(buf_), in_stack_buf_size) {}
    explicit dynarray(const Alloc& al) noexcept
        : detail::dynarray_base<Ty, Alloc>(al, reinterpret_cast<Ty*>(buf_), in_stack_buf_size) {}
    explicit dynarray(size_type count, const Alloc& al = Alloc())
        : detail::dynarray_base<Ty, Alloc>(al, reinterpret_cast<Ty*>(buf_), in_stack_buf_size) {
        this->init(count);
    }
    dynarray(size_type count, const value_type& val, const Alloc& al = Alloc())
        : detail::dynarray_base<Ty, Alloc>(al, reinterpret_cast<Ty*>(buf_), in_stack_buf_size) {
        this->init(count, val);
    }

 private:
    enum : unsigned {  // Must be even
#if UXS_DEBUG_REDUCED_BUFFERS != 0
        in_stack_buf_size = 2,
#else   // UXS_DEBUG_REDUCED_BUFFERS != 0
        in_stack_buf_size = InStackBufSize != est::dynamic_extent ? ((InStackBufSize + 1) & ~size_type(1)) : 16,
#endif  // UXS_DEBUG_REDUCED_BUFFERS != 0
    };

    alignas(std::alignment_of<Ty>::value) std::uint8_t buf_[in_stack_buf_size * sizeof(Ty)];
};

template<typename Ty, typename Alloc>
class dynarray<Ty, 0, Alloc> : public detail::dynarray_base<Ty, Alloc> {
 public:
    using size_type = typename detail::dynarray_base<Ty, Alloc>::size_type;
    using value_type = typename detail::dynarray_base<Ty, Alloc>::value_type;

    dynarray() noexcept(std::is_nothrow_default_constructible<Alloc>::value)
        : detail::dynarray_base<Ty, Alloc>(nullptr, 0) {}
    explicit dynarray(const Alloc& al) noexcept : detail::dynarray_base<Ty, Alloc>(al, nullptr, 0) {}
    explicit dynarray(size_type count, const Alloc& al = Alloc()) : detail::dynarray_base<Ty, Alloc>(al, nullptr, 0) {
        this->init(count);
    }
    dynarray(size_type count, const value_type& val, const Alloc& al = Alloc())
        : detail::dynarray_base<Ty, Alloc>(al, nullptr, 0) {
        this->init(count, val);
    }
};

}  // namespace uxs
