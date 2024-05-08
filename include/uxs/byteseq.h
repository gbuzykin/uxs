#pragma once

#include "span.h"

#include <vector>

namespace uxs {

class byteseqdev;

namespace detail {
struct byteseq_chunk_t {
    using alloc_type = std::allocator<byteseq_chunk_t>;
    std::size_t size() const { return static_cast<std::size_t>(end - data); }
    std::size_t capacity() const { return static_cast<std::size_t>(boundary - data); }
    std::size_t avail() const { return static_cast<std::size_t>(boundary - end); }
    static std::size_t get_alloc_sz(std::size_t cap) {
        return (offsetof(byteseq_chunk_t, data) + cap + sizeof(byteseq_chunk_t) - 1) / sizeof(byteseq_chunk_t);
    }
    static std::size_t max_size(const alloc_type& al) {
        return (std::allocator_traits<alloc_type>::max_size(al) * sizeof(byteseq_chunk_t) -
                offsetof(byteseq_chunk_t, data));
    }
    static void dealloc(alloc_type& al, byteseq_chunk_t* chunk) {
        al.deallocate(chunk, get_alloc_sz(chunk->capacity()));
    }
    static byteseq_chunk_t* alloc(alloc_type& al, std::size_t cap);
    byteseq_chunk_t* next;
    byteseq_chunk_t* prev;
    std::uint8_t* end;
    std::uint8_t* boundary;
    alignas(std::alignment_of<max_align_t>::value) std::uint8_t data[1];
};
}  // namespace detail

class byteseq : public detail::byteseq_chunk_t::alloc_type {
 public:
    byteseq() noexcept = default;
    byteseq(const byteseq& other) { assign(other); }
    byteseq(byteseq&& other) noexcept : head_(other.head_), size_(other.size_) {
        other.head_ = nullptr, other.size_ = 0;
    }
    UXS_EXPORT ~byteseq();

    byteseq& operator=(const byteseq& other) { return &other != this ? assign(other) : *this; }
    byteseq& operator=(byteseq&& other) noexcept {
        if (&other != this) {
            head_ = other.head_, size_ = other.size_;
            other.head_ = nullptr, other.size_ = 0;
        }
        return *this;
    }

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    void clear() noexcept;
    UXS_EXPORT std::uint32_t calc_crc32() const noexcept;

    void swap(byteseq& other) {
        std::swap(head_, other.head_);
        std::swap(size_, other.size_);
    }

    template<typename FillFunc>
    byteseq& assign(std::size_t max_size, FillFunc func) {
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

    UXS_EXPORT byteseq& assign(const byteseq& other);
    UXS_NODISCARD UXS_EXPORT std::vector<std::uint8_t> make_vector() const;
    UXS_EXPORT static byteseq from_vector(uxs::span<const std::uint8_t> v);

    UXS_NODISCARD UXS_EXPORT byteseq make_compressed() const;
    UXS_NODISCARD UXS_EXPORT byteseq make_uncompressed() const;
    UXS_EXPORT bool compress();
    UXS_EXPORT bool uncompress();

 private:
    using chunk_t = detail::byteseq_chunk_t;

    friend class byteseqdev;

    enum : std::size_t { chunk_size = 0x100000, max_avail_count = 0x40000000 };

    chunk_t* head_ = nullptr;
    std::size_t size_ = 0;

    UXS_EXPORT void delete_chunks() noexcept;
    UXS_EXPORT void clear_and_reserve(std::size_t cap);
    UXS_EXPORT void create_head(std::size_t cap);
    UXS_EXPORT void create_head_chunk();
    UXS_EXPORT void create_next_chunk();
};

}  // namespace uxs
