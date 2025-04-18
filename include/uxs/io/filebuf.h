#pragma once

#include "devbuf.h"
#include "sysfile.h"

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

    ~basic_filebuf() override { this->freebuf(); }

    basic_filebuf(basic_filebuf&& other) noexcept
        : basic_devbuf<CharT>(std::move(other)), file_(std::move(other.file_)) {
        this->setdev(&file_);
    }
    basic_filebuf& operator=(basic_filebuf&& other) noexcept {
        if (&other == this) { return *this; }
        basic_devbuf<CharT>::operator=(std::move(other));
        file_ = std::move(other.file_);
        this->setdev(&file_);
        return *this;
    }

    void attach(file_desc_t fd, iomode mode) {
        this->initbuf(mode);
        file_.attach(fd);
    }
    file_desc_t detach() noexcept {
        this->freebuf();
        return file_.detach();
    }

    bool open(const char* fname, iomode mode) {
        this->freebuf();
        const bool res = file_.open(fname, mode);
        if (res) { this->initbuf(mode); }
        return res;
    }
    bool open(const wchar_t* fname, iomode mode) {
        this->freebuf();
        const bool res = file_.open(fname, mode);
        if (res) { this->initbuf(mode); }
        return res;
    }
    bool open(const char* fname, const char* mode) {
        return open(fname, detail::iomode_from_str(mode, is_character<CharT>::value ? iomode::text : iomode::none));
    }
    bool open(const wchar_t* fname, const char* mode) {
        return open(fname, detail::iomode_from_str(mode, is_character<CharT>::value ? iomode::text : iomode::none));
    }
    void close() noexcept {
        this->freebuf();
        file_.close();
    }

 protected:
    int sync() override { return basic_devbuf<CharT>::sync(); }

 private:
    sysfile file_;
};

using filebuf = basic_filebuf<char>;
using wfilebuf = basic_filebuf<wchar_t>;
using u8filebuf = basic_filebuf<std::uint8_t>;

}  // namespace uxs
