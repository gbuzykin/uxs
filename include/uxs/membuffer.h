#pragma once

#include "iterator.h"
#include "string_view.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace uxs {

template<typename Ty>
class basic_membuffer {
 private:
    static_assert(std::is_same<std::remove_cv_t<Ty>, Ty>::value,
                  "uxs::basic_membuffer<> must have a non-const, non-volatile value type");
    static_assert(std::is_trivially_copyable<Ty>::value && std::is_trivially_destructible<Ty>::value,
                  "uxs::basic_membuffer<> must have trivially copyable and destructible value type");

 public:
    using value_type = Ty;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using reference = value_type&;
    using const_reference = const value_type&;
    using const_iterator = array_iterator<basic_membuffer, const_pointer, true>;
    using iterator = const_iterator;

    basic_membuffer() noexcept = default;
    explicit basic_membuffer(pointer data) noexcept : data_(data), capacity_(std::numeric_limits<size_type>::max()) {}
    basic_membuffer(pointer data, size_type capacity) noexcept : data_(data), capacity_(capacity) {}
    basic_membuffer(pointer data, size_type size, size_type capacity) noexcept
        : data_(data), size_(size), capacity_(capacity) {}
    virtual ~basic_membuffer() = default;
    basic_membuffer(const basic_membuffer&) = delete;
    basic_membuffer& operator=(const basic_membuffer&) = delete;

    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    size_type capacity() const noexcept { return capacity_; }
    size_type avail() const noexcept { return capacity_ - size_; }

    iterator begin() noexcept { return iterator(data_, data_, endp()); }
    const_iterator begin() const noexcept { return const_iterator(data_, data_, endp()); }
    const_iterator cbegin() const noexcept { return begin(); }

    iterator end() noexcept { return iterator(endp(), data_, endp()); }
    const_iterator end() const noexcept { return const_iterator(endp(), data_, endp()); }
    const_iterator cend() const noexcept { return end(); }

    pointer data() noexcept { return data_; }
    const_pointer data() const noexcept { return data_; }
    pointer endp() noexcept { return data_ + size_; }
    const_pointer endp() const noexcept { return data_ + size_; }

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

    void clear() noexcept { size_ = 0; }

    void setsize(size_type size) noexcept {
        assert(size <= capacity_);
        size_ = size;
    }

    void advance(difference_type n) noexcept {
        assert(n >= 0 ? static_cast<size_type>(n) <= avail() : static_cast<size_type>(-n) <= size_);
        size_ += n;
    }

    template<typename InputIt, typename = std::enable_if_t<is_random_access_iterator<InputIt>::value &&
                                                           std::is_same<iterator_value_t<InputIt>, Ty>::value>>
    basic_membuffer& append(InputIt first, InputIt last) {
        assert(first <= last);
        size_type count = static_cast<size_type>(last - first);
        size_type n_avail = avail();
        while (count > n_avail) {
            std::copy_n(first, n_avail, endp());
            first += n_avail, count -= n_avail, size_ += n_avail;
            if (!(n_avail = try_grow(count))) { return *this; }
        }
        std::copy(first, last, endp());
        size_ += count;
        return *this;
    }

    basic_membuffer& append(size_type count, const value_type& val) {
        size_type n_avail = avail();
        while (count > n_avail) {
            std::fill_n(endp(), n_avail, val);
            count -= n_avail, size_ += n_avail;
            if (!(n_avail = try_grow(count))) { return *this; }
        }
        std::fill_n(endp(), count, val);
        size_ += count;
        return *this;
    }

    template<typename... Args>
    void emplace_back(Args&&... args) {
        if (size_ != capacity_ || try_grow(1)) { ::new (&data_[size_++]) value_type(std::forward<Args>(args)...); }
    }
    void push_back(const value_type& val) { emplace_back(val); }

    void pop_back() noexcept {
        assert(size_ != 0);
        --size_;
    }

    template<typename CharT = value_type>
    std::enable_if_t<is_character<CharT>::value, basic_membuffer&> append(const_pointer s, size_type count) {
        return append(s, s + count);
    }
    template<typename CharT = value_type>
    std::enable_if_t<is_character<CharT>::value, basic_membuffer&> operator+=(std::basic_string_view<value_type> s) {
        return append(s.data(), s.size());
    }
    template<typename CharT = value_type>
    std::enable_if_t<is_character<CharT>::value, basic_membuffer&> operator+=(const_pointer s) {
        return *this += std::basic_string_view<value_type>(s);
    }
    template<typename CharT = value_type>
    std::enable_if_t<is_character<CharT>::value, basic_membuffer&> operator+=(value_type ch) {
        push_back(ch);
        return *this;
    }

 protected:
    void reset(Ty* data, size_type size, size_type capacity) noexcept {
        data_ = data, size_ = size, capacity_ = capacity;
    }

    virtual size_type try_grow(size_type /*extra*/) { return 0; }

 private:
    Ty* data_ = nullptr;
    size_type size_ = 0;
    size_type capacity_ = 0;
};

using membuffer = basic_membuffer<char>;
using wmembuffer = basic_membuffer<wchar_t>;

template<typename Ty, typename Alloc>
class basic_dynbuffer : protected std::allocator_traits<Alloc>::template rebind_alloc<Ty>, public basic_membuffer<Ty> {
 private:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<Ty>;
    using alloc_traits = std::allocator_traits<alloc_type>;

 public:
    using value_type = typename basic_membuffer<Ty>::value_type;
    using size_type = typename basic_membuffer<Ty>::size_type;
    using difference_type = typename basic_membuffer<Ty>::difference_type;
    using pointer = typename basic_membuffer<Ty>::pointer;
    using const_pointer = typename basic_membuffer<Ty>::const_pointer;
    using reference = typename basic_membuffer<Ty>::reference;
    using const_reference = typename basic_membuffer<Ty>::const_reference;
    using allocator_type = Alloc;

    basic_dynbuffer() noexcept(std::is_nothrow_default_constructible<alloc_type>::value) : alloc_type() {}
    explicit basic_dynbuffer(const Alloc& al) noexcept : alloc_type(al) {}

    ~basic_dynbuffer() override {
        if (this->capacity() & 1) { alloc_traits::deallocate(*this, this->data(), this->capacity()); }
    }

    allocator_type get_allocator() const noexcept { return allocator_type(*this); }
    size_type max_size() const noexcept { return alloc_traits::max_size(*this); }

    void reserve(size_type extra) {
        if (extra > this->avail()) { try_grow(extra); }
    }

 protected:
    basic_dynbuffer(Ty* data, size_type capacity) noexcept(std::is_nothrow_default_constructible<alloc_type>::value)
        : alloc_type(), basic_membuffer<Ty>(data, capacity) {}
    basic_dynbuffer(Ty* data, size_type capacity, const Alloc& al) noexcept
        : alloc_type(al), basic_membuffer<Ty>(data, capacity) {}

    size_type try_grow(size_type extra) override;
};

template<typename Ty, typename Alloc>
typename basic_dynbuffer<Ty, Alloc>::size_type basic_dynbuffer<Ty, Alloc>::try_grow(size_type extra) {
    const size_type sz = this->size();
    size_type delta_sz = std::max(++extra, sz >> 1);
    const size_type max_avail = std::allocator_traits<alloc_type>::max_size(*this) - sz;
    if (delta_sz > max_avail) {
        if (extra > max_avail) { throw std::length_error("too much to reserve"); }
        delta_sz = std::max(extra, max_avail >> 1);
    }
    const size_type capacity = ((sz + delta_sz - 1) & ~size_type(1)) + 1;  // Make new dynamic odd capacity
    Ty* data = alloc_traits::allocate(*this, capacity);
    std::copy(this->data(), this->endp(), data);
    if (this->capacity() & 1) { alloc_traits::deallocate(*this, this->data(), this->capacity()); }
    this->reset(data, this->size(), capacity);
    return this->avail();
}

template<typename Ty, std::size_t InlineBufSize = 0, typename Alloc = std::allocator<Ty>>
class inline_basic_dynbuffer final : public basic_dynbuffer<Ty, Alloc> {
 public:
    inline_basic_dynbuffer() noexcept(std::is_nothrow_default_constructible<basic_dynbuffer<Ty, Alloc>>::value)
        : basic_dynbuffer<Ty, Alloc>(reinterpret_cast<Ty*>(buf_), inline_buf_size) {}
    explicit inline_basic_dynbuffer(const Alloc& al) noexcept
        : basic_dynbuffer<Ty, Alloc>(reinterpret_cast<Ty*>(buf_), inline_buf_size, al) {}

 private:
    enum : unsigned {  // Always even
#if UXS_DEBUG_REDUCED_BUFFERS != 0
        inline_buf_size = 8,
#else   // UXS_DEBUG_REDUCED_BUFFERS != 0
        inline_buf_size = ((InlineBufSize != 0 ? InlineBufSize : 256 / sizeof(Ty)) + 1) & ~1U,
#endif  // UXS_DEBUG_REDUCED_BUFFERS != 0
    };

    alignas(std::alignment_of<Ty>::value) std::uint8_t buf_[inline_buf_size * sizeof(Ty)];
};

using inline_dynbuffer = inline_basic_dynbuffer<char>;
using inline_wdynbuffer = inline_basic_dynbuffer<wchar_t>;

template<typename Ty>
class basic_membuffer_with_size_tracker final : public basic_membuffer<Ty> {
 public:
    using size_type = typename basic_membuffer<Ty>::size_type;

    basic_membuffer_with_size_tracker(Ty* data, size_type capacity) noexcept
        : basic_membuffer<Ty>(data, capacity), tracked_size_(capacity) {}
    size_type tracked_size() const noexcept { return this->avail() ? this->size() : tracked_size_; }

 private:
    size_type tracked_size_;

    size_type try_grow(size_type extra) override {
        tracked_size_ += extra;
        return 0;
    }
};

using membuffer_with_size_tracker = basic_membuffer_with_size_tracker<char>;
using wmembuffer_with_size_tracker = basic_membuffer_with_size_tracker<wchar_t>;

}  // namespace uxs
