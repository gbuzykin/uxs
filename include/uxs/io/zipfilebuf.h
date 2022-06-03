#pragma once

#include "uxs/chars.h"

#include "devbuf.h"
#include "zipfile.h"

namespace uxs {

template<typename CharT>
class UXS_EXPORT basic_zipfilebuf : public basic_devbuf<CharT> {
 public:
    basic_zipfilebuf() : basic_devbuf<CharT>(zip_file_) {}
    basic_zipfilebuf(ziparch& arch, const char* fname,
                     iomode mode = is_character<CharT>::value ? iomode::kText | iomode::kIn : iomode::kIn);
    basic_zipfilebuf(ziparch& arch, const wchar_t* fname,
                     iomode mode = is_character<CharT>::value ? iomode::kText | iomode::kIn : iomode::kIn);
    ~basic_zipfilebuf() override;
    basic_zipfilebuf(basic_zipfilebuf&& other) NOEXCEPT;
    basic_zipfilebuf& operator=(basic_zipfilebuf&& other) NOEXCEPT;

    bool open(ziparch& arch, const char* fname,
              iomode mode = is_character<CharT>::value ? iomode::kText | iomode::kIn : iomode::kIn);
    bool open(ziparch& arch, const wchar_t* fname,
              iomode mode = is_character<CharT>::value ? iomode::kText | iomode::kIn : iomode::kIn);
    void close();

 private:
    zipfile zip_file_;
};

using zipfilebuf = basic_zipfilebuf<char>;
using wzipfilebuf = basic_zipfilebuf<wchar_t>;
using u8zipfilebuf = basic_zipfilebuf<uint8_t>;

}  // namespace uxs
