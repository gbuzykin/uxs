#pragma once

#include "iterator.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace uxs {

template<typename Ty, typename Alloc = std::allocator<Ty>>
class dynarray : protected std::allocator_traits<Alloc>::template rebind_alloc<Ty> {
 private:
    static_assert(std::is_same<std::remove_cv_t<Ty>, Ty>::value,
                  "util::dynarray must have a non-const, non-volatile value type");

    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<Ty>;
    using alloc_traits = std::allocator_traits<alloc_type>;

 public:
    using value_type = Ty;
    using allocator_type = Alloc;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using reference = value_type&;
    using const_reference = const value_type&;
    using iterator = est::array_iterator<dynarray, pointer, false>;
    using const_iterator = est::array_iterator<dynarray, pointer, true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    dynarray() noexcept(std::is_nothrow_default_constructible<alloc_type>::value) : alloc_type() {}
    explicit dynarray(const Alloc& alloc) noexcept : alloc_type(alloc) {}
    explicit dynarray(size_type count, const Alloc& alloc = Alloc()) : alloc_type(alloc) { init(count); }
    dynarray(size_type count, const value_type& val, const Alloc& alloc = Alloc()) : alloc_type(alloc) {
        init(count, val);
    }
    ~dynarray() { tidy(); }

    dynarray(const dynarray&) = delete;
    dynarray& operator=(const dynarray&) = delete;

    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    size_type capacity() const noexcept { return capacity_; }
    size_type max_size() const noexcept { return alloc_traits::max_size(*this); }
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
        assert(size_ != 0);
        return data_[0];
    }
    const_reference front() const {
        assert(size_ != 0);
        return data_[0];
    }

    reference back() {
        assert(size_ != 0);
        return data_[size_ - 1];
    }
    const_reference back() const {
        assert(size_ != 0);
        return data_[size_ - 1];
    }

    void clear() noexcept {
        destruct_items(data_, data_ + size_);
        size_ = 0;
    }

    void reserve(size_type size) {
        if (size > capacity_) { append_relocated(*this, eval_new_capacity(size - size_)); }
    }

    void resize(size_type size) { resize_impl(size); }
    void resize(size_type size, const value_type& val) { resize_impl(size, val); }

    template<typename... Args>
    reference emplace_back(Args&&... args) {
        if (size_ != capacity_) {
            ::new (data_ + size_) value_type(std::forward<Args>(args)...);
        } else {
            append_relocated(
                *this, eval_new_capacity(1),
                [](Ty* first, Args&&... args) {
                    ::new (first) value_type(std::forward<Args>(args)...);
                    return first + 1;
                },
                std::forward<Args>(args)...);
        }
        return data_[size_++];
    }

    void push_back(const value_type& val) { emplace_back(val); }
    void push_back(value_type&& val) { emplace_back(std::move(val)); }

    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        const std::size_t n = static_cast<std::size_t>(pos - cbegin());
        emplace_back(std::forward<Args>(args)...);
        if (n < size_ - 1) {
            value_type t = std::move(data_[size_ - 1]);
            for (std::size_t i = size_ - 1; i > n; --i) { data_[i] = std::move(data_[i - 1]); }
            data_[n] = std::move(t);
        }
        return iterator(data_ + n, data_, data_ + size_);
    }

    iterator insert(const_iterator pos, const value_type& val) { return emplace(pos, val); }
    iterator insert(const_iterator pos, value_type&& val) { return emplace(pos, std::move(val)); }

    void pop_back() noexcept {
        assert(size_ != 0);
        data_[--size_].~value_type();
    }

    iterator erase(const_iterator pos) {
        assert(size_ != 0);
        const std::size_t n = static_cast<std::size_t>(pos - cbegin());
        if (n < size_ - 1) {
            for (std::size_t i = n; i < size_ - 1; ++i) { data_[i] = std::move(data_[i + 1]); }
        }
        data_[--size_].~value_type();
        return iterator(data_ + n, data_, data_ + size_);
    }

 protected:
    dynarray(Ty* data, size_type capacity) noexcept(std::is_nothrow_default_constructible<alloc_type>::value)
        : alloc_type(), data_(data), capacity_(capacity) {
        assert(!(capacity & 1));
    }
    dynarray(Ty* data, size_type capacity, const Alloc& alloc) noexcept
        : alloc_type(alloc), data_(data), capacity_(capacity) {
        assert(!(capacity & 1));
    }
    dynarray(Ty* data, size_type capacity, size_type count, const Alloc& alloc)
        : alloc_type(alloc), data_(data), capacity_(capacity) {
        assert(!(capacity & 1));
        init(count);
    }
    dynarray(Ty* data, size_type capacity, size_type count, const value_type& val, const Alloc& alloc)
        : alloc_type(alloc), data_(data), capacity_(capacity) {
        assert(!(capacity & 1));
        init(count, val);
    }

    size_type eval_new_capacity(size_type extra) const;

    template<typename... Args>
    void init(size_type count, Args&&... args);

    template<typename... Args>
    void resize_impl(size_type size, Args&&... args);

    static void destruct_items(Ty* first, Ty* last) noexcept {
        destruct_items_dispatch(first, last, std::is_trivially_destructible<Ty>());
    }

    template<typename Ty_>
    static void destruct_items_dispatch(Ty_* /*first*/, Ty_* /*last*/,
                                        std::true_type /* trivially destructible */) noexcept {}

    template<typename Ty_>
    static void destruct_items_dispatch(Ty_* first, Ty_* last, std::false_type /* trivially destructible */) noexcept {
        for (; first != last; ++first) { first->~value_type(); };
    }

    template<typename... Args>
    void append_relocated(alloc_type& al, size_type capacity, Args&&... args) {
        Ty* new_data = append_relocated_dispatch(al, capacity, std::is_trivially_copyable<Ty>(),
                                                 std::is_nothrow_move_constructible<Ty>(), std::forward<Args>(args)...);
        tidy();
        data_ = new_data;
        capacity_ = capacity;
    }

    template<typename Ty_ = Ty>
    Ty* append_relocated_dispatch(alloc_type& al, size_type capacity, std::true_type /* trivially copyable */,
                                  std::true_type /* nothrow move constructible */);

    template<typename Ty_ = Ty>
    Ty* append_relocated_dispatch(alloc_type& al, size_type capacity, std::false_type /* trivially copyable */,
                                  std::true_type /* nothrow move constructible */);

    template<typename AppendFn, typename... Args>
    Ty* append_relocated_dispatch(alloc_type& al, size_type capacity, std::true_type /* trivially copyable */,
                                  std::true_type /* nothrow move constructible */, AppendFn&& append_fn,
                                  Args&&... args);

    template<typename AppendFn, typename... Args>
    Ty* append_relocated_dispatch(alloc_type& al, size_type capacity, std::false_type /* trivially copyable */,
                                  std::true_type /* nothrow move constructible */, AppendFn&& append_fn,
                                  Args&&... args);

    template<typename AppendFn = est::identity, typename... Args>
    Ty* append_relocated_dispatch(alloc_type& al, size_type capacity, std::false_type /* trivially copyable */,
                                  std::false_type /* nothrow move constructible */, AppendFn&& append_fn = AppendFn{},
                                  Args&&... args);

    void tidy() noexcept {
        destruct_items(data_, data_ + size_);
        if (capacity_ & 1) { alloc_traits::deallocate(*this, data_, capacity_); }
    }

 private:
    Ty* data_ = nullptr;
    size_type size_ = 0;
    size_type capacity_ = 0;
};

template<typename Ty, typename Alloc>
auto dynarray<Ty, Alloc>::eval_new_capacity(size_type extra) const -> size_type {
    const size_type size = size_;
    size_type delta_sz = std::max(++extra, size >> 1);
    const size_type max_avail = std::allocator_traits<alloc_type>::max_size(*this) - size;
    if (delta_sz > max_avail) {
        if (extra > max_avail) { throw std::length_error("too much to reserve"); }
        delta_sz = std::max(extra, max_avail >> 1);
    }
    return ((size + delta_sz - 1) & ~size_type(1)) + 1;  // Make new dynamic odd capacity
}

template<typename Ty, typename Alloc>
template<typename... Args>
void dynarray<Ty, Alloc>::init(size_type count, Args&&... args) {
    if (count > capacity_) {
        capacity_ = (count & ~size_type(1)) + 1;  // Make new dynamic odd capacity
        data_ = alloc_traits::allocate(*this, capacity_);
    }
    Ty* dst = data_;
    try {
        for (; dst != data_ + count; ++dst) { ::new (dst) value_type(std::forward<Args>(args)...); }
        size_ = count;
    } catch (...) {
        destruct_items(data_, dst);
        if (capacity_ & 1) { alloc_traits::deallocate(*this, data_, capacity_); }
        throw;
    }
}

template<typename Ty, typename Alloc>
template<typename... Args>
void dynarray<Ty, Alloc>::resize_impl(size_type size, Args&&... args) {
    if (size <= size_) {
        destruct_items(data_ + size, data_ + size_);
    } else if (size <= capacity_) {
        Ty* dst = data_ + size_;
        try {
            for (; dst != data_ + size; ++dst) { ::new (dst) value_type(std::forward<Args>(args)...); }
        } catch (...) {
            destruct_items(data_ + size_, dst);
            throw;
        }
    } else {
        append_relocated(
            *this, eval_new_capacity(size - size_),
            [](Ty* first, std::size_t count, Args&&... args) {
                for (Ty* last = first + count; first != last; ++first) {
                    ::new (first) value_type(std::forward<Args>(args)...);
                }
                return first;
            },
            size - size_, std::forward<Args>(args)...);
    }
    size_ = size;
}

template<typename Ty, typename Alloc>
template<typename>
Ty* dynarray<Ty, Alloc>::append_relocated_dispatch(alloc_type& al, size_type capacity,
                                                   std::true_type /* trivially copyable */,
                                                   std::true_type /* nothrow move constructible */) {
    Ty* new_data = alloc_traits::allocate(al, capacity);
    std::memcpy(new_data, data_, size_ * sizeof(Ty));
    return new_data;
}

template<typename Ty, typename Alloc>
template<typename>
Ty* dynarray<Ty, Alloc>::append_relocated_dispatch(alloc_type& al, size_type capacity,
                                                   std::false_type /* trivially copyable */,
                                                   std::true_type /* nothrow move constructible */) {
    Ty* new_data = alloc_traits::allocate(al, capacity);
    Ty* dst = new_data;
    for (Ty* src = data_; src != data_ + size_; ++dst, ++src) { ::new (dst) value_type(std::move(*src)); };
    return new_data;
}

template<typename Ty, typename Alloc>
template<typename AppendFn, typename... Args>
Ty* dynarray<Ty, Alloc>::append_relocated_dispatch(alloc_type& al, size_type capacity,
                                                   std::true_type /* trivially copyable */,
                                                   std::true_type /* nothrow move constructible */,
                                                   AppendFn&& append_fn, Args&&... args) {
    Ty* new_data = alloc_traits::allocate(al, capacity);
    Ty* dst_last = new_data + size_;
    try {
        dst_last = append_fn(dst_last, std::forward<Args>(args)...);
        std::memcpy(new_data, data_, size_ * sizeof(Ty));
        return new_data;
    } catch (...) {
        destruct_items(new_data + size_, dst_last);
        alloc_traits::deallocate(al, new_data, capacity);
        throw;
    }
}

template<typename Ty, typename Alloc>
template<typename AppendFn, typename... Args>
Ty* dynarray<Ty, Alloc>::append_relocated_dispatch(alloc_type& al, size_type capacity,
                                                   std::false_type /* trivially copyable */,
                                                   std::true_type /* nothrow move constructible */,
                                                   AppendFn&& append_fn, Args&&... args) {
    Ty* new_data = alloc_traits::allocate(al, capacity);
    Ty* dst_last = new_data + size_;
    try {
        dst_last = append_fn(dst_last, std::forward<Args>(args)...);
        Ty* dst = new_data;
        for (Ty* src = data_; src != data_ + size_; ++dst, ++src) { ::new (dst) value_type(std::move(*src)); };
        return new_data;
    } catch (...) {
        destruct_items(new_data + size_, dst_last);
        alloc_traits::deallocate(al, new_data, capacity);
        throw;
    }
}

template<typename Ty, typename Alloc>
template<typename AppendFn, typename... Args>
Ty* dynarray<Ty, Alloc>::append_relocated_dispatch(alloc_type& al, size_type capacity,
                                                   std::false_type /* trivially copyable */,
                                                   std::false_type /* nothrow move constructible */,
                                                   AppendFn&& append_fn, Args&&... args) {
    Ty* new_data = alloc_traits::allocate(al, capacity);
    Ty* dst = new_data;
    Ty* dst_last = new_data + size_;
    try {
        dst_last = append_fn(dst_last, std::forward<Args>(args)...);
        for (const Ty* src = data_; src != data_ + size_; ++dst, ++src) { ::new (dst) value_type(*src); };
        return new_data;
    } catch (...) {
        destruct_items(new_data, dst);
        destruct_items(new_data + size_, dst_last);
        alloc_traits::deallocate(al, new_data, capacity);
        throw;
    }
}

template<typename Ty, std::size_t InlineBufSize = 0, typename Alloc = std::allocator<Ty>>
class inline_dynarray : public dynarray<Ty, Alloc> {
 public:
    using size_type = typename dynarray<Ty, Alloc>::size_type;
    using value_type = typename dynarray<Ty, Alloc>::value_type;

    inline_dynarray() noexcept(std::is_nothrow_default_constructible<dynarray<Ty, Alloc>>::value)
        : dynarray<Ty, Alloc>(reinterpret_cast<Ty*>(buf_), inline_buf_size) {}
    explicit inline_dynarray(const Alloc& alloc) noexcept
        : dynarray<Ty, Alloc>(reinterpret_cast<Ty*>(buf_), inline_buf_size, alloc) {}
    explicit inline_dynarray(size_type count, const Alloc& alloc = Alloc())
        : dynarray<Ty, Alloc>(reinterpret_cast<Ty*>(buf_), inline_buf_size, count, alloc) {}
    inline_dynarray(size_type count, const value_type& val, const Alloc& alloc = Alloc())
        : dynarray<Ty, Alloc>(reinterpret_cast<Ty*>(buf_), inline_buf_size, count, val, alloc) {}

 private:
    enum : unsigned {  // Always even
#if UXS_DEBUG_REDUCED_BUFFERS != 0
        inline_buf_size = 2,
#else   // UXS_DEBUG_REDUCED_BUFFERS != 0
        inline_buf_size = InlineBufSize != 0 ? ((InlineBufSize + 1) & ~1U) : 16,
#endif  // UXS_DEBUG_REDUCED_BUFFERS != 0
    };

    alignas(std::alignment_of<Ty>::value) std::uint8_t buf_[inline_buf_size * sizeof(Ty)];
};

}  // namespace uxs
