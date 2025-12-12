#pragma once

#include "span.h"

#include <memory>
#include <vector>

namespace uxs {

template<typename Alloc>
class basic_byteseqdev;

namespace detail {
template<typename Alloc>
struct byteseq_chunk {
    byteseq_chunk* next;
    byteseq_chunk* prev;
    std::uint8_t* end;
    std::uint8_t* boundary;
    alignas(std::alignment_of<max_align_t>::value) std::uint8_t data[1];

    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<byteseq_chunk>;

    std::size_t size() const noexcept { return static_cast<std::size_t>(end - data); }
    std::size_t capacity() const noexcept { return static_cast<std::size_t>(boundary - data); }
    std::size_t avail() const noexcept { return static_cast<std::size_t>(boundary - end); }
    static std::size_t get_alloc_sz(std::size_t cap) noexcept {
        return (offsetof(byteseq_chunk, data) + cap + sizeof(byteseq_chunk) - 1) / sizeof(byteseq_chunk);
    }
    static std::size_t max_size(const alloc_type& al) noexcept {
        return (std::allocator_traits<alloc_type>::max_size(al) * sizeof(byteseq_chunk) - offsetof(byteseq_chunk, data));
    }
    static byteseq_chunk* alloc(alloc_type& al, std::size_t cap);
    static void dealloc(alloc_type& al, byteseq_chunk* chunk) noexcept {
        al.deallocate(chunk, get_alloc_sz(chunk->capacity()));
    }
};
}  // namespace detail

template<typename Alloc>
class basic_byteseq : protected detail::byteseq_chunk<Alloc>::alloc_type {
 private:
    using chunk_t = detail::byteseq_chunk<Alloc>;
    using alloc_type = typename chunk_t::alloc_type;

 public:
    using allocator_type = Alloc;

    basic_byteseq() noexcept(std::is_nothrow_default_constructible<alloc_type>::value) : alloc_type() {}
    explicit basic_byteseq(const Alloc& al) noexcept : alloc_type(al) {}
    basic_byteseq(const basic_byteseq& other) : alloc_type(other) { assign(other); }
    basic_byteseq(basic_byteseq&& other) noexcept
        : alloc_type(std::move(other)), size_(other.size_), head_(other.head_) {
        other.size_ = 0, other.head_ = nullptr;
    }
    ~basic_byteseq() {
        if (head_) { tidy(); }
    }

    basic_byteseq& operator=(const basic_byteseq& other) {
        if (&other == this) { return *this; }
        assign(other);
        return *this;
    }

    basic_byteseq& operator=(basic_byteseq&& other) noexcept {
        if (&other == this) { return *this; }
        if (head_) { tidy(); }
        alloc_type::operator=(std::move(other));
        size_ = other.size_, head_ = other.head_;
        other.size_ = 0, other.head_ = nullptr;
        return *this;
    }

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    allocator_type get_allocator() const noexcept { return allocator_type(*this); }

    UXS_EXPORT void clear() noexcept;
    UXS_EXPORT std::uint32_t calc_crc32() const noexcept;

    void swap(basic_byteseq& other) noexcept {
        std::swap(size_, other.size_);
        std::swap(head_, other.head_);
    }

    template<typename FillFunc>
    basic_byteseq& assign(std::size_t max_size, FillFunc func) {
        clear_and_reserve(max_size);
        if (head_) {
            size_ = func(head_->data, max_size);
            head_->end = head_->data + size_;
        }
        return *this;
    }

    template<typename ScanFunc>
    void scan(ScanFunc func) const {
        if (!size_) { return; }
        const chunk_t* chunk = head_->next;
        do {
            func(chunk->data, chunk->size());
            chunk = chunk->next;
        } while (chunk != head_->next);
    }

    UXS_EXPORT basic_byteseq& assign(const basic_byteseq& other);
    UXS_NODISCARD UXS_EXPORT std::vector<std::uint8_t> make_vector() const;
    UXS_EXPORT static basic_byteseq from_vector(est::span<const std::uint8_t> v);

    UXS_EXPORT void resize(std::size_t sz);
    UXS_NODISCARD UXS_EXPORT basic_byteseq make_compressed(unsigned level = 0) const;
    UXS_NODISCARD UXS_EXPORT basic_byteseq make_uncompressed() const;
    UXS_EXPORT bool compress(unsigned level = 0);
    UXS_EXPORT bool uncompress();

 private:
    friend class basic_byteseqdev<Alloc>;

    enum : std::size_t { chunk_size = 0x100000, max_avail_count = 0x40000000 };

    std::size_t size_ = 0;
    chunk_t* head_ = nullptr;

    UXS_EXPORT void tidy() noexcept;
    UXS_EXPORT void delete_chunks() noexcept;
    UXS_EXPORT void clear_and_reserve(std::size_t cap);
    UXS_EXPORT void create_head(std::size_t cap);
    UXS_EXPORT void create_head_chunk();
    UXS_EXPORT void create_next_chunk();
};

using byteseq = basic_byteseq<std::allocator<std::uint8_t>>;

}  // namespace uxs
