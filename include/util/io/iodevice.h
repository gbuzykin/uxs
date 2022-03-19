#pragma once

#include "../span.h"
#include "iostate.h"

namespace util {

class iodevice {
 public:
    using size_type = size_t;
    iodevice() = default;
    virtual ~iodevice() = default;
    iodevice(const iodevice&) = delete;
    iodevice& operator=(const iodevice&) = delete;
    virtual int read(void* data, size_type sz, size_type& n_read) = 0;
    virtual int write(const void* data, size_type sz, size_type& n_written) = 0;
    virtual int ctrlesc_color(util::span<uint8_t> v) = 0;
    virtual int flush() = 0;
};

}  // namespace util
