#pragma once

#include "iodevice.h"

namespace uxs {

#if defined(WIN32)
using file_desc_t = void*;
#elif defined(__linux__)
using file_desc_t = int;
#endif

class UXS_EXPORT sysfile : public iodevice {
 public:
    sysfile();
    explicit sysfile(file_desc_t fd);
    sysfile(const char* fname, iomode mode) : sysfile() { open(fname, mode); }
    sysfile(const wchar_t* fname, iomode mode) : sysfile() { open(fname, mode); }
    sysfile(const char* fname, const char* mode) : sysfile() {
        open(fname, detail::iomode_from_str(mode, iomode::kNone));
    }
    sysfile(const wchar_t* fname, const char* mode) : sysfile() {
        open(fname, detail::iomode_from_str(mode, iomode::kNone));
    }
    ~sysfile() override;
    sysfile(sysfile&& other) NOEXCEPT : fd_(other.detach()) {}
    sysfile& operator=(sysfile&& other) {
        if (&other == this) { return *this; }
        attach(other.detach());
        return *this;
    }

    bool valid() const;
    explicit operator bool() const { return valid(); }

    void attach(file_desc_t fd);
    file_desc_t detach();

    bool open(const char* fname, iomode mode);
    bool open(const wchar_t* fname, iomode mode);
    bool open(const char* fname, const char* mode) { return open(fname, detail::iomode_from_str(mode, iomode::kNone)); }
    bool open(const wchar_t* fname, const char* mode) {
        return open(fname, detail::iomode_from_str(mode, iomode::kNone));
    }
    void close();

    int read(void* buf, size_t sz, size_t& n_read) override;
    int write(const void* buf, size_t sz, size_t& n_written) override;
    int64_t seek(int64_t off, seekdir dir) override;
    int ctrlesc_color(uxs::span<uint8_t> v) override;
    int flush() override;

    static bool remove(const char* fname);
    static bool remove(const wchar_t* fname);

 private:
    file_desc_t fd_;
};

}  // namespace uxs
