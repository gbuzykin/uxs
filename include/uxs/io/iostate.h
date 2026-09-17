#pragma once

#include "uxs/utility.h"

#include <stdexcept>
#include <string>

namespace uxs {

enum class iomode : std::uint16_t {
    none = 0,
    in = est::bit(0),
    out = est::bit(1),
    truncate = est::bit(2),
    append = est::bit(3),
    create = est::bit(4),
    exclusive = est::bit(5),
    cr_lf = est::bit(6),
#if defined(WIN32)
    text = cr_lf,
#else   // defined(WIN32)
    text = 0,
#endif  // defined(WIN32)
    z_compr = est::bit(7),
    z_compr_level = est::bit(8),
    z_compr_level_mask = est::genmask(11, 8),
    ctrl_esc = est::bit(12),
    skip_ctrl_esc = est::bit(13) | est::bit(12),
    invert_endian = est::bit(15),
};
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(iomode);

enum class iostate_bits : std::uint8_t {
    good = 0,
    bad = est::bit(0),
    fail = est::bit(1),
    eof = est::bit(2),
};
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
    explicit operator bool() const noexcept { return !fail(); }
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
