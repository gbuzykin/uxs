#pragma once

#include "iostate.h"

#include <cstdlib>
#include <string>

namespace uxs {

enum class zipfile_compression {
    deflate = 0,
    store,
};

struct zip_sourced_t {
    explicit zip_sourced_t() = default;
};

#if __cplusplus >= 201703L
constexpr zip_sourced_t zip_sourced{};
#endif  // __cplusplus >= 201703L

struct zipfile_info {
    std::string name;
    std::uint64_t index = 0;
    std::uint64_t size = 0;
    std::uint32_t crc = 0;
};

class ziparch;
class zipfile;

class ziparch_source {
 public:
    ziparch_source() noexcept = default;
    ziparch_source(ziparch_source&& other) noexcept : data_(other.data_), size_(other.size_) { other.data_ = nullptr; }
    ziparch_source& operator=(ziparch_source&& other) noexcept {
        if (&other == this) { return *this; }
        data_ = other.data_, size_ = other.size_;
        other.data_ = nullptr;
        return *this;
    }
    ~ziparch_source() { std::free(data_); }
    const void* data() const noexcept { return data_; }
    std::size_t size() const noexcept { return size_; }

 private:
    friend class ziparch;

    void* data_ = nullptr;
    std::size_t size_ = 0;
};

class ziparch {
 public:
    ziparch() noexcept = default;
    ziparch(const char* name, iomode mode) { open(name, mode); }
    ziparch(const wchar_t* name, iomode mode) { open(name, mode); }
    ziparch(zip_sourced_t, const void* data, std::size_t sz) { open_sourced(data, sz); }
    ziparch(zip_sourced_t) { open_sourced(); }
    ziparch(const char* name, const char* mode) { open(name, mode); }
    ziparch(const wchar_t* name, const char* mode) { open(name, mode); }
    ~ziparch() { close(); }
    ziparch(ziparch&& other) noexcept : zip_(other.zip_), zip_source_(other.zip_source_) {
        other.zip_ = other.zip_source_ = nullptr;
    }
    ziparch& operator=(ziparch&& other) noexcept {
        if (&other == this) { return *this; }
        zip_ = other.zip_, zip_source_ = other.zip_source_;
        other.zip_ = other.zip_source_ = nullptr;
        return *this;
    }

    bool valid() const noexcept { return zip_ != nullptr; }
    explicit operator bool() const noexcept { return zip_ != nullptr; }

    UXS_EXPORT bool open(const char* name, iomode mode);
    UXS_EXPORT bool open(const wchar_t* name, iomode mode);
    UXS_EXPORT bool open_sourced(const void* data, std::size_t sz);
    bool open_sourced() { return open_sourced(nullptr, 0); }
    bool open(const char* name, const char* mode) { return open(name, detail::iomode_from_str(mode, iomode::in)); }
    bool open(const wchar_t* name, const char* mode) { return open(name, detail::iomode_from_str(mode, iomode::in)); }
    UXS_EXPORT ziparch_source release_source();
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
    void* zip_source_ = nullptr;
};

}  // namespace uxs
