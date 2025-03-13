#pragma once

#include "uxs/io/byteseqdev.h"

#include <cassert>
#include <cstring>

namespace uxs {

template<typename Alloc>
basic_byteseqdev<Alloc>::basic_byteseqdev(basic_byteseqdev&& other) noexcept
    : seq_(other.seq_), chunk_(other.chunk_), pos0_(other.pos0_), pos_(other.pos_) {
    other.seq_ = nullptr;
    other.chunk_ = nullptr;
}

template<typename Alloc>
basic_byteseqdev<Alloc>& basic_byteseqdev<Alloc>::operator=(basic_byteseqdev&& other) noexcept {
    if (&other == this) { return *this; }
    seq_ = other.seq_;
    chunk_ = other.chunk_;
    pos0_ = other.pos0_, pos_ = other.pos_;
    other.seq_ = nullptr;
    other.chunk_ = nullptr;
    return *this;
}

template<typename Alloc>
void basic_byteseqdev<Alloc>::clear() noexcept {
    if (!seq_ || !!(this->caps() & iodevcaps::rdonly)) { return; }
    seq_->clear();
    chunk_ = seq_->head_;
    pos0_ = pos_ = 0;
}

template<typename Alloc>
int basic_byteseqdev<Alloc>::read(void* data, std::size_t sz, std::size_t& n_read) {
    const std::size_t sz0 = sz;
    while (sz) {
        std::size_t mapped_sz = 0;
        void* p = basic_byteseqdev::map(mapped_sz, false);
        if (!p || !mapped_sz) { break; }
        if (sz < mapped_sz) { mapped_sz = sz; }
        std::memcpy(data, p, mapped_sz);
        basic_byteseqdev::advance(mapped_sz);
        data = static_cast<std::uint8_t*>(data) + mapped_sz, sz -= mapped_sz;
    }
    n_read = sz0 - sz;
    return 0;
}

template<typename Alloc>
int basic_byteseqdev<Alloc>::write(const void* data, std::size_t sz, std::size_t& n_written) {
    const std::size_t sz0 = sz;
    while (sz) {
        std::size_t mapped_sz = 0;
        void* p = basic_byteseqdev::map(mapped_sz, true);
        if (!p || !mapped_sz) { return -1; }
        if (sz < mapped_sz) { mapped_sz = sz; }
        std::memcpy(p, data, mapped_sz);
        basic_byteseqdev::advance(mapped_sz);
        data = static_cast<const std::uint8_t*>(data) + mapped_sz, sz -= mapped_sz;
    }
    n_written = sz0;
    return 0;
}

template<typename Alloc>
void* basic_byteseqdev<Alloc>::map(std::size_t& sz, bool wr) {
    if (!seq_ || (wr && !!(this->caps() & iodevcaps::rdonly))) { return nullptr; }
    const std::size_t chunk_pos = pos_ - pos0_;
    if (!wr || chunk_ != seq_->head_) {
        sz = chunk_ ? chunk_->size() - chunk_pos : 0;
        return chunk_ ? chunk_->data + chunk_pos : nullptr;
    }
    if (chunk_ && chunk_pos < chunk_->capacity()) {
        sz = chunk_->capacity() - chunk_pos;
        return chunk_->data + chunk_pos;
    }
    if (!chunk_) {
        seq_->create_head_chunk();
    } else {
        const std::size_t delta = chunk_->capacity();
        seq_->create_next_chunk();
        pos0_ += delta;
    }
    chunk_ = seq_->head_;
    sz = chunk_->capacity();
    return chunk_->data;
}

template<typename Alloc>
void basic_byteseqdev<Alloc>::advance(std::size_t n) {
    if (!seq_ || !n) { return; }
    assert(chunk_ && seq_->head_);
    pos_ += n;
    const std::size_t chunk_pos = pos_ - pos0_;
    if (pos_ > seq_->size_) {
        assert(chunk_ == seq_->head_ && chunk_pos <= chunk_->capacity());
        chunk_->end += pos_ - seq_->size_, seq_->size_ = pos_;
    } else if (chunk_ != seq_->head_ && chunk_pos >= chunk_->size()) {
        assert(chunk_pos == chunk_->size());
        pos0_ += chunk_->size();
        chunk_ = chunk_->next;
    }
}

template<typename Alloc>
std::int64_t basic_byteseqdev<Alloc>::seek(std::int64_t off, seekdir dir) {
    if (!seq_) { return -1; }

    switch (dir) {
        case seekdir::beg: {
            pos_ = off > 0 ? static_cast<std::size_t>(off) : 0;
        } break;
        case seekdir::curr: {
            if (off == 0) { return pos_; }
            pos_ = static_cast<std::size_t>(off > 0 || static_cast<std::size_t>(-off) < pos_ ? pos_ + off : 0);
        } break;
        case seekdir::end: {
            pos_ = static_cast<std::size_t>(
                off > 0 || static_cast<std::size_t>(-off) < seq_->size_ ? seq_->size_ + off : 0);
        } break;
    }

    if (pos_ > pos0_) {
        if (pos_ >= seq_->size_) {
            if (!!(this->caps() & iodevcaps::rdonly)) {
                pos_ = seq_->size_;
            } else if (pos_ > seq_->size_) {
                seq_->resize(pos_);
            }
            chunk_ = seq_->head_;
            pos0_ = pos_ - chunk_->size();
        } else {
            while (chunk_ != seq_->head_ && pos_ - pos0_ >= chunk_->size()) {
                pos0_ += chunk_->size();
                chunk_ = chunk_->next;
            }
        }
    } else {
        while (pos_ < pos0_) {
            chunk_ = chunk_->prev;
            pos0_ -= chunk_->size();
        }
    }
    return static_cast<std::int64_t>(pos_);
}

template<typename Alloc>
int basic_byteseqdev<Alloc>::truncate() {
    if (!seq_ || !!(this->caps() & iodevcaps::rdonly)) { return -1; }
    seq_->resize(pos_);
    return 0;
}

}  // namespace uxs
