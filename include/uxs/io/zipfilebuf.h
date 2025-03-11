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

    bool open(ziparch& arch, const char* fname, iomode mode) {
        this->freebuf();
        const bool res = zip_file_.open(arch, fname, mode);
        if (res) { this->initbuf(mode); }
        return res;
    }
    bool open(ziparch& arch, const wchar_t* fname, iomode mode) {
        this->freebuf();
        const bool res = zip_file_.open(arch, fname, mode);
        if (res) { this->initbuf(mode); }
        return res;
    }
    bool open(ziparch& arch, const char* fname, const char* mode) {
        return open(arch, fname,
                    detail::iomode_from_str(mode, is_character<CharT>::value ? iomode::text : iomode::none));
    }
    bool open(ziparch& arch, const wchar_t* fname, const char* mode) {
        return open(arch, fname,
                    detail::iomode_from_str(mode, is_character<CharT>::value ? iomode::text : iomode::none));
    }
    void set_compression(zipfile_compression compr, unsigned level = 0) { zip_file_.set_compression(compr, level); }
    void close() noexcept {
        this->freebuf();
        zip_file_.close();
    }

 private:
    zipfile zip_file_;
};

using zipfilebuf = basic_zipfilebuf<char>;
using wzipfilebuf = basic_zipfilebuf<wchar_t>;
using u8zipfilebuf = basic_zipfilebuf<std::uint8_t>;

}  // namespace uxs
