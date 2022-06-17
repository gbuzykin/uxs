#pragma once

#include "iostate.h"

#include "uxs/span.h"

namespace uxs {

class iodevice {
 public:
    iodevice() = default;
    virtual ~iodevice() = default;
    iodevice(const iodevice&) = delete;
    iodevice& operator=(const iodevice&) = delete;
    virtual int read(void* data, size_t sz, size_t& n_read) = 0;
    virtual int write(const void* data, size_t sz, size_t& n_written) = 0;
    virtual int64_t seek(int64_t off, seekdir dir) { return -1; }
    virtual int ctrlesc_color(span<uint8_t> v) { return -1; }
    virtual int flush() = 0;
};

}  // namespace uxs
