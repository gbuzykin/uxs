#pragma once

#include "devbuf.h"
#include "zipfile.h"

#include "uxs/chars.h"

namespace uxs {

template<typename CharT>
class basic_zipfilebuf : public basic_devbuf<CharT> {
 public:
    basic_zipfilebuf() : basic_devbuf<CharT>(zip_file_) {}
    basic_zipfilebuf(ziparch& arch, const char* fname, iomode mode)
        : basic_devbuf<CharT>(zip_file_), zip_file_(arch, fname, mode) {
        if (zip_file_.valid()) { this->initbuf(mode); }
    }
    basic_zipfilebuf(ziparch& arch, const wchar_t* fname, iomode mode)
        : basic_devbuf<CharT>(zip_file_), zip_file_(arch, fname, mode) {
        if (zip_file_.valid()) { this->initbuf(mode); }
    }
    basic_zipfilebuf(ziparch& arch, const char* fname, const char* mode)
        : basic_zipfilebuf(arch, fname,
                           detail::iomode_from_str(mode, is_character<CharT>::value ? iomode::text : iomode::none)) {}
    basic_zipfilebuf(ziparch& arch, const wchar_t* fname, const char* mode)
        : basic_zipfilebuf(arch, fname,
                           detail::iomode_from_str(mode, is_character<CharT>::value ? iomode::text : iomode::none)) {}
    UXS_EXPORT ~basic_zipfilebuf() override;
    UXS_EXPORT basic_zipfilebuf(basic_zipfilebuf&& other) noexcept;
    UXS_EXPORT basic_zipfilebuf& operator=(basic_zipfilebuf&& other) noexcept;

    UXS_EXPORT bool open(ziparch& arch, const char* fname, iomode mode);
    UXS_EXPORT bool open(ziparch& arch, const wchar_t* fname, iomode mode);
    bool open(ziparch& arch, const char* fname, const char* mode) {
        return open(arch, fname,
                    detail::iomode_from_str(mode, is_character<CharT>::value ? iomode::text : iomode::none));
    }
    bool open(ziparch& arch, const wchar_t* fname, const char* mode) {
        return open(arch, fname,
                    detail::iomode_from_str(mode, is_character<CharT>::value ? iomode::text : iomode::none));
    }
    UXS_EXPORT void close() noexcept;

 private:
    zipfile zip_file_;
};

using zipfilebuf = basic_zipfilebuf<char>;
using wzipfilebuf = basic_zipfilebuf<wchar_t>;
using u8zipfilebuf = basic_zipfilebuf<std::uint8_t>;

}  // namespace uxs
