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
    sysfile() noexcept;
    explicit sysfile(file_desc_t fd) noexcept;

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

    bool valid() const noexcept;
    explicit operator bool() const noexcept { return valid(); }

    void attach(file_desc_t fd) noexcept;
    file_desc_t detach() noexcept;

    template<typename NameCharT>
    bool open(const NameCharT* fname, iomode mode);

    template<typename NameCharT>
    bool open(const NameCharT* fname, const char* mode) {
        return open(fname, detail::iomode_from_str(mode, iomode::none));
    }

    void close() noexcept;

    int read(void* data, std::size_t sz, std::size_t& n_read) override;
    int write(const void* data, std::size_t sz, std::size_t& n_written) override;
    std::int64_t seek(std::int64_t off, seekdir dir) override;
    int apply_ctrlesc(est::span<const std::uint8_t> v) override;
    int truncate() override;
    int flush() override;

 private:
    file_desc_t fd_;
};

}  // namespace uxs
