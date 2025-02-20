#pragma once

#include "filebuf.h"

namespace uxs {

template<typename CharT>
basic_filebuf<CharT>::~basic_filebuf() {
    this->freebuf();
}

template<typename CharT>
basic_filebuf<CharT>::basic_filebuf(basic_filebuf&& other) noexcept
    : basic_devbuf<CharT>(std::move(other)), file_(std::move(other.file_)) {
    this->setdev(&file_);
}

template<typename CharT>
basic_filebuf<CharT>& basic_filebuf<CharT>::operator=(basic_filebuf&& other) noexcept {
    if (&other == this) { return *this; }
    basic_devbuf<CharT>::operator=(std::move(other));
    file_ = std::move(other.file_);
    this->setdev(&file_);
    return *this;
}

template<typename CharT>
void basic_filebuf<CharT>::attach(file_desc_t fd, iomode mode) {
    this->initbuf(mode);
    file_.attach(fd);
}

template<typename CharT>
file_desc_t basic_filebuf<CharT>::detach() noexcept {
    this->freebuf();
    return file_.detach();
}

template<typename CharT>
bool basic_filebuf<CharT>::open(const char* fname, iomode mode) {
    this->freebuf();
    const bool res = file_.open(fname, mode);
    if (res) { this->initbuf(mode); }
    return res;
}

template<typename CharT>
bool basic_filebuf<CharT>::open(const wchar_t* fname, iomode mode) {
    this->freebuf();
    const bool res = file_.open(fname, mode);
    if (res) { this->initbuf(mode); }
    return res;
}

template<typename CharT>
void basic_filebuf<CharT>::close() noexcept {
    this->freebuf();
    file_.close();
}

}  // namespace uxs
