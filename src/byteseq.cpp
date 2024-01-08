#include "uxs/byteseq.h"

#include "uxs/crc32.h"
#include "uxs/dllist.h"

#if defined(UXS_USE_ZLIB)
#    define ZLIB_CONST
#    include <zlib.h>
#endif

#include <cstring>
#include <stdexcept>

using namespace uxs;

//---------------------------------------------------------------------------------
// byteseq class implementation

byteseq::~byteseq() {
    if (!head_) { return; }
    chunk_t::alloc_type al;
    chunk_t* chunk = head_->next;
    while (chunk != head_) {
        chunk_t* next = chunk->next;
        chunk_t::dealloc(al, chunk);
        chunk = next;
    }
    chunk_t::dealloc(al, head_);
}

uint32_t byteseq::calc_crc32() const {
    uint32_t crc32 = 0xffffffff;
    if (!size_) { return crc32; }
    chunk_t* chunk = head_->next;
    do {
        crc32 = crc32::calc(chunk->data, chunk->end, crc32);
        chunk = chunk->next;
    } while (chunk != head_->next);
    return crc32;
}

void byteseq::clear() {
    clear_and_reserve(0);
    if (head_) { head_->end = head_->data; }
    size_ = 0;
}

void byteseq::assign(const byteseq& other) {
    size_t sz = other.size_;
    flags_ = other.flags_;
    clear_and_reserve(sz);
    if (head_) {
        uint8_t* p = head_->data;
        chunk_t* chunk = other.head_->next;
        do {
            std::memcpy(p, chunk->data, chunk->size());
            p += chunk->size();
            chunk = chunk->next;
        } while (chunk != other.head_->next);
        head_->end = head_->data + sz;
    }
    size_ = sz;
}

bool byteseq::compress() {
    if (!!(flags_ & byteseq_flags::compressed)) {
        return true;
    } else if (empty()) {
        flags_ |= byteseq_flags::compressed;
        return true;
    }
    auto buf = make_compressed();
    if (buf.empty()) { return false; }
    *this = std::move(buf);
    return true;
}

bool byteseq::uncompress() {
    if (!(flags_ & byteseq_flags::compressed)) {
        return true;
    } else if (empty()) {
        flags_ &= ~byteseq_flags::compressed;
        return true;
    }
    auto buf = make_uncompressed();
    if (buf.empty()) { return false; }
    *this = std::move(buf);
    return true;
}

std::vector<uint8_t> byteseq::make_vector() const {
    std::vector<uint8_t> result;
    if (!size_) { return result; }
    result.reserve(size_);
    chunk_t* chunk = head_->next;
    do {
        result.insert(result.end(), chunk->data, chunk->end);
        chunk = chunk->next;
    } while (chunk != head_->next);
    return result;
}

/*static*/ byteseq byteseq::from_vector(uxs::span<const uint8_t> v, byteseq_flags flags) {
    byteseq buf(flags);
    buf.create_head_checked(v.size());
    if (buf.head_) {
        std::memcpy(buf.head_->data, v.data(), v.size());
        buf.head_->end = buf.head_->data + v.size();
    }
    buf.size_ = v.size();
    return buf;
}

#if defined(UXS_USE_ZLIB)
byteseq byteseq::make_compressed() const {
    if (!!(flags_ & byteseq_flags::compressed)) {
        return *this;
    } else if (empty()) {
        return byteseq(flags_ | byteseq_flags::compressed);
    }

    byteseq buf(flags_ | byteseq_flags::compressed);
    buf.create_head_chunk();

    z_stream zstr;
    std::memset(&zstr, 0, sizeof(z_stream));
    deflateInit(&zstr, Z_DEFAULT_COMPRESSION);

    chunk_t* chunk = head_->next;
    zstr.next_in = chunk->data;
    zstr.next_out = buf.head_->end = buf.head_->data;

    while (true) {
        if (zstr.next_in == chunk->end && chunk != head_) {
            chunk = chunk->next;
            zstr.next_in = chunk->data;
        }

        zstr.avail_in = static_cast<uInt>(std::min<size_t>(chunk->end - zstr.next_in, max_avail_count));
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
            zstr.next_out = buf.head_->end = buf.head_->data;
        }
    }

    buf.head_->end = zstr.next_out;
    buf.size_ += buf.head_->size();
    return buf;
}

byteseq byteseq::make_uncompressed() const {
    if (!(flags_ & byteseq_flags::compressed)) {
        return *this;
    } else if (empty()) {
        return byteseq(flags_ & ~byteseq_flags::compressed);
    }

    byteseq buf(flags_ & ~byteseq_flags::compressed);
    buf.create_head_chunk();

    z_stream zstr;
    std::memset(&zstr, 0, sizeof(z_stream));
    inflateInit(&zstr);

    chunk_t* chunk = head_->next;
    zstr.next_in = chunk->data;
    zstr.next_out = buf.head_->end = buf.head_->data;

    while (true) {
        if (zstr.next_in == chunk->end && chunk != head_) {
            chunk = chunk->next;
            zstr.next_in = chunk->data;
        }

        zstr.avail_in = static_cast<uInt>(std::min<size_t>(chunk->end - zstr.next_in, max_avail_count));
        zstr.avail_out = static_cast<uInt>(buf.head_->boundary - zstr.next_out);

        int ret = inflate(&zstr, zstr.avail_in ? Z_NO_FLUSH : Z_FINISH);
        if (ret == Z_STREAM_END) {
            if (inflateEnd(&zstr) != Z_OK) { return byteseq(byteseq_flags::compressed); }
            break;
        } else if (ret != Z_OK) {
            inflateEnd(&zstr);
            return byteseq(byteseq_flags::compressed);
        }

        if (zstr.next_out == buf.head_->boundary) {
            buf.create_next_chunk();
            zstr.next_out = buf.head_->end = buf.head_->data;
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

void byteseq::clear_and_reserve(size_t cap) {
    chunk_t::alloc_type al;
    if (head_) {
        // delete chunks excepts of the last
        chunk_t* chunk = head_->next;
        while (chunk != head_) {
            chunk_t* next = chunk->next;
            chunk_t::dealloc(al, chunk);
            chunk = next;
        }
        if (head_->capacity() < cap) {
            // create new head buffer
            chunk_t::dealloc(al, head_);
            create_head_checked(cap);
        } else {  // reuse head buffer
            dllist_make_cycle(head_);
        }
    } else if (cap) {
        create_head_checked(cap);
    }
}

/*static*/ byteseq::chunk_t* byteseq::chunk_t::alloc(alloc_type& al, size_t cap) {
    const size_t alloc_sz = get_alloc_sz(cap);
    chunk_t* chunk = al.allocate(alloc_sz);
    chunk->boundary = reinterpret_cast<uint8_t*>(&chunk[alloc_sz]);
    assert(chunk->capacity() >= cap && get_alloc_sz(chunk->capacity()) == alloc_sz);
    return chunk;
}

void byteseq::create_head_chunk() {
    chunk_t::alloc_type al;
    head_ = chunk_t::alloc(al, chunk_size);
    dllist_make_cycle(head_);
}

void byteseq::create_next_chunk() {
    chunk_t::alloc_type al;
    chunk_t* chunk = chunk_t::alloc(al, chunk_size);
    dllist_insert_after(head_, chunk);
    size_ += head_->avail();
    head_->end = head_->boundary;
    head_ = chunk;
}

void byteseq::create_head_checked(size_t cap) {
    chunk_t::alloc_type al;
    if (cap > chunk_t::max_size(al)) { throw std::length_error("too much to reserve"); }
    head_ = chunk_t::alloc(al, cap);
    dllist_make_cycle(head_);
}
