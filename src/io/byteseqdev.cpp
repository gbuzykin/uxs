#include "uxs/io/byteseqdev.h"

#include <cstring>

using namespace uxs;

//---------------------------------------------------------------------------------
// byteseqdev class implementation

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
    if (!wr || chunk_ != seq_.head_) {
        sz = chunk_ ? chunk_->size() - (pos_ - pos0_) : 0;
        return chunk_ ? chunk_->data + (pos_ - pos0_) : nullptr;
    }
    if (!chunk_) {
        seq_.create_head_chunk();
        chunk_ = seq_.head_;
        std::memset(chunk_->data, 0, chunk_->capacity());
        chunk_->end = chunk_->data;
    } else if (pos_ - pos0_ == chunk_->capacity()) {
        pos0_ += chunk_->capacity();
        seq_.create_next_chunk();
        chunk_ = seq_.head_;
        std::memset(chunk_->data, 0, chunk_->capacity());
        chunk_->end = chunk_->data;
    }
    sz = chunk_->capacity() - (pos_ - pos0_);
    return chunk_->data + (pos_ - pos0_);
}

int64_t byteseqdev::seek(int64_t off, seekdir dir) {
    switch (dir) {
        case uxs::seekdir::beg: {
            if (off < 0) { return -1; }
            pos_ = static_cast<size_t>(off);
        } break;
        case uxs::seekdir::curr: {
            if (off < 0 && static_cast<size_t>(-off) > pos_) { return -1; }
            pos_ += static_cast<size_t>(off);
        } break;
        case uxs::seekdir::end: {
            if (off < 0 && static_cast<size_t>(-off) > seq_.size()) { return -1; }
            pos_ = seq_.size() + static_cast<size_t>(off);
        } break;
    }

    if (pos_ > 0 && !chunk_) {
        seq_.create_head_chunk();
        chunk_ = seq_.head_;
        std::memset(chunk_->data, 0, chunk_->capacity());
        chunk_->end = chunk_->data;
    }
    if (pos_ > pos0_) {
        while (chunk_ != seq_.head_ && pos_ - pos0_ >= chunk_->size()) {
            pos0_ += chunk_->size();
            chunk_ = chunk_->next;
        }
        if (chunk_ == seq_.head_) {
            while (pos_ - pos0_ > chunk_->capacity()) {
                pos0_ += chunk_->capacity();
                seq_.create_next_chunk();
                chunk_ = seq_.head_;
                std::memset(chunk_->data, 0, chunk_->capacity());
                chunk_->end = chunk_->data;
            }
            if (pos_ - pos0_ > chunk_->size()) {
                const size_t extra = pos_ - pos0_ - chunk_->size();
                seq_.size_ += extra, chunk_->end += extra;
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
