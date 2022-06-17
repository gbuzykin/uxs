#pragma once

#include "iostate.h"

#include "uxs/span.h"

namespace uxs {

enum class iodevcaps : uint32_t {
    kNone = 0,
    kMappable = 1,
};
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(iodevcaps, uint32_t);

class iodevice {
 public:
    iodevice() = default;
    explicit iodevice(iodevcaps caps) : caps_(caps) {}
    virtual ~iodevice() = default;
    iodevice(const iodevice&) = delete;
    iodevice& operator=(const iodevice&) = delete;
    iodevcaps caps() const { return caps_; }
    virtual int read(void* data, size_t sz, size_t& n_read) = 0;
    virtual int write(const void* data, size_t sz, size_t& n_written) = 0;
    virtual void* map(size_t& sz, bool wr = false) { return nullptr; }
    virtual int64_t seek(int64_t off, seekdir dir = seekdir::kBeg) { return -1; }
    virtual int ctrlesc_color(span<uint8_t> v) { return -1; }
    virtual int flush() = 0;

 private:
    iodevcaps caps_ = iodevcaps::kNone;
};

}  // namespace uxs
