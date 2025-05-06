#pragma once

#include "uxs/byteseq.h"
#include "uxs/crc32.h"
#include "uxs/dllist.h"

#if defined(UXS_USE_ZLIB)
#    define ZLIB_CONST
#    include <zlib.h>
#endif  // defined(UXS_USE_ZLIB)

#include <algorithm>
#include <cassert>
#include <cstring>
#include <stdexcept>

namespace uxs {

template<typename Alloc>
basic_byteseq<Alloc>::~basic_byteseq() {
    if (!head_) { return; }
    delete_chunks();
    chunk_t::dealloc(*this, head_);
}

template<typename Alloc>
void basic_byteseq<Alloc>::clear() noexcept {
    if (!head_) { return; }
    delete_chunks();
    size_ = 0;
    dllist_make_cycle(head_);
    head_->end = head_->data;
}

template<typename Alloc>
std::uint32_t basic_byteseq<Alloc>::calc_crc32() const noexcept {
    std::uint32_t crc32 = 0xffffffff;
    scan([&crc32](const std::uint8_t* p, std::size_t sz) { crc32 = crc32_calc{}(p, p + sz, crc32); });
    return crc32;
}

template<typename Alloc>
basic_byteseq<Alloc>& basic_byteseq<Alloc>::assign(const basic_byteseq& other) {
    return assign(other.size_, [&other](std::uint8_t* dst, std::size_t dst_sz) {
        other.scan([&dst](const std::uint8_t* p, std::size_t sz) {
            std::memcpy(dst, p, sz);
            dst += sz;
        });
        return dst_sz;
    });
}

template<typename Alloc>
std::vector<std::uint8_t> basic_byteseq<Alloc>::make_vector() const {
    std::vector<std::uint8_t> result;
    scan([&result](const std::uint8_t* p, std::size_t sz) { result.insert(result.end(), p, p + sz); });
    return result;
}

template<typename Alloc>
/*static*/ basic_byteseq<Alloc> basic_byteseq<Alloc>::from_vector(est::span<const std::uint8_t> v) {
    basic_byteseq seq;
    return seq.assign(v.size(), [&v](std::uint8_t* dst, std::size_t dst_sz) {
        std::memcpy(dst, v.data(), dst_sz);
        return dst_sz;
    });
    return seq;
}

template<typename Alloc>
void basic_byteseq<Alloc>::resize(std::size_t sz) {
    if (sz > size_) {
        if (!head_) { create_head_chunk(); }
        while (sz - size_ > head_->avail()) {
            std::memset(head_->end, 0, head_->avail());
            create_next_chunk();
        }
        const std::size_t extra = sz - size_;
        std::memset(head_->end, 0, extra);
        head_->end += extra;
    } else {
        while (size_ - sz > head_->size()) {
            chunk_t* prev = head_->prev;
            size_ -= head_->size();
            dllist_remove(head_);
            chunk_t::dealloc(*this, head_);
            head_ = prev;
        }
        head_->end -= size_ - sz;
    }
    size_ = sz;
}

template<typename Alloc>
bool basic_byteseq<Alloc>::compress(unsigned level) {
    if (empty()) { return true; }
    auto seq = make_compressed(level);
    if (seq.empty()) { return false; }
    *this = std::move(seq);
    return true;
}

template<typename Alloc>
bool basic_byteseq<Alloc>::uncompress() {
    if (empty()) { return true; }
    auto seq = make_uncompressed();
    if (seq.empty()) { return false; }
    *this = std::move(seq);
    return true;
}

#if defined(UXS_USE_ZLIB)
template<typename Alloc>
basic_byteseq<Alloc> basic_byteseq<Alloc>::make_compressed(unsigned level) const {
    if (empty()) { return {}; }

    basic_byteseq seq;
    seq.create_head_chunk();

    z_stream zstr;
    std::memset(&zstr, 0, sizeof(z_stream));
    ::deflateInit(&zstr, level > 0 ? static_cast<int>(level) : Z_DEFAULT_COMPRESSION);

    try {
        const chunk_t* chunk = head_->next;
        zstr.next_in = chunk->data;
        zstr.next_out = seq.head_->data;

        while (true) {
            if (zstr.next_in == chunk->end && chunk != head_) {
                chunk = chunk->next;
                zstr.next_in = chunk->data;
            }

            zstr.avail_in = static_cast<uInt>(std::min<std::size_t>(chunk->end - zstr.next_in, max_avail_count));
            zstr.avail_out = static_cast<uInt>(seq.head_->boundary - zstr.next_out);

            const int ret = ::deflate(&zstr, zstr.avail_in ? Z_NO_FLUSH : Z_FINISH);
            if (ret != Z_OK) {
                if (::deflateEnd(&zstr) == Z_OK && ret == Z_STREAM_END) {
                    seq.head_->end = zstr.next_out;
                    seq.size_ += seq.head_->size();
                    return seq;
                }
                break;
            }

            if (zstr.next_out == seq.head_->boundary) {
                seq.create_next_chunk();
                zstr.next_out = seq.head_->data;
            }
        }
    } catch (...) {
        ::deflateEnd(&zstr);
        throw;
    }

    return {};
}

template<typename Alloc>
basic_byteseq<Alloc> basic_byteseq<Alloc>::make_uncompressed() const {
    if (empty()) { return {}; }

    basic_byteseq seq;
    seq.create_head_chunk();

    z_stream zstr;
    std::memset(&zstr, 0, sizeof(z_stream));
    ::inflateInit(&zstr);

    try {
        const chunk_t* chunk = head_->next;
        zstr.next_in = chunk->data;
        zstr.next_out = seq.head_->data;

        while (true) {
            if (zstr.next_in == chunk->end && chunk != head_) {
                chunk = chunk->next;
                zstr.next_in = chunk->data;
            }

            zstr.avail_in = static_cast<uInt>(std::min<std::size_t>(chunk->end - zstr.next_in, max_avail_count));
            zstr.avail_out = static_cast<uInt>(seq.head_->boundary - zstr.next_out);

            const int ret = ::inflate(&zstr, zstr.avail_in ? Z_NO_FLUSH : Z_FINISH);
            if (ret != Z_OK) {
                if (::inflateEnd(&zstr) == Z_OK && ret == Z_STREAM_END) {
                    seq.head_->end = zstr.next_out;
                    seq.size_ += seq.head_->size();
                    return seq;
                }
                break;
            }

            if (zstr.next_out == seq.head_->boundary) {
                seq.create_next_chunk();
                zstr.next_out = seq.head_->data;
            }
        }
    } catch (...) {
        ::deflateEnd(&zstr);
        throw;
    }

    return {};
}
#else   // defined(UXS_USE_ZLIB)
template<typename Alloc>
basic_byteseq<Alloc> basic_byteseq<Alloc>::make_compressed(unsigned /*level*/) const {
    return *this;
}
template<typename Alloc>
basic_byteseq<Alloc> basic_byteseq<Alloc>::make_uncompressed() const {
    return *this;
}
#endif  // defined(UXS_USE_ZLIB)

template<typename Alloc>
void basic_byteseq<Alloc>::delete_chunks() noexcept {
    chunk_t* chunk = head_->next;
    while (chunk != head_) {
        chunk_t* next = chunk->next;
        chunk_t::dealloc(*this, chunk);
        chunk = next;
    }
}

template<typename Alloc>
void basic_byteseq<Alloc>::clear_and_reserve(std::size_t cap) {
    if (head_) {
        delete_chunks();
        size_ = 0;
        // delete chunks excepts of the last
        if (head_->capacity() < cap) {
            // create new head buffer
            chunk_t::dealloc(*this, head_);
            head_ = nullptr;
            create_head(cap);
        } else {  // reuse head buffer
            dllist_make_cycle(head_);
            head_->end = head_->data;
        }
    } else if (cap) {
        create_head(cap);
    }
}

template<typename Alloc>
/*static*/ detail::byteseq_chunk<Alloc>* detail::byteseq_chunk<Alloc>::alloc(alloc_type& al, std::size_t cap) {
    const std::size_t alloc_sz = get_alloc_sz(cap);
    byteseq_chunk* chunk = al.allocate(alloc_sz);
    chunk->boundary = chunk->data + alloc_sz * sizeof(byteseq_chunk) - offsetof(byteseq_chunk, data);
    assert(chunk->capacity() >= cap && get_alloc_sz(chunk->capacity()) == alloc_sz);
    return chunk;
}

template<typename Alloc>
void basic_byteseq<Alloc>::create_head(std::size_t cap) {
    if (cap > chunk_t::max_size(*this)) { throw std::length_error("too much to reserve"); }
    head_ = chunk_t::alloc(*this, cap);
    dllist_make_cycle(head_);
    head_->end = head_->data;
}

template<typename Alloc>
void basic_byteseq<Alloc>::create_head_chunk() {
    head_ = chunk_t::alloc(*this, chunk_size);
    dllist_make_cycle(head_);
    head_->end = head_->data;
}

template<typename Alloc>
void basic_byteseq<Alloc>::create_next_chunk() {
    chunk_t* chunk = chunk_t::alloc(*this, chunk_size);
    dllist_insert_after(head_, chunk);
    chunk->end = chunk->data;
    size_ += head_->avail();
    head_->end = head_->boundary;
    head_ = chunk;
}

}  // namespace uxs
