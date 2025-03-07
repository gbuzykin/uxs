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

using namespace uxs;

//---------------------------------------------------------------------------------
// byteseq class implementation

byteseq::~byteseq() {
    if (!head_) { return; }
    delete_chunks();
    chunk_t::dealloc(*this, head_);
}

void byteseq::clear() noexcept {
    if (!head_) { return; }
    delete_chunks();
    size_ = 0;
    dllist_make_cycle(head_);
    head_->end = head_->data;
}

std::uint32_t byteseq::calc_crc32() const noexcept {
    std::uint32_t crc32 = 0xffffffff;
    scan([&crc32](const std::uint8_t* p, std::size_t sz) { crc32 = crc32_calc{}(p, p + sz, crc32); });
    return crc32;
}

byteseq& byteseq::assign(const byteseq& other) {
    return assign(other.size_, [&other](std::uint8_t* dst, std::size_t dst_sz) {
        other.scan([&dst](const std::uint8_t* p, std::size_t sz) {
            std::memcpy(dst, p, sz);
            dst += sz;
        });
        return dst_sz;
    });
}

bool byteseq::compress() {
    if (empty()) { return true; }
    auto seq = make_compressed();
    if (seq.empty()) { return false; }
    *this = std::move(seq);
    return true;
}

bool byteseq::uncompress() {
    if (empty()) { return true; }
    auto seq = make_uncompressed();
    if (seq.empty()) { return false; }
    *this = std::move(seq);
    return true;
}

std::vector<std::uint8_t> byteseq::make_vector() const {
    std::vector<std::uint8_t> result;
    scan([&result](const std::uint8_t* p, std::size_t sz) { result.insert(result.end(), p, p + sz); });
    return result;
}

/*static*/ byteseq byteseq::from_vector(est::span<const std::uint8_t> v) {
    byteseq seq;
    return seq.assign(v.size(), [&v](std::uint8_t* dst, std::size_t dst_sz) {
        std::memcpy(dst, v.data(), dst_sz);
        return dst_sz;
    });
    return seq;
}

#if defined(UXS_USE_ZLIB)
byteseq byteseq::make_compressed() const {
    if (empty()) { return {}; }

    byteseq seq;
    seq.create_head_chunk();

    z_stream zstr;
    std::memset(&zstr, 0, sizeof(z_stream));
    ::deflateInit(&zstr, Z_DEFAULT_COMPRESSION);

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

byteseq byteseq::make_uncompressed() const {
    if (empty()) { return {}; }

    byteseq seq;
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
byteseq byteseq::make_compressed() const { return *this; }
byteseq byteseq::make_uncompressed() const { return *this; }
#endif  // defined(UXS_USE_ZLIB)

void byteseq::delete_chunks() noexcept {
    chunk_t* chunk = head_->next;
    while (chunk != head_) {
        chunk_t* next = chunk->next;
        chunk_t::dealloc(*this, chunk);
        chunk = next;
    }
}

void byteseq::clear_and_reserve(std::size_t cap) {
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

/*static*/ detail::byteseq_chunk* detail::byteseq_chunk::alloc(alloc_type& al, std::size_t cap) {
    const std::size_t alloc_sz = get_alloc_sz(cap);
    detail::byteseq_chunk* chunk = al.allocate(alloc_sz);
    chunk->boundary = chunk->data + alloc_sz * sizeof(detail::byteseq_chunk) - offsetof(detail::byteseq_chunk, data);
    assert(chunk->capacity() >= cap && get_alloc_sz(chunk->capacity()) == alloc_sz);
    return chunk;
}

void byteseq::create_head(std::size_t cap) {
    if (cap > chunk_t::max_size(*this)) { throw std::length_error("too much to reserve"); }
    head_ = chunk_t::alloc(*this, cap);
    dllist_make_cycle(head_);
    head_->end = head_->data;
}

void byteseq::create_head_chunk() {
    head_ = chunk_t::alloc(*this, chunk_size);
    dllist_make_cycle(head_);
    head_->end = head_->data;
}

void byteseq::create_next_chunk() {
    chunk_t* chunk = chunk_t::alloc(*this, chunk_size);
    dllist_insert_after(head_, chunk);
    chunk->end = chunk->data;
    size_ += head_->avail();
    head_->end = head_->boundary;
    head_ = chunk;
}
