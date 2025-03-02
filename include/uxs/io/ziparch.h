#pragma once

#include "iostate.h"

namespace uxs {

class zipfile;

class ziparch {
 public:
    ziparch() noexcept = default;
    ziparch(const char* name, iomode mode) { open(name, mode); }
    ziparch(const wchar_t* name, iomode mode) { open(name, mode); }
    ziparch(const char* name, const char* mode) : ziparch(name, detail::iomode_from_str(mode, iomode::in)) {}
    ziparch(const wchar_t* name, const char* mode) : ziparch(name, detail::iomode_from_str(mode, iomode::in)) {}
    ~ziparch() { close(); }
    ziparch(ziparch&& other) noexcept : zip_(other.zip_) { other.zip_ = nullptr; }
    ziparch& operator=(ziparch&& other) noexcept {
        if (&other == this) { return *this; }
        zip_ = other.zip_;
        other.zip_ = nullptr;
        return *this;
    }

    bool valid() const noexcept { return zip_ != nullptr; }
    explicit operator bool() const noexcept { return zip_ != nullptr; }

    UXS_EXPORT bool open(const char* name, iomode mode);
    UXS_EXPORT bool open(const wchar_t* name, iomode mode);
    bool open(const char* name, const char* mode) { return open(name, detail::iomode_from_str(mode, iomode::in)); }
    bool open(const wchar_t* name, const char* mode) { return open(name, detail::iomode_from_str(mode, iomode::in)); }
    UXS_EXPORT void close() noexcept;

    UXS_EXPORT bool add_file(const char* fname, const void* buf, std::size_t sz);
    UXS_EXPORT bool add_file(const wchar_t* fname, const void* buf, std::size_t sz);

    UXS_EXPORT bool stat_size(const char* fname, std::uint64_t& sz);
    UXS_EXPORT bool stat_crc(const char* fname, std::uint32_t& crc);

    UXS_EXPORT bool stat_size(const wchar_t* fname, std::uint64_t& sz);
    UXS_EXPORT bool stat_crc(const wchar_t* fname, std::uint32_t& crc);

 private:
    friend class zipfile;
    void* zip_ = nullptr;
};

}  // namespace uxs
