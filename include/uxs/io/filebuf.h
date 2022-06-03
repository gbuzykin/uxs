#pragma once

#include "uxs/chars.h"

#include "devbuf.h"
#include "sysfile.h"

namespace uxs {

template<typename CharT>
class UXS_EXPORT basic_filebuf : public basic_devbuf<CharT> {
 public:
    basic_filebuf() : basic_devbuf<CharT>(file_) {}
    basic_filebuf(file_desc_t fd, iomode mode, basic_iobuf<CharT>* tie = nullptr);
    basic_filebuf(const char* fname, iomode mode);
    basic_filebuf(const wchar_t* fname, iomode mode);
    basic_filebuf(const char* fname, const char* mode)
        : basic_filebuf(fname,
                        detail::iomode_from_str(mode, is_character<CharT>::value ? iomode::kText : iomode::kNone)) {}
    basic_filebuf(const wchar_t* fname, const char* mode)
        : basic_filebuf(fname,
                        detail::iomode_from_str(mode, is_character<CharT>::value ? iomode::kText : iomode::kNone)) {}
    ~basic_filebuf() override;
    basic_filebuf(basic_filebuf&& other) NOEXCEPT;
    basic_filebuf& operator=(basic_filebuf&& other) NOEXCEPT;

    void attach(file_desc_t fd, iomode mode);
    file_desc_t detach();

    bool open(const char* fname, iomode mode);
    bool open(const wchar_t* fname, iomode mode);
    bool open(const char* fname, const char* mode) {
        return open(fname, detail::iomode_from_str(mode, is_character<CharT>::value ? iomode::kText : iomode::kNone));
    }
    bool open(const wchar_t* fname, const char* mode) {
        return open(fname, detail::iomode_from_str(mode, is_character<CharT>::value ? iomode::kText : iomode::kNone));
    }
    void close();

 private:
    sysfile file_;
};

using filebuf = basic_filebuf<char>;
using wfilebuf = basic_filebuf<wchar_t>;
using u8filebuf = basic_filebuf<uint8_t>;

}  // namespace uxs
