#pragma once

#include "iodevice.h"

namespace uxs {

#if defined(WIN32)
using file_desc_t = void*;
#elif defined(__linux__)
using file_desc_t = int;
#endif

class UXS_EXPORT_ALL_STUFF_FOR_GNUC sysfile : public iodevice {
 public:
    UXS_EXPORT sysfile() noexcept;
    UXS_EXPORT explicit sysfile(file_desc_t fd) noexcept;
    sysfile(const char* fname, iomode mode) : sysfile() { open(fname, mode); }
    sysfile(const wchar_t* fname, iomode mode) : sysfile() { open(fname, mode); }
    sysfile(const char* fname, const char* mode) : sysfile() {
        open(fname, detail::iomode_from_str(mode, iomode::none));
    }
    sysfile(const wchar_t* fname, const char* mode) : sysfile() {
        open(fname, detail::iomode_from_str(mode, iomode::none));
    }
    UXS_EXPORT ~sysfile() override;
    sysfile(sysfile&& other) noexcept : fd_(other.detach()) {}
    sysfile& operator=(sysfile&& other) noexcept {
        if (&other == this) { return *this; }
        attach(other.detach());
        return *this;
    }

    UXS_EXPORT bool valid() const noexcept;
    explicit operator bool() const noexcept { return valid(); }

    UXS_EXPORT void attach(file_desc_t fd) noexcept;
    UXS_EXPORT file_desc_t detach() noexcept;

    UXS_EXPORT bool open(const char* fname, iomode mode);
    UXS_EXPORT bool open(const wchar_t* fname, iomode mode);
    bool open(const char* fname, const char* mode) { return open(fname, detail::iomode_from_str(mode, iomode::none)); }
    bool open(const wchar_t* fname, const char* mode) {
        return open(fname, detail::iomode_from_str(mode, iomode::none));
    }
    UXS_EXPORT void close() noexcept;

    UXS_EXPORT int read(void* buf, std::size_t sz, std::size_t& n_read) override;
    UXS_EXPORT int write(const void* buf, std::size_t sz, std::size_t& n_written) override;
    UXS_EXPORT std::int64_t seek(std::int64_t off, seekdir dir) override;
    UXS_EXPORT int ctrlesc_color(uxs::span<const std::uint8_t> v) override;
    UXS_EXPORT int flush() override;

    UXS_EXPORT static bool remove(const char* fname);
    UXS_EXPORT static bool remove(const wchar_t* fname);

 private:
    file_desc_t fd_;
};

}  // namespace uxs
