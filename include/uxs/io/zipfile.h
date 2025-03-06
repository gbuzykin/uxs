#pragma once

#include "iodevice.h"
#include "ziparch.h"

#include <string>

namespace uxs {

class UXS_EXPORT_ALL_STUFF_FOR_GNUC zipfile : public iodevice {
 public:
    zipfile() noexcept = default;
    zipfile(ziparch& arch, const char* fname, iomode mode) { open(arch, fname, mode); }
    zipfile(ziparch& arch, const wchar_t* fname, iomode mode) { open(arch, fname, mode); }
    zipfile(ziparch& arch, const char* fname, const char* mode) {
        open(arch, fname, detail::iomode_from_str(mode, iomode::none));
    }
    zipfile(ziparch& arch, const wchar_t* fname, const char* mode) {
        open(arch, fname, detail::iomode_from_str(mode, iomode::none));
    }
    ~zipfile() override { close(); }
    zipfile(zipfile&& other) noexcept
        : mode_(other.mode_), fname_(std::move(other.fname_)), zip_arch_(other.zip_arch_), zip_fd_(other.zip_fd_) {
        other.zip_arch_ = nullptr;
        other.zip_fd_ = nullptr;
    }
    zipfile& operator=(zipfile&& other) noexcept {
        if (&other == this) { return *this; }
        mode_ = other.mode_;
        fname_ = std::move(other.fname_);
        zip_arch_ = other.zip_arch_;
        zip_fd_ = other.zip_fd_;
        other.zip_arch_ = nullptr;
        other.zip_fd_ = nullptr;
        return *this;
    }

    bool valid() const noexcept { return zip_fd_ != nullptr; }
    explicit operator bool() const noexcept { return zip_fd_ != nullptr; }

    UXS_EXPORT bool open(ziparch& arch, const char* fname, iomode mode);
    UXS_EXPORT bool open(ziparch& arch, const wchar_t* fname, iomode mode);
    bool open(ziparch& arch, const char* fname, const char* mode) {
        return open(arch, fname, detail::iomode_from_str(mode, iomode::none));
    }
    bool open(ziparch& arch, const wchar_t* fname, const char* mode) {
        return open(arch, fname, detail::iomode_from_str(mode, iomode::none));
    }
    UXS_EXPORT void close() noexcept;

    UXS_EXPORT int read(void* data, std::size_t sz, std::size_t& n_read) override;
    UXS_EXPORT int write(const void* data, std::size_t sz, std::size_t& n_written) override;
    int flush() override { return -1; }

 private:
    iomode mode_ = iomode::none;
    std::string fname_;
    void* zip_arch_ = nullptr;
    void* zip_fd_ = nullptr;
};

}  // namespace uxs
