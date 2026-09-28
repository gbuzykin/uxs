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

    template<typename NameCharT>
    basic_filebuf(const NameCharT* fname, iomode mode) : basic_devbuf<CharT>(file_), file_(fname, mode) {
        if (file_.valid()) { this->initbuf(mode); }
    }

    template<typename NameCharT>
    basic_filebuf(const NameCharT* fname, const char* mode)
        : basic_filebuf(fname,
                        detail::iomode_from_str(mode, est::is_character<CharT>::value ? iomode::text : iomode::none)) {}

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

    template<typename NameCharT>
    bool open(const NameCharT* fname, iomode mode) {
        this->freebuf();
        const bool res = file_.open(fname, mode);
        if (res) { this->initbuf(mode); }
        return res;
    }

    template<typename NameCharT>
    bool open(const NameCharT* fname, const char* mode) {
        return open(fname, detail::iomode_from_str(mode, est::is_character<CharT>::value ? iomode::text : iomode::none));
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

#define UXS_DECLARE_TYPE_ALIASES(type, prefix) using prefix##filebuf = basic_filebuf<type>
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
