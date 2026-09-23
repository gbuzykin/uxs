#pragma once

#include "iterator.h"
#include "string_view.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace uxs {

template<typename CharT>
class basic_membuffer {
 private:
    static_assert(std::is_same<std::remove_cv_t<CharT>, CharT>::value,
                  "uxs::basic_membuffer<> must have a non-const, non-volatile value type");
    static_assert(std::is_integral<CharT>::value, "uxs::basic_membuffer<> defined for integral types");

 public:
    using value_type = CharT;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using reference = value_type&;
    using const_reference = value_type;
    using const_iterator = est::array_iterator<basic_membuffer, const_pointer, true>;
    using iterator = const_iterator;

 protected:
    using try_grow_impl_t = size_type (*)(basic_membuffer& buf, size_type extra, bool track_size);

 public:
    explicit basic_membuffer(pointer data) noexcept : data_(data), capacity_(std::numeric_limits<size_type>::max()) {}
    basic_membuffer(pointer data, size_type capacity, try_grow_impl_t try_grow_impl) noexcept
        : data_(data), capacity_(capacity), try_grow_impl_(try_grow_impl) {}
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

    template<typename InputIt, typename = std::enable_if_t<est::is_random_access_iterator<InputIt>::value &&
                                                           std::is_same<est::iterator_value_t<InputIt>, CharT>::value>>
    basic_membuffer& append(InputIt first, InputIt last) {
        assert(first <= last);
        size_type count = static_cast<size_type>(last - first);
        size_type n_avail = avail();
        while (count > n_avail) {
            std::copy_n(first, n_avail, endp());
            first += n_avail;
            count -= n_avail, size_ += n_avail;
            if (!(n_avail = try_grow_impl_(*this, count, true))) { return *this; }
        }
        std::copy(first, last, endp());
        size_ += count;
        return *this;
    }

    basic_membuffer& append(size_type count, value_type val) {
        size_type n_avail = avail();
        while (count > n_avail) {
            std::fill_n(endp(), n_avail, val);
            count -= n_avail, size_ += n_avail;
            if (!(n_avail = try_grow_impl_(*this, count, true))) { return *this; }
        }
        std::fill_n(endp(), count, val);
        size_ += count;
        return *this;
    }

    void push_back(value_type val) {
        if (size_ == capacity_ && !try_grow_impl_(*this, 1, true)) { return; }
        data_[size_++] = val;
    }

    void pop_back() noexcept {
        assert(size_ != 0);
        --size_;
    }

    template<typename CharT_ = value_type>
    std::enable_if_t<est::is_character<CharT_>::value, basic_membuffer&> append(const_pointer s, size_type count) {
        return append(s, s + count);
    }
    template<typename CharT_ = value_type>
    std::enable_if_t<est::is_character<CharT_>::value, basic_membuffer&> operator+=(
        std::basic_string_view<value_type> s) {
        return append(s.data(), s.size());
    }
    template<typename CharT_ = value_type>
    std::enable_if_t<est::is_character<CharT_>::value, basic_membuffer&> operator+=(const_pointer s) {
        return *this += to_string_view(s);
    }
    template<typename CharT_ = value_type>
    std::enable_if_t<est::is_character<CharT_>::value, basic_membuffer&> operator+=(value_type ch) {
        push_back(ch);
        return *this;
    }

    size_type try_grow(size_type extra) { return try_grow_impl_(*this, extra, false); }

 protected:
    basic_membuffer(pointer data, size_type size, size_type capacity, try_grow_impl_t try_grow_impl) noexcept
        : data_(data), size_(size), capacity_(capacity), try_grow_impl_(try_grow_impl) {}

    void reset(CharT* data, size_type size, size_type capacity) noexcept {
        data_ = data, size_ = size, capacity_ = capacity;
    }

 private:
    CharT* data_ = nullptr;
    size_type size_ = 0;
    size_type capacity_ = 0;

    try_grow_impl_t try_grow_impl_ = nullptr;
};

using membuffer = basic_membuffer<char>;
using wmembuffer = basic_membuffer<wchar_t>;

template<typename CharT, typename Alloc = std::allocator<CharT>>
class basic_dynbuffer : protected std::allocator_traits<Alloc>::template rebind_alloc<CharT>,
                        public basic_membuffer<CharT> {
 private:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<CharT>;
    using alloc_traits = std::allocator_traits<alloc_type>;

 public:
    using value_type = typename basic_membuffer<CharT>::value_type;
    using size_type = typename basic_membuffer<CharT>::size_type;
    using difference_type = typename basic_membuffer<CharT>::difference_type;
    using pointer = typename basic_membuffer<CharT>::pointer;
    using const_pointer = typename basic_membuffer<CharT>::const_pointer;
    using reference = typename basic_membuffer<CharT>::reference;
    using const_reference = typename basic_membuffer<CharT>::const_reference;
    using allocator_type = Alloc;

    basic_dynbuffer() noexcept(std::is_nothrow_default_constructible<alloc_type>::value)
        : alloc_type(), basic_membuffer<CharT>(nullptr, 0, try_grow_impl) {}
    explicit basic_dynbuffer(const Alloc& al) noexcept
        : alloc_type(al), basic_membuffer<CharT>(nullptr, 0, try_grow_impl) {}
    ~basic_dynbuffer() {
        if (this->capacity() & 1) { alloc_traits::deallocate(*this, this->data(), this->capacity()); }
    }

    allocator_type get_allocator() const noexcept { return allocator_type(*this); }
    size_type max_size() const noexcept { return alloc_traits::max_size(*this); }

    void reserve(size_type size) {
        if (size > this->capacity()) { this->try_grow(size - this->size()); }
    }

 protected:
    basic_dynbuffer(CharT* data, size_type capacity) noexcept(std::is_nothrow_default_constructible<alloc_type>::value)
        : alloc_type(), basic_membuffer<CharT>(data, capacity, try_grow_impl) {}
    basic_dynbuffer(CharT* data, size_type capacity, const Alloc& al) noexcept
        : alloc_type(al), basic_membuffer<CharT>(data, capacity, try_grow_impl) {}

    void reset(CharT* data, size_type size, size_type capacity) noexcept {
        basic_membuffer<CharT>::reset(data, size, capacity);
    }

    static size_type try_grow_impl(basic_membuffer<CharT>& buf, size_type extra, bool /*track_size*/) {
        const size_type size = buf.size();
        size_type delta_sz = std::max(++extra, size >> 1);
        auto& dynbuf = static_cast<basic_dynbuffer&>(buf);
        const size_type max_avail = std::allocator_traits<alloc_type>::max_size(dynbuf) - size;
        if (delta_sz > max_avail) {
            if (extra > max_avail) { throw std::length_error("too much to reserve"); }
            delta_sz = std::max(extra, max_avail >> 1);
        }
        const size_type capacity = ((size + delta_sz - 1) & ~size_type(1)) + 1;  // Make new dynamic odd capacity
        CharT* data = alloc_traits::allocate(dynbuf, capacity);
        std::memcpy(data, buf.data(), size * sizeof(CharT));
        if (buf.capacity() & 1) { alloc_traits::deallocate(dynbuf, buf.data(), buf.capacity()); }
        dynbuf.reset(data, buf.size(), capacity);
        return buf.avail();
    }
};

template<typename CharT, std::size_t InlineBufSize = 0, typename Alloc = std::allocator<CharT>>
class basic_inline_dynbuffer : public basic_dynbuffer<CharT, Alloc> {
 public:
    basic_inline_dynbuffer() noexcept(std::is_nothrow_default_constructible<basic_dynbuffer<CharT, Alloc>>::value)
        : basic_dynbuffer<CharT, Alloc>(reinterpret_cast<CharT*>(buf_), inline_buf_size) {}
    explicit basic_inline_dynbuffer(const Alloc& al) noexcept
        : basic_dynbuffer<CharT, Alloc>(reinterpret_cast<CharT*>(buf_), inline_buf_size, al) {}

 private:
    enum : unsigned {  // Always even
#if UXS_DEBUG_REDUCED_BUFFERS != 0
        inline_buf_size = 8,
#else   // UXS_DEBUG_REDUCED_BUFFERS != 0
        inline_buf_size = ((InlineBufSize != 0 ? InlineBufSize : 256 / sizeof(CharT)) + 1) & ~1U,
#endif  // UXS_DEBUG_REDUCED_BUFFERS != 0
    };

    alignas(std::alignment_of<CharT>::value) std::uint8_t buf_[inline_buf_size * sizeof(CharT)];
};

using inline_dynbuffer = basic_inline_dynbuffer<char>;
using inline_wdynbuffer = basic_inline_dynbuffer<wchar_t>;

template<typename CharT>
class basic_membuffer_with_size_tracker : public basic_membuffer<CharT> {
 public:
    using size_type = typename basic_membuffer<CharT>::size_type;

    basic_membuffer_with_size_tracker(CharT* data, size_type capacity) noexcept
        : basic_membuffer<CharT>(data, capacity, try_grow_impl), tracked_size_(capacity) {}
    size_type tracked_size() const noexcept { return this->avail() ? this->size() : tracked_size_; }

 private:
    size_type tracked_size_;

    static size_type try_grow_impl(basic_membuffer<CharT>& buf, size_type extra, bool track_size) {
        if (track_size) { static_cast<basic_membuffer_with_size_tracker&>(buf).tracked_size_ += extra; }
        return 0;
    }
};

using membuffer_with_size_tracker = basic_membuffer_with_size_tracker<char>;
using wmembuffer_with_size_tracker = basic_membuffer_with_size_tracker<wchar_t>;

}  // namespace uxs
