#pragma once

#include "iodevice.h"

#include "uxs/byteseq.h"

namespace uxs {

class UXS_EXPORT_ALL_STUFF_FOR_GNUC byteseqdev : public iodevice {
 public:
    explicit byteseqdev(const byteseq& seq) noexcept
        : iodevice(iodevcaps::rdonly | iodevcaps::mappable), seq_(&const_cast<byteseq&>(seq)),
          chunk_(seq_->head_ ? seq_->head_->next : nullptr) {}
    explicit byteseqdev(byteseq& seq) noexcept
        : iodevice(iodevcaps::mappable), seq_(&seq), chunk_(seq_->head_ ? seq_->head_->next : nullptr) {}
    UXS_EXPORT byteseqdev(byteseqdev&& other) noexcept;
    UXS_EXPORT byteseqdev& operator=(byteseqdev&& other) noexcept;

    const byteseq* get() const noexcept { return seq_; }
    UXS_EXPORT void clear() noexcept;
    UXS_EXPORT int read(void* data, std::size_t sz, std::size_t& n_read) override;
    UXS_EXPORT int write(const void* data, std::size_t sz, std::size_t& n_written) override;
    UXS_EXPORT void* map(std::size_t& sz, bool wr) override;
    UXS_EXPORT void advance(std::size_t n) override;
    UXS_EXPORT std::int64_t seek(std::int64_t off, seekdir dir) override;
    UXS_EXPORT int truncate() override;
    int flush() override { return 0; }

 private:
    byteseq* seq_;
    byteseq::chunk_t* chunk_ = nullptr;
    std::size_t pos0_ = 0;
    std::size_t pos_ = 0;
};

}  // namespace uxs
