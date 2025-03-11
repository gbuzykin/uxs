#pragma once

#include "iostate.h"

#include <string>

namespace uxs {

enum class zipfile_compression {
    deflate = 0,
    store,
};

struct zipfile_info {
    std::string name;
    std::uint64_t index = 0;
    std::uint64_t size = 0;
    std::uint32_t crc = 0;
};

class zipfile;

class ziparch {
 public:
    ziparch() noexcept = default;
    ziparch(const char* name, iomode mode) { open(name, mode); }
    ziparch(const wchar_t* name, iomode mode) { open(name, mode); }
    ziparch(const char* name, const char* mode) { open(name, mode); }
    ziparch(const wchar_t* name, const char* mode) { open(name, mode); }
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

    UXS_EXPORT std::int64_t add_file(const char* fname, const void* data, std::size_t sz,
                                     zipfile_compression compr = zipfile_compression::deflate, unsigned level = 0);
    UXS_EXPORT std::int64_t add_file(const wchar_t* fname, const void* data, std::size_t sz,
                                     zipfile_compression compr = zipfile_compression::deflate, unsigned level = 0);

    UXS_EXPORT bool stat_file(const char* fname, zipfile_info& info) const;
    UXS_EXPORT bool stat_file(const wchar_t* fname, zipfile_info& info) const;
    UXS_EXPORT bool stat_file(std::uint64_t index, zipfile_info& info) const;

 private:
    friend class zipfile;
    void* zip_ = nullptr;
};

}  // namespace uxs
