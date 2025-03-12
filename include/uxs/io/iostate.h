#pragma once

#include "uxs/utility.h"

namespace uxs {

enum class iomode : std::uint16_t {
    none = 0,
    in = 1,
    out = 2,
    truncate = 4,
    append = 8,
    create = 0x10,
    exclusive = 0x20,
    z_compr = 0x40,
    cr_lf = 0x80,
#if defined(WIN32)
    text = cr_lf,
#else   // defined(WIN32)
    text = 0,
#endif  // defined(WIN32)
    ctrl_esc = 0x100,
    skip_ctrl_esc = 0x300,
    invert_endian = 0x8000,
};
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(iomode);

enum class iostate_bits : std::uint8_t { good = 0, bad = 1, fail = 2, eof = 4 };
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(iostate_bits);

enum class seekdir : std::uint8_t { beg = 0, end, curr };
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(seekdir);

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

    void setstate(iostate_bits bits) noexcept { state_ |= bits; }
    void clear(iostate_bits bits = iostate_bits::good) noexcept { state_ = bits; }

 protected:
    void setmode(iomode mode) noexcept { mode_ = mode; }

 private:
    iomode mode_ = iomode::none;
    iostate_bits state_ = iostate_bits::good;
};

namespace detail {
UXS_EXPORT iomode iomode_from_str(const char* mode, iomode default_mode) noexcept;
}

}  // namespace uxs
