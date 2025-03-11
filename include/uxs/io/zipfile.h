#pragma once

#include "iodevice.h"
#include "ziparch.h"

namespace uxs {

class UXS_EXPORT_ALL_STUFF_FOR_GNUC zipfile : public iodevice {
 public:
    zipfile() noexcept = default;
    zipfile(ziparch& arch, const char* fname, iomode mode) { open(arch, fname, mode); }
    zipfile(ziparch& arch, const wchar_t* fname, iomode mode) { open(arch, fname, mode); }
    zipfile(ziparch& arch, std::uint64_t index, iomode mode) { open(arch, index, mode); }
    zipfile(ziparch& arch, const char* fname, const char* mode) { open(arch, fname, mode); }
    zipfile(ziparch& arch, const wchar_t* fname, const char* mode) { open(arch, fname, mode); }
    zipfile(ziparch& arch, std::uint64_t index, const char* mode) { open(arch, index, mode); }
    ~zipfile() override { close(); }
    zipfile(zipfile&& other) noexcept : mode_(other.mode_), zip_fdesc_(other.zip_fdesc_) { other.zip_fdesc_ = nullptr; }
    zipfile& operator=(zipfile&& other) noexcept {
        if (&other == this) { return *this; }
        mode_ = other.mode_, zip_fdesc_ = other.zip_fdesc_;
        other.zip_fdesc_ = nullptr;
        return *this;
    }

    bool valid() const noexcept { return zip_fdesc_ != nullptr; }
    explicit operator bool() const noexcept { return zip_fdesc_ != nullptr; }

    UXS_EXPORT bool open(ziparch& arch, const char* fname, iomode mode);
    UXS_EXPORT bool open(ziparch& arch, const wchar_t* fname, iomode mode);
    UXS_EXPORT bool open(ziparch& arch, std::uint64_t index, iomode mode);
    bool open(ziparch& arch, const char* fname, const char* mode) {
        return open(arch, fname, detail::iomode_from_str(mode, iomode::none));
    }
    bool open(ziparch& arch, const wchar_t* fname, const char* mode) {
        return open(arch, fname, detail::iomode_from_str(mode, iomode::none));
    }
    bool open(ziparch& arch, std::uint64_t index, const char* mode) {
        return open(arch, index, detail::iomode_from_str(mode, iomode::none));
    }
    UXS_EXPORT void set_compression(zipfile_compression compr, unsigned level = 0);
    UXS_EXPORT void close() noexcept;

    UXS_EXPORT int read(void* data, std::size_t sz, std::size_t& n_read) override;
    UXS_EXPORT int write(const void* data, std::size_t sz, std::size_t& n_written) override;
    int flush() override { return -1; }

 private:
    struct writing_desc_t {
#if __cplusplus < 201703L
        writing_desc_t(std::string fname, void* zip_arch, void* zip_source)
            : fname(std::move(fname)), zip_arch(zip_arch), zip_source(zip_source) {}
#endif  // __cplusplus < 201703L
        std::string fname;
        void* zip_arch = nullptr;
        void* zip_source = nullptr;
        zipfile_compression zip_compr = zipfile_compression::deflate;
        unsigned zip_compr_level = 0;
    };

    iomode mode_ = iomode::none;
    void* zip_fdesc_ = nullptr;
};

}  // namespace uxs
