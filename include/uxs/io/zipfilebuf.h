#pragma once

#include "devbuf.h"
#include "zipfile.h"

#include "uxs/chars.h"

namespace uxs {

template<typename CharT>
class basic_zipfilebuf : public basic_devbuf<CharT> {
 public:
    basic_zipfilebuf() : basic_devbuf<CharT>(zip_file_) {}
    basic_zipfilebuf(ziparch& arch, const char* fname,
                     iomode mode = is_character<CharT>::value ? iomode::text | iomode::in : iomode::in)
        : basic_devbuf<CharT>(zip_file_), zip_file_(arch, fname) {
        if (zip_file_.valid()) { this->initbuf(mode); }
    }
    basic_zipfilebuf(ziparch& arch, const wchar_t* fname,
                     iomode mode = is_character<CharT>::value ? iomode::text | iomode::in : iomode::in)
        : basic_devbuf<CharT>(zip_file_), zip_file_(arch, fname) {
        if (zip_file_.valid()) { this->initbuf(mode); }
    }
    UXS_EXPORT ~basic_zipfilebuf() override;
    UXS_EXPORT basic_zipfilebuf(basic_zipfilebuf&& other) noexcept;
    UXS_EXPORT basic_zipfilebuf& operator=(basic_zipfilebuf&& other);

    UXS_EXPORT bool open(ziparch& arch, const char* fname,
                         iomode mode = is_character<CharT>::value ? iomode::text | iomode::in : iomode::in);
    UXS_EXPORT bool open(ziparch& arch, const wchar_t* fname,
                         iomode mode = is_character<CharT>::value ? iomode::text | iomode::in : iomode::in);
    UXS_EXPORT void close();

 private:
    zipfile zip_file_;
};

using zipfilebuf = basic_zipfilebuf<char>;
using wzipfilebuf = basic_zipfilebuf<wchar_t>;
using u8zipfilebuf = basic_zipfilebuf<uint8_t>;

}  // namespace uxs
