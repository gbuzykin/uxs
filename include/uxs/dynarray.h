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
    using iterator = uxs::array_iterator<dynarray, pointer, false>;
    using const_iterator = uxs::array_iterator<dynarray, pointer, true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    dynarray() noexcept(std::is_nothrow_default_constructible<alloc_type>::value) : alloc_type() {}
    explicit dynarray(const Alloc& alloc) noexcept : alloc_type(alloc) {}
    explicit dynarray(size_type count, const Alloc& alloc = Alloc()) : alloc_type(alloc) { init(count); }
    ~dynarray() {
        destruct_items(data_, data_ + size_);
        if (capacity_ & 1) { alloc_traits::deallocate(*this, data_, capacity_); }
    }

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

    void reserve(size_type sz) {
        if (sz > capacity_) { grow(sz - size_); }
    }

    void resize(size_type sz);

    template<typename... Args>
    reference emplace_back(Args&&... args) {
        if (size_ == capacity_) { grow(1); }
        return *::new (&data_[size_++]) value_type(std::forward<Args>(args)...);
    }

    void push_back(const value_type& val) { emplace_back(val); }
    void push_back(value_type&& val) { emplace_back(std::move(val)); }

    void pop_back() noexcept {
        assert(size_ != 0);
        data_[--size_].~value_type();
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

    void grow(size_type extra);

    void init(size_type count);

    template<typename Ty_ = Ty, typename = std::enable_if_t<std::is_trivially_destructible<Ty_>::value>>
    static void destruct_items(Ty_* /*first*/, Ty_* /*last*/) noexcept {}

    template<typename Ty_ = Ty, typename... Dummy>
    static void destruct_items(Ty_* first, Ty_* last, Dummy&&...) noexcept {
        static_assert(!std::is_trivially_destructible<Ty>::value, "");
        for (; first != last; ++first) { first->~value_type(); };
    }

    template<typename Ty_ = Ty, typename = std::enable_if_t<std::is_nothrow_move_constructible<Ty_>::value>>
    static Ty* move_items(alloc_type& al, size_type sz, Ty_* first, Ty_* last) {
        Ty* data = alloc_traits::allocate(al, sz);
        for (Ty* dst = data; first != last; ++first, ++dst) { ::new (dst) value_type(std::move(*first)); };
        return data;
    }

    template<typename Ty_ = Ty, typename... Dummy>
    static Ty* move_items(alloc_type& al, size_type sz, Ty_* first, Ty_* last, Dummy&&...) {
        static_assert(!std::is_nothrow_move_constructible<Ty>::value, "");
        Ty* data = alloc_traits::allocate(al, sz);
        try {
            std::uninitialized_copy(first, last, data);
            return data;
        } catch (...) {
            alloc_traits::deallocate(al, data, sz);
            throw;
        }
    }

 private:
    Ty* data_ = nullptr;
    size_type size_ = 0;
    size_type capacity_ = 0;
};

template<typename Ty, typename Alloc>
void dynarray<Ty, Alloc>::resize(size_type sz) {
    if (sz > size_) {
        if (sz > capacity_) { grow(sz - size_); }
        Ty* item = data_ + size_;
        try {
            for (Ty* last = data_ + sz; item != last; ++item) { alloc_traits::construct(*this, item); }
        } catch (...) {
            destruct_items(data_ + size_, item);
            throw;
        }
    } else {
        destruct_items(data_ + sz, data_ + size_);
    }
    size_ = sz;
}

template<typename Ty, typename Alloc>
void dynarray<Ty, Alloc>::init(size_type count) {
    if (count > capacity_) {
        capacity_ = (count & ~size_type(1)) + 1;  // Make new dynamic odd capacity
        data_ = alloc_traits::allocate(*this, capacity_);
    }
    Ty* item = data_;
    try {
        for (Ty* last = data_ + count; item != last; ++item) { alloc_traits::construct(*this, item); }
        size_ = count;
    } catch (...) {
        destruct_items(data_, item);
        if (capacity_ & 1) { alloc_traits::deallocate(*this, data_, capacity_); }
        throw;
    }
}

template<typename Ty, typename Alloc>
void dynarray<Ty, Alloc>::grow(size_type extra) {
    const size_type sz = size_;
    size_type delta_sz = std::max(++extra, sz >> 1);
    const size_type max_avail = std::allocator_traits<alloc_type>::max_size(*this) - sz;
    if (delta_sz > max_avail) {
        if (extra > max_avail) { throw std::length_error("too much to reserve"); }
        delta_sz = std::max(extra, max_avail >> 1);
    }
    const size_type capacity = ((sz + delta_sz - 1) & ~size_type(1)) + 1;  // Make new dynamic odd capacity
    Ty* data = move_items(*this, capacity, data_, data_ + size_);
    destruct_items(data_, data_ + size_);
    if (capacity_ & 1) { alloc_traits::deallocate(*this, data_, capacity_); }
    data_ = data, capacity_ = capacity;
}

template<typename Ty, std::size_t InlineBufSize = 0, typename Alloc = std::allocator<Ty>>
class inline_dynarray final : public dynarray<Ty, Alloc> {
 public:
    using size_type = typename dynarray<Ty, Alloc>::size_type;

    inline_dynarray() noexcept(std::is_nothrow_default_constructible<dynarray<Ty, Alloc>>::value)
        : dynarray<Ty, Alloc>(reinterpret_cast<Ty*>(buf_), inline_buf_size) {}
    explicit inline_dynarray(const Alloc& alloc) noexcept
        : dynarray<Ty, Alloc>(reinterpret_cast<Ty*>(buf_), inline_buf_size, alloc) {}
    explicit inline_dynarray(size_type count, const Alloc& alloc = Alloc())
        : dynarray<Ty, Alloc>(reinterpret_cast<Ty*>(buf_), inline_buf_size, count, alloc) {}

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
