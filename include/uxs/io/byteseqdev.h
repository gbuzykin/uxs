#pragma once

#include "iodevice.h"

#include "uxs/byteseq.h"

namespace uxs {

template<typename Alloc>
class basic_byteseqdev : public iodevice {
 public:
    explicit basic_byteseqdev(const basic_byteseq<Alloc>& seq) noexcept
        : iodevice(iodevcaps::rdonly | iodevcaps::mappable), seq_(&const_cast<basic_byteseq<Alloc>&>(seq)),
          chunk_(seq_->head_ ? seq_->head_->next : nullptr) {}
    explicit basic_byteseqdev(basic_byteseq<Alloc>& seq) noexcept
        : iodevice(iodevcaps::mappable), seq_(&seq), chunk_(seq_->head_ ? seq_->head_->next : nullptr) {}

    UXS_EXPORT basic_byteseqdev(basic_byteseqdev&& other) noexcept;
    UXS_EXPORT basic_byteseqdev& operator=(basic_byteseqdev&& other) noexcept;

    const basic_byteseq<Alloc>* get() const noexcept { return seq_; }
    UXS_EXPORT void clear() noexcept;
    UXS_EXPORT int read(void* data, std::size_t sz, std::size_t& n_read) override;
    UXS_EXPORT int write(const void* data, std::size_t sz, std::size_t& n_written) override;
    UXS_EXPORT void* map(std::size_t& sz, bool wr) override;
    UXS_EXPORT void advance(std::size_t n) override;
    UXS_EXPORT std::int64_t seek(std::int64_t off, seekdir dir) override;
    UXS_EXPORT int truncate() override;
    int flush() override { return 0; }

 private:
    basic_byteseq<Alloc>* seq_;
    typename basic_byteseq<Alloc>::chunk_t* chunk_ = nullptr;
    std::size_t pos0_ = 0;
    std::size_t pos_ = 0;
};

using byteseqdev = basic_byteseqdev<std::allocator<std::uint8_t>>;

}  // namespace uxs
