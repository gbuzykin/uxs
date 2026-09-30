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

    template<typename NameCharT>
    sysfile(const NameCharT* fname, iomode mode) : sysfile() {
        open(fname, mode);
    }

    template<typename NameCharT>
    sysfile(const NameCharT* fname, const char* mode) : sysfile() {
        open(fname, mode);
    }

    ~sysfile() override { close(); }
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

    template<typename NameCharT>
    UXS_EXPORT bool open(const NameCharT* fname, iomode mode);

    template<typename NameCharT>
    bool open(const NameCharT* fname, const char* mode) {
        return open(fname, detail::iomode_from_str(mode, iomode::none));
    }

    UXS_EXPORT void close() noexcept;

    UXS_EXPORT int read(void* data, std::size_t sz, std::size_t& n_read) override;
    UXS_EXPORT int write(const void* data, std::size_t sz, std::size_t& n_written) override;
    UXS_EXPORT std::int64_t seek(std::int64_t off, seekdir dir) override;
    UXS_EXPORT int ctrlesc_color(est::span<const std::uint8_t> v) override;
    UXS_EXPORT int truncate() override;
    UXS_EXPORT int flush() override;

    template<typename NameCharT>
    UXS_EXPORT static bool remove(const NameCharT* fname);

 private:
    file_desc_t fd_;
};

}  // namespace uxs
