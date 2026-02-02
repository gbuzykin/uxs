#pragma once

#include "uxs/utility.h"

#include <stdexcept>
#include <string>

namespace uxs {

enum class iomode : std::uint16_t {
    none = 0,
    in = 1,
    out = 2,
    truncate = 4,
    append = 8,
    create = 0x10,
    exclusive = 0x20,
    cr_lf = 0x40,
#if defined(WIN32)
    text = cr_lf,
#else   // defined(WIN32)
    text = 0,
#endif  // defined(WIN32)
    z_compr = 0x80,
    z_compr_level = 0x100,
    z_compr_level_mask = 0xf00,
    ctrl_esc = 0x1000,
    skip_ctrl_esc = 0x3000,
    invert_endian = 0x8000,
};
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(iomode);

enum class iostate_bits : std::uint8_t { good = 0, bad = 1, fail = 2, eof = 4 };
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(iostate_bits);

enum class seekdir : std::uint8_t { beg = 0, end, curr };
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(seekdir);

class UXS_EXPORT_ALL_STUFF_FOR_GNUC iobuf_error : public std::runtime_error {
 public:
    UXS_EXPORT explicit iobuf_error(const char* message);
    UXS_EXPORT explicit iobuf_error(const std::string& message);
    UXS_EXPORT const char* what() const noexcept override;
};

class iostate {
 public:
    iostate() noexcept = default;
    explicit iostate(iomode mode) noexcept : mode_(mode) {}
    iostate(iomode mode, iostate_bits state) noexcept : mode_(mode), state_(state) {}

    iomode mode() const noexcept { return mode_; }

    iostate_bits rdstate() const noexcept { return state_; }
    bool good() const noexcept { return state_ == iostate_bits::good; }
    bool bad() const noexcept { return !!(state_ & iostate_bits::bad); }
    bool fail() const noexcept { return !!(state_ & (iostate_bits::fail | iostate_bits::bad)); }
    bool eof() const noexcept { return !!(state_ & iostate_bits::eof); }
    operator bool() const noexcept { return !fail(); }
    bool operator!() const noexcept { return fail(); }

    void setstate(iostate_bits bits) { clear(state_ | bits); }
    void clear(iostate_bits bits = iostate_bits::good) {
        if (!!(bits & except_mask_)) { throw iobuf_error("iobuf error"); }
        state_ = bits;
    }

    iostate_bits exceptions() const noexcept { return except_mask_; }
    void exceptions(iostate_bits except_mask) noexcept { except_mask_ = except_mask; }

 protected:
    void reset_mode(iomode mode) noexcept { mode_ = mode; }
    void reset_state(iostate_bits bits) noexcept { state_ = bits; }

 private:
    iomode mode_ = iomode::none;
    iostate_bits state_ = iostate_bits::good;
    iostate_bits except_mask_ = iostate_bits::good;
};

namespace detail {
UXS_EXPORT iomode iomode_from_str(const char* mode, iomode default_mode) noexcept;
}

}  // namespace uxs
