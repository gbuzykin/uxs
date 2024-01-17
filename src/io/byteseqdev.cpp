#include "uxs/io/byteseqdev.h"

#include <cstring>

using namespace uxs;

//---------------------------------------------------------------------------------
// byteseqdev class implementation

byteseqdev::byteseqdev(byteseqdev&& other) noexcept
    : seq_(other.seq_), chunk_(other.chunk_), pos0_(other.pos0_), pos_(other.pos_) {
    other.seq_ = nullptr;
}
byteseqdev& byteseqdev::operator=(byteseqdev&& other) noexcept {
    if (&other == this) { return *this; }
    seq_ = other.seq_, chunk_ = other.chunk_;
    pos0_ = other.pos0_, pos_ = other.pos_;
    other.seq_ = nullptr;
    return *this;
}

void byteseqdev::clear() {
    if (!seq_ || !!(this->caps() & iodevcaps::rdonly)) { return; }
    seq_->clear();
    chunk_ = seq_->head_;
    pos0_ = pos_ = 0;
}

int byteseqdev::read(void* data, size_t sz, size_t& n_read) {
    const size_t sz0 = sz;
    while (sz) {
        size_t mapped_sz = 0;
        void* p = map(mapped_sz, false);
        if (!p || !mapped_sz) { break; }
        if (sz < mapped_sz) { mapped_sz = sz; }
        std::memcpy(data, p, mapped_sz);
        seek(mapped_sz, seekdir::curr);
        data = static_cast<uint8_t*>(data) + mapped_sz, sz -= mapped_sz;
    }
    n_read = sz0 - sz;
    return 0;
}

int byteseqdev::write(const void* data, size_t sz, size_t& n_written) {
    const size_t sz0 = sz;
    while (sz) {
        size_t mapped_sz = 0;
        void* p = map(mapped_sz, true);
        if (!p || !mapped_sz) { return -1; }
        if (sz < mapped_sz) { mapped_sz = sz; }
        std::memcpy(p, data, mapped_sz);
        seek(mapped_sz, seekdir::curr);
        data = static_cast<const uint8_t*>(data) + mapped_sz, sz -= mapped_sz;
    }
    n_written = sz0;
    return 0;
}

void* byteseqdev::map(size_t& sz, bool wr) {
    if (!seq_ || (wr && !!(this->caps() & iodevcaps::rdonly))) { return nullptr; }
    if (!wr || chunk_ != seq_->head_) {
        sz = chunk_ ? chunk_->size() - (pos_ - pos0_) : 0;
        return chunk_ ? chunk_->data + (pos_ - pos0_) : nullptr;
    } else if (chunk_ && pos_ - pos0_ < chunk_->capacity()) {
        sz = chunk_->capacity() - (pos_ - pos0_);
        return chunk_->data + (pos_ - pos0_);
    }
    if (!chunk_) {
        seq_->create_head_chunk();
    } else {
        pos0_ += chunk_->capacity();
        seq_->create_next_chunk();
    }
    chunk_ = seq_->head_;
    std::memset(chunk_->data, 0, chunk_->capacity());
    sz = chunk_->capacity();
    return chunk_->data;
}

int64_t byteseqdev::seek(int64_t off, seekdir dir) {
    if (!seq_) { return -1; }

    switch (dir) {
        case uxs::seekdir::beg: {
            pos_ = off > 0 ? static_cast<size_t>(off) : 0;
        } break;
        case uxs::seekdir::curr: {
            if (off == 0) { return pos_; }
            pos_ = off > 0 || static_cast<size_t>(-off) < pos_ ? pos_ + off : 0;
        } break;
        case uxs::seekdir::end: {
            pos_ = off > 0 || static_cast<size_t>(-off) < seq_->size_ ? seq_->size_ + off : 0;
        } break;
    }

    if (pos_ > pos0_) {
        while (chunk_ != seq_->head_ && pos_ - pos0_ >= chunk_->size()) {
            pos0_ += chunk_->size();
            chunk_ = chunk_->next;
        }
        if (chunk_ == seq_->head_) {
            if (!!(this->caps() & iodevcaps::rdonly)) {
                pos_ = std::min(pos_, seq_->size_);
                return static_cast<int64_t>(pos_);
            }
            if (!chunk_) {
                seq_->create_head_chunk();
                chunk_ = seq_->head_;
                std::memset(chunk_->data, 0, chunk_->capacity());
            }
            while (pos_ - pos0_ > chunk_->capacity()) {
                pos0_ += chunk_->capacity();
                seq_->create_next_chunk();
                chunk_ = seq_->head_;
                std::memset(chunk_->data, 0, chunk_->capacity());
            }
            if (pos_ - pos0_ > chunk_->size()) {
                const size_t extra = pos_ - pos0_ - chunk_->size();
                seq_->size_ += extra, chunk_->end += extra;
            }
        }
    } else {
        while (pos_ < pos0_) {
            chunk_ = chunk_->prev;
            pos0_ -= chunk_->size();
        }
    }
    return static_cast<int64_t>(pos_);
}
