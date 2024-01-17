#pragma once

#include "span.h"

#include <vector>

namespace uxs {

class byteseqdev;

enum class byteseq_flags : unsigned { none = 0, compressed = 1 };
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(byteseq_flags, unsigned);

namespace detail {
struct byteseq_chunk_t {
    using alloc_type = std::allocator<byteseq_chunk_t>;
    size_t size() const { return static_cast<size_t>(end - data); }
    size_t capacity() const { return static_cast<size_t>(boundary - data); }
    size_t avail() const { return static_cast<size_t>(boundary - end); }
    static size_t get_alloc_sz(size_t cap) {
        return (offsetof(byteseq_chunk_t, data) + cap + sizeof(byteseq_chunk_t) - 1) / sizeof(byteseq_chunk_t);
    }
    static size_t max_size(const alloc_type& al) {
        return (std::allocator_traits<alloc_type>::max_size(al) * sizeof(byteseq_chunk_t) -
                offsetof(byteseq_chunk_t, data));
    }
    static void dealloc(alloc_type& al, byteseq_chunk_t* chunk) {
        al.deallocate(chunk, get_alloc_sz(chunk->capacity()));
    }
    static byteseq_chunk_t* alloc(alloc_type& al, size_t cap);
    byteseq_chunk_t* next;
    byteseq_chunk_t* prev;
    uint8_t* end;
    uint8_t* boundary;
    alignas(std::alignment_of<max_align_t>::value) uint8_t data[1];
};
}  // namespace detail

class byteseq : public detail::byteseq_chunk_t::alloc_type {
 public:
    byteseq() = default;
    explicit byteseq(byteseq_flags flags) : flags_(flags) {}
    byteseq(const byteseq& other) { assign(other); }
    byteseq(byteseq&& other) noexcept : flags_(other.flags_), head_(other.head_), size_(other.size_) {
        other.head_ = nullptr, other.size_ = 0;
    }
    UXS_EXPORT ~byteseq();

    byteseq& operator=(const byteseq& other) { return &other != this ? assign(other) : *this; }
    byteseq& operator=(byteseq&& other) noexcept {
        if (&other != this) {
            flags_ = other.flags_, head_ = other.head_, size_ = other.size_;
            other.head_ = nullptr, other.size_ = 0;
        }
        return *this;
    }

    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    byteseq_flags flags() const { return flags_; }
    bool compressed() const { return !!(flags_ & byteseq_flags::compressed); }
    void clear() { clear_and_reserve(0); }
    UXS_EXPORT uint32_t calc_crc32() const;

    void swap(byteseq& other) {
        std::swap(flags_, other.flags_);
        std::swap(head_, other.head_);
        std::swap(size_, other.size_);
    }

    template<typename FillFunc>
    byteseq& assign(size_t max_size, byteseq_flags flags, FillFunc func) {
        flags_ = flags;
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
    UXS_EXPORT std::vector<uint8_t> make_vector() const;
    UXS_EXPORT static byteseq from_vector(uxs::span<const uint8_t> v, byteseq_flags flags);

    UXS_EXPORT byteseq make_compressed() const;
    UXS_EXPORT byteseq make_uncompressed() const;
    UXS_EXPORT bool compress();
    UXS_EXPORT bool uncompress();

 private:
    using chunk_t = detail::byteseq_chunk_t;

    friend class byteseqdev;

    enum : size_t { chunk_size = 0x100000, max_avail_count = 0x40000000 };

    byteseq_flags flags_ = byteseq_flags::none;
    chunk_t* head_ = nullptr;
    size_t size_ = 0;

    UXS_EXPORT void clear_and_reserve(size_t cap);
    UXS_EXPORT void create_head(size_t cap);
    UXS_EXPORT void create_head_chunk();
    UXS_EXPORT void create_next_chunk();
};

}  // namespace uxs
