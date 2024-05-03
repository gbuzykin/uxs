#pragma once

#include "common.h"

#include <array>

namespace uxs {

namespace detail {
// CRC32 lookup table
struct crc32_table_t {
    std::array<std::uint32_t, 256> v{};
    UXS_CONSTEXPR crc32_table_t() {
        // The official polynomial used by CRC-32 in PKZip, WinZip and Ethernet
        const std::uint32_t poly = 0x04c11db7;
        const std::uint32_t msbit = 0x80000000;
        for (unsigned i = 0; i < 256; ++i) {
            std::uint32_t r = reflect(i, 8) << 24;
            for (int j = 0; j < 8; ++j) { r = (r << 1) ^ (r & msbit ? poly : 0); }
            v[i] = reflect(r, 32);
        }
    }
    UXS_CONSTEXPR std::uint32_t reflect(std::uint32_t ref, unsigned ch) {
        std::uint32_t value = 0;
        for (unsigned i = 1; i < ch + 1; ++i) {  // Swap bit 0 for bit 7 bit 1 for bit 6, etc.
            if (ref & 1) { value |= 1 << (ch - i); }
            ref >>= 1;
        }
        return value;
    }
};
}  // namespace detail

class crc32 {
 public:
    template<typename InputIt>
    static UXS_CONSTEXPR std::uint32_t calc(InputIt it, InputIt end, std::uint32_t crc32 = 0xffffffff) noexcept {
        while (it != end) { crc32 = (crc32 >> 8) ^ table_.v[(crc32 & 0xff) ^ static_cast<std::uint8_t>(*it++)]; }
        return crc32;
    }
    static UXS_CONSTEXPR std::uint32_t calc(const char* zstr, std::uint32_t crc32 = 0xffffffff) noexcept {
        while (*zstr) { crc32 = (crc32 >> 8) ^ table_.v[(crc32 & 0xff) ^ static_cast<std::uint8_t>(*zstr++)]; }
        return crc32;
    }

 private:
#if __cplusplus < 201703L
    UXS_EXPORT static const detail::crc32_table_t table_;
#else   // __cplusplus < 201703L
    static constexpr detail::crc32_table_t table_{};
#endif  // __cplusplus < 201703L
};

}  // namespace uxs
