#pragma once

#include "span.h"
#include "stringcvt.h"

#include <iterator>
#include <vector>

namespace uxs {

class byteseqdev;

enum class byteseq_flags : unsigned { none = 0, compressed = 1 };
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(byteseq_flags, unsigned);

class UXS_EXPORT byteseq {
 public:
    byteseq() = default;
    explicit byteseq(byteseq_flags flags) : flags_(flags) {}
    byteseq(const byteseq& other) { assign(other); }
    byteseq(byteseq&& other) NOEXCEPT : head_(other.head_), size_(other.size_), flags_(other.flags_) {
        other.head_ = nullptr, other.size_ = 0;
    }
    ~byteseq();

    byteseq& operator=(const byteseq& other) {
        if (&other != this) { assign(other); }
        return *this;
    }
    byteseq& operator=(byteseq&& other) NOEXCEPT {
        if (&other != this) {
            head_ = other.head_, size_ = other.size_, flags_ = other.flags_;
            other.head_ = nullptr, other.size_ = 0;
        }
        return *this;
    }

    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    byteseq_flags flags() const { return flags_; }
    bool compressed() const { return !!(flags_ & byteseq_flags::compressed); }
    uint32_t calc_crc32() const;

    void clear();
    void assign(const byteseq& other);

    void swap(byteseq& other) {
        std::swap(head_, other.head_);
        std::swap(size_, other.size_);
        std::swap(flags_, other.flags_);
    }

    std::vector<uint8_t> make_vector() const;
    static byteseq from_vector(uxs::span<const uint8_t> v, byteseq_flags flags);

    byteseq make_compressed() const;
    byteseq make_uncompressed() const;
    bool compress();
    bool uncompress();

    template<typename CharT, typename Traits>
    void from_string(std::basic_string_view<CharT, Traits> s);
    template<typename StrTy>
    StrTy& to_string(StrTy& s, const fmt_opts& fmt) const;

 private:
    friend class byteseqdev;

    enum : size_t { chunk_size = 0x100000, max_avail_count = 0x40000000 };

    struct chunk_t {
        using alloc_type = std::allocator<chunk_t>;
        size_t size() { return static_cast<size_t>(end - data); }
        size_t capacity() { return static_cast<size_t>(boundary - data); }
        size_t avail() { return static_cast<size_t>(boundary - end); }
        static size_t get_alloc_sz(size_t cap) {
            return (offsetof(chunk_t, data) + cap + sizeof(chunk_t) - 1) / sizeof(chunk_t);
        }
        static size_t max_size(const alloc_type& al) {
            return (std::allocator_traits<alloc_type>::max_size(al) * sizeof(chunk_t) - offsetof(chunk_t, data));
        }
        static void dealloc(alloc_type& al, chunk_t* chunk) { al.deallocate(chunk, get_alloc_sz(chunk->capacity())); }
        static chunk_t* alloc(alloc_type& al, size_t cap);
        chunk_t* next;
        chunk_t* prev;
        uint8_t* end;
        uint8_t* boundary;
        uint8_t data[1];
    };

    chunk_t* head_ = nullptr;
    size_t size_ = 0;
    byteseq_flags flags_ = byteseq_flags::none;

    void clear_and_reserve(size_t cap);
    void create_head_chunk();
    void create_next_chunk();
    void create_head_checked(size_t cap);
};

template<typename CharT, typename Traits>
void byteseq::from_string(std::basic_string_view<CharT, Traits> s) {
    size_t sz = s.size() / 2;
    clear_and_reserve(sz);
    if (head_) {
        auto it = s.begin();
        for (uint8_t *p = head_->data, *p_end = p + sz; p != p_end; ++p, it += 2) {
            *p = static_cast<uint8_t>(from_hex(it, 2));
        }
        head_->end = head_->data + sz;
    }
    size_ = sz;
}

template<typename StrTy>
StrTy& byteseq::to_string(StrTy& s, const fmt_opts& fmt) const {
    size_t sz = 2 * size();
    if (!sz) { return s; }
    chunk_t* chunk = head_->next;
    do {
        for (uint8_t* p = chunk->data; p != chunk->end; ++p) {
            to_hex(static_cast<uint8_t>(*p), std::back_inserter(s), 2);
        }
        chunk = chunk->next;
    } while (chunk != head_->next);
    return s;
}

}  // namespace uxs

namespace uxs {

template<typename CharT>
struct string_parser<byteseq, CharT> {
    const CharT* from_chars(const CharT* first, const CharT* last, byteseq& val) const {
        val.from_string(std::basic_string_view<CharT>{first, static_cast<size_t>(last - first)});
        return last;
    }
};
template<typename CharT>
struct formatter<byteseq, CharT> {
    template<typename StrTy>
    void format(StrTy& s, const byteseq& val, const fmt_opts& fmt) const {
        val.to_string(s, fmt);
    }
};

}  // namespace uxs
