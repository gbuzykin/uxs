#pragma once

#include "iostate.h"

#include "uxs/span.h"

namespace uxs {

enum class iodevcaps : unsigned { none = 0, rdonly = 1, mappable = 2 };
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(iodevcaps);

class iodevice {
 public:
    iodevice() noexcept = default;
    explicit iodevice(iodevcaps caps) noexcept : caps_(caps) {}
    virtual ~iodevice() = default;
    iodevice(const iodevice&) = delete;
    iodevice& operator=(const iodevice&) = delete;
    iodevcaps caps() const noexcept { return caps_; }
    virtual int read(void* data, std::size_t sz, std::size_t& n_read) = 0;
    virtual int write(const void* data, std::size_t sz, std::size_t& n_written) = 0;
    virtual void* map(std::size_t& /*sz*/, bool /*wr*/) { return nullptr; }
    virtual void advance(std::size_t /*n*/) {}
    virtual std::int64_t seek(std::int64_t /*off*/, seekdir /*dir*/) { return -1; }
    virtual int ctrlesc_color(est::span<const std::uint8_t> /*v*/) { return -1; }
    virtual int truncate() { return -1; }
    virtual int flush() = 0;

 private:
    iodevcaps caps_ = iodevcaps::none;
};

}  // namespace uxs
