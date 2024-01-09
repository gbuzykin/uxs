#pragma once

#include "devbuf.h"
#include "sysfile.h"

#include "uxs/chars.h"

namespace uxs {

template<typename CharT>
class basic_filebuf : public basic_devbuf<CharT> {
 public:
    basic_filebuf() : basic_devbuf<CharT>(file_) {}
    basic_filebuf(file_desc_t fd, iomode mode, basic_iobuf<CharT>* tie = nullptr)
        : basic_devbuf<CharT>(file_), file_(fd) {
        this->settie(tie);
        if (file_.valid()) { this->initbuf(mode); }
    }
    basic_filebuf(const char* fname, iomode mode) : basic_devbuf<CharT>(file_), file_(fname, mode) {
        if (file_.valid()) { this->initbuf(mode); }
    }
    basic_filebuf(const wchar_t* fname, iomode mode) : basic_devbuf<CharT>(file_), file_(fname, mode) {
        if (file_.valid()) { this->initbuf(mode); }
    }
    basic_filebuf(const char* fname, const char* mode)
        : basic_filebuf(fname,
                        detail::iomode_from_str(mode, is_character<CharT>::value ? iomode::text : iomode::none)) {}
    basic_filebuf(const wchar_t* fname, const char* mode)
        : basic_filebuf(fname,
                        detail::iomode_from_str(mode, is_character<CharT>::value ? iomode::text : iomode::none)) {}
    UXS_EXPORT ~basic_filebuf() override;
    UXS_EXPORT basic_filebuf(basic_filebuf&& other) noexcept;
    UXS_EXPORT basic_filebuf& operator=(basic_filebuf&& other);

    UXS_EXPORT void attach(file_desc_t fd, iomode mode);
    UXS_EXPORT file_desc_t detach();

    UXS_EXPORT bool open(const char* fname, iomode mode);
    UXS_EXPORT bool open(const wchar_t* fname, iomode mode);
    bool open(const char* fname, const char* mode) {
        return open(fname, detail::iomode_from_str(mode, is_character<CharT>::value ? iomode::text : iomode::none));
    }
    bool open(const wchar_t* fname, const char* mode) {
        return open(fname, detail::iomode_from_str(mode, is_character<CharT>::value ? iomode::text : iomode::none));
    }
    UXS_EXPORT void close();

 protected:
    UXS_EXPORT int sync() override { return basic_devbuf<CharT>::sync(); }

 private:
    sysfile file_;
};

using filebuf = basic_filebuf<char>;
using wfilebuf = basic_filebuf<wchar_t>;
using u8filebuf = basic_filebuf<uint8_t>;

}  // namespace uxs
