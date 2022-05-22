#pragma once

#include "util/utility.h"

namespace util {

enum class iomode : uint16_t {
    kNone = 0,
    kIn = 1,
    kOut = 2,
    kTruncate = 4,
    kAppend = 8,
    kCreate = 0x10,
    kExcl = 0x20,
    kCrLf = 0x80,
#if defined(WIN32)
    kText = kCrLf,
#else   // defined(WIN32)
    kText = 0,
#endif  // defined(WIN32)
    kCtrlEsc = 0x100,
    kSkipCtrlEsc = 0x300,
};
UTIL_IMPLEMENT_BITWISE_OPS_FOR_ENUM(iomode, uint16_t);

enum class iostate_bits : uint8_t { kGood = 0, kBad = 1, kFail = 2, kEof = 4 };
UTIL_IMPLEMENT_BITWISE_OPS_FOR_ENUM(iostate_bits, uint8_t);

enum class seekdir : uint8_t { kBeg = 0, kEnd, kCurr };
UTIL_IMPLEMENT_BITWISE_OPS_FOR_ENUM(seekdir, uint8_t);

class iostate {
 public:
    iostate() = default;
    explicit iostate(iomode mode) : mode_(mode) {}
    iostate(iomode mode, iostate_bits state) : mode_(mode), state_(state) {}

    iomode mode() const { return mode_; }

    iostate_bits rdstate() const { return state_; }
    bool good() const { return state_ == iostate_bits::kGood; }
    bool bad() const { return !!(state_ & iostate_bits::kBad); }
    bool fail() const { return !!(state_ & (iostate_bits::kFail | iostate_bits::kBad)); }
    bool eof() const { return !!(state_ & iostate_bits::kEof); }
    operator bool() const { return !fail(); }
    bool operator!() const { return fail(); }

    void setstate(iostate_bits bits) { state_ |= bits; }
    void clear(iostate_bits bits = iostate_bits::kGood) { state_ = bits; }

 protected:
    void setmode(iomode mode) { mode_ = mode; }

 private:
    iomode mode_ = iomode::kNone;
    iostate_bits state_ = iostate_bits::kGood;
};

namespace detail {
UTIL_EXPORT iomode iomode_from_str(const char* mode, iomode def);
}

}  // namespace util
