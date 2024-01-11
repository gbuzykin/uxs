#pragma once

#include "iodevice.h"

#include "uxs/byteseq.h"

namespace uxs {

class UXS_EXPORT_ALL_STUFF_FOR_GNUC byteseqdev : public iodevice {
 public:
    explicit byteseqdev(const byteseq& seq)
        : iodevice(iodevcaps::mappable), rdonly_(true), seq_(&const_cast<byteseq&>(seq)),
          chunk_(seq_->head_ ? seq_->head_->next : nullptr) {}
    explicit byteseqdev(byteseq& seq)
        : iodevice(iodevcaps::mappable), rdonly_(false), seq_(&seq), chunk_(seq_->head_ ? seq_->head_->next : nullptr) {
    }
    byteseqdev(byteseqdev&& other) noexcept
        : rdonly_(other.rdonly_), seq_(other.seq_), chunk_(other.chunk_), pos0_(other.pos0_), pos_(other.pos_) {
        other.seq_ = nullptr;
    }
    byteseqdev& operator=(byteseqdev&& other) noexcept {
        if (&other == this) { return *this; }
        rdonly_ = other.rdonly_, seq_ = other.seq_, chunk_ = other.chunk_;
        pos0_ = other.pos0_, pos_ = other.pos_;
        other.seq_ = nullptr;
        return *this;
    }

    const byteseq* get() const { return seq_; }
    void clear() {
        if (!seq_ || rdonly_) { return; }
        seq_->clear();
        chunk_ = seq_->head_;
        pos0_ = pos_ = 0;
    }
    UXS_EXPORT int read(void* data, size_t sz, size_t& n_read) final;
    UXS_EXPORT int write(const void* data, size_t sz, size_t& n_written) final;
    UXS_EXPORT void* map(size_t& sz, bool wr) final;
    UXS_EXPORT int64_t seek(int64_t off, seekdir dir) final;
    int flush() final { return 0; }

 private:
    bool rdonly_;
    byteseq* seq_;
    byteseq::chunk_t* chunk_ = nullptr;
    size_t pos0_ = 0;
    size_t pos_ = 0;
};

}  // namespace uxs
