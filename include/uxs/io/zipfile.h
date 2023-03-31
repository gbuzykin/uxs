#pragma once

#include "iodevice.h"
#include "ziparch.h"

namespace uxs {

class UXS_EXPORT zipfile : public iodevice {
 public:
    zipfile() = default;
    zipfile(ziparch& arch, const char* fname) { open(arch, fname); }
    zipfile(ziparch& arch, const wchar_t* fname) { open(arch, fname); }
    ~zipfile() { close(); }
    zipfile(zipfile&& other) NOEXCEPT : zip_fd_(other.zip_fd_) { other.zip_fd_ = nullptr; }
    zipfile& operator=(zipfile&& other) {
        if (&other == this) { return *this; }
        zip_fd_ = other.zip_fd_;
        other.zip_fd_ = nullptr;
        return *this;
    }

    bool valid() const { return zip_fd_ != nullptr; }
    explicit operator bool() const { return zip_fd_ != nullptr; }

    bool open(ziparch& arch, const char* fname);
    bool open(ziparch& arch, const wchar_t* fname);
    void close();

    int read(void* buf, size_t sz, size_t& n_read) override;
    int write(const void* data, size_t sz, size_t& n_written) override { return -1; }
    int flush() override { return -1; }

 private:
    void* zip_fd_ = nullptr;
};

}  // namespace uxs
