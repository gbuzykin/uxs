#pragma once

#include "iodevice.h"

#include "uxs/byteseq.h"

namespace uxs {

class UXS_EXPORT_ALL_STUFF_FOR_GNUC byteseqdev : public iodevice {
 public:
    byteseqdev() : iodevice(iodevcaps::mappable) {}
    explicit byteseqdev(byteseq seq)
        : iodevice(iodevcaps::mappable), seq_(std::move(seq)), chunk_(seq_.head_ ? seq_.head_->next : nullptr) {}
    byteseqdev(byteseqdev&& other) NOEXCEPT : seq_(std::move(other.seq_)),
                                              pos0_(other.pos0_),
                                              pos_(other.pos_),
                                              chunk_(other.chunk_) {
        other.pos0_ = other.pos_ = 0;
        other.chunk_ = nullptr;
    }
    byteseqdev& operator=(byteseqdev&& other) NOEXCEPT {
        if (&other == this) { return *this; }
        seq_ = std::move(other.seq_);
        pos0_ = other.pos0_, pos_ = other.pos_;
        chunk_ = other.chunk_;
        other.pos0_ = other.pos_ = 0;
        other.chunk_ = nullptr;
        return *this;
    }

    const byteseq& get() const { return seq_; }
    byteseq detach() { return std::move(seq_); }
    void clear() {
        seq_.clear();
        pos0_ = pos_ = 0;
        chunk_ = seq_.head_;
    }
    UXS_EXPORT int read(void* data, size_t sz, size_t& n_read) final;
    UXS_EXPORT int write(const void* data, size_t sz, size_t& n_written) final;
    UXS_EXPORT void* map(size_t& sz, bool wr) final;
    UXS_EXPORT int64_t seek(int64_t off, seekdir dir) final;
    UXS_EXPORT int ctrlesc_color(uxs::span<uint8_t> v) final { return -1; }
    int flush() final { return 0; }

 private:
    byteseq seq_;
    size_t pos0_ = 0;
    size_t pos_ = 0;
    byteseq::chunk_t* chunk_ = nullptr;
};

}  // namespace uxs
