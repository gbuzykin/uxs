#include "uxs/byteseq.h"

#include "uxs/crc32.h"
#include "uxs/dllist.h"

#if defined(UXS_USE_ZLIB)
#    define ZLIB_CONST
#    include <zlib.h>
#endif

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
    dllist_make_cycle(head_);
    head_->end = head_->data;
    size_ = 0;
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
    auto buf = make_compressed();
    if (buf.empty()) { return false; }
    *this = std::move(buf);
    return true;
}

bool byteseq::uncompress() {
    if (empty()) { return true; }
    auto buf = make_uncompressed();
    if (buf.empty()) { return false; }
    *this = std::move(buf);
    return true;
}

std::vector<std::uint8_t> byteseq::make_vector() const {
    std::vector<std::uint8_t> result;
    scan([&result](const std::uint8_t* p, std::size_t sz) { result.insert(result.end(), p, p + sz); });
    return result;
}

/*static*/ byteseq byteseq::from_vector(uxs::span<const std::uint8_t> v) {
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

    byteseq buf;
    buf.create_head_chunk();

    z_stream zstr;
    std::memset(&zstr, 0, sizeof(z_stream));
    deflateInit(&zstr, Z_DEFAULT_COMPRESSION);

    const chunk_t* chunk = head_->next;
    zstr.next_in = chunk->data;
    zstr.next_out = buf.head_->data;

    while (true) {
        if (zstr.next_in == chunk->end && chunk != head_) {
            chunk = chunk->next;
            zstr.next_in = chunk->data;
        }

        zstr.avail_in = static_cast<uInt>(std::min<std::size_t>(chunk->end - zstr.next_in, max_avail_count));
        zstr.avail_out = static_cast<uInt>(buf.head_->boundary - zstr.next_out);

        int ret = deflate(&zstr, zstr.avail_in ? Z_NO_FLUSH : Z_FINISH);
        if (ret == Z_STREAM_END) {
            if (deflateEnd(&zstr) != Z_OK) { return {}; }
            break;
        } else if (ret != Z_OK) {
            deflateEnd(&zstr);
            return {};
        }

        if (zstr.next_out == buf.head_->boundary) {
            buf.create_next_chunk();
            zstr.next_out = buf.head_->data;
        }
    }

    buf.head_->end = zstr.next_out;
    buf.size_ += buf.head_->size();
    return buf;
}

byteseq byteseq::make_uncompressed() const {
    if (empty()) { return {}; }

    byteseq buf;
    buf.create_head_chunk();

    z_stream zstr;
    std::memset(&zstr, 0, sizeof(z_stream));
    inflateInit(&zstr);

    const chunk_t* chunk = head_->next;
    zstr.next_in = chunk->data;
    zstr.next_out = buf.head_->data;

    while (true) {
        if (zstr.next_in == chunk->end && chunk != head_) {
            chunk = chunk->next;
            zstr.next_in = chunk->data;
        }

        zstr.avail_in = static_cast<uInt>(std::min<std::size_t>(chunk->end - zstr.next_in, max_avail_count));
        zstr.avail_out = static_cast<uInt>(buf.head_->boundary - zstr.next_out);

        int ret = inflate(&zstr, zstr.avail_in ? Z_NO_FLUSH : Z_FINISH);
        if (ret == Z_STREAM_END) {
            if (inflateEnd(&zstr) != Z_OK) { return {}; }
            break;
        } else if (ret != Z_OK) {
            inflateEnd(&zstr);
            return {};
        }

        if (zstr.next_out == buf.head_->boundary) {
            buf.create_next_chunk();
            zstr.next_out = buf.head_->data;
        }
    }

    buf.head_->end = zstr.next_out;
    buf.size_ += buf.head_->size();
    return buf;
}
#else
byteseq byteseq::make_compressed() const { return *this; }
byteseq byteseq::make_uncompressed() const { return *this; }
#endif

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
        // delete chunks excepts of the last
        if (head_->capacity() < cap) {
            // create new head buffer
            chunk_t::dealloc(*this, head_);
            create_head(cap);
        } else {  // reuse head buffer
            dllist_make_cycle(head_);
            head_->end = head_->data;
        }
        size_ = 0;
    } else if (cap) {
        create_head(cap);
    }
}

/*static*/ detail::byteseq_chunk_t* detail::byteseq_chunk_t::alloc(alloc_type& al, std::size_t cap) {
    const std::size_t alloc_sz = get_alloc_sz(cap);
    detail::byteseq_chunk_t* chunk = al.allocate(alloc_sz);
    chunk->boundary = chunk->data + alloc_sz * sizeof(detail::byteseq_chunk_t) -
                      offsetof(detail::byteseq_chunk_t, data);
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
