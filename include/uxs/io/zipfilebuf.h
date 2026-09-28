#pragma once

#include "devbuf.h"
#include "zipfile.h"

namespace uxs {

template<typename CharT>
class basic_zipfilebuf : public basic_devbuf<CharT> {
 public:
    basic_zipfilebuf() : basic_devbuf<CharT>(zip_file_) {}

    template<typename NameCharT>
    basic_zipfilebuf(ziparch& arch, const NameCharT* fname, iomode mode)
        : basic_devbuf<CharT>(zip_file_), zip_file_(arch, fname, mode) {
        if (zip_file_.valid()) { this->initbuf(mode); }
    }

    template<typename NameCharT>
    basic_zipfilebuf(ziparch& arch, const NameCharT* fname, const char* mode)
        : basic_zipfilebuf(
              arch, fname,
              detail::iomode_from_str(mode, est::is_character<CharT>::value ? iomode::text : iomode::none)) {}

    ~basic_zipfilebuf() override { this->freebuf(); }

    basic_zipfilebuf(basic_zipfilebuf&& other) noexcept
        : basic_devbuf<CharT>(std::move(other)), zip_file_(std::move(other.zip_file_)) {
        this->setdev(&zip_file_);
    }
    basic_zipfilebuf& operator=(basic_zipfilebuf&& other) noexcept {
        if (&other == this) { return *this; }
        basic_devbuf<CharT>::operator=(std::move(other));
        zip_file_ = std::move(other.zip_file_);
        this->setdev(&zip_file_);
        return *this;
    }

    template<typename NameCharT>
    bool open(ziparch& arch, const NameCharT* fname, iomode mode) {
        this->freebuf();
        const bool res = zip_file_.open(arch, fname, mode);
        if (res) { this->initbuf(mode); }
        return res;
    }

    template<typename NameCharT>
    bool open(ziparch& arch, const NameCharT* fname, const char* mode) {
        return open(arch, fname,
                    detail::iomode_from_str(mode, est::is_character<CharT>::value ? iomode::text : iomode::none));
    }

    void set_compression(zipfile_compression compr, unsigned level = 0) { zip_file_.set_compression(compr, level); }

    void close() noexcept {
        this->freebuf();
        zip_file_.close();
    }

 private:
    zipfile zip_file_;
};

#define UXS_DECLARE_TYPE_ALIASES(type, prefix) using prefix##zipfilebuf = basic_zipfilebuf<type>
UXS_DECLARE_TYPE_ALIASES(char, );
#if UXS_USE_WCHAR_T != 0
UXS_DECLARE_TYPE_ALIASES(wchar_t, w);
#endif  // UXS_USE_WCHAR_T != 0
#if UXS_USE_CHAR8_T != 0
UXS_DECLARE_TYPE_ALIASES(char8_t, u8);
#endif  // UXS_USE_CHAR8_T != 0
UXS_DECLARE_TYPE_ALIASES(char16_t, u16);
UXS_DECLARE_TYPE_ALIASES(char32_t, u32);
UXS_DECLARE_TYPE_ALIASES(std::uint8_t, b);
#undef UXS_DECLARE_TYPE_ALIASES

}  // namespace uxs
