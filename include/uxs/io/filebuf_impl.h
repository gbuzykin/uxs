#pragma once

#include "uxs/io/filebuf.h"

namespace uxs {

template<typename CharT>
basic_filebuf<CharT>::basic_filebuf(file_desc_t fd, iomode mode, basic_iobuf<CharT>* tie)
    : basic_devbuf<CharT>(file_), file_(fd) {
    this->settie(tie);
    if (file_.valid()) { this->initbuf(mode); }
}

template<typename CharT>
basic_filebuf<CharT>::basic_filebuf(const char* fname, iomode mode) : basic_devbuf<CharT>(file_), file_(fname, mode) {
    if (file_.valid()) { this->initbuf(mode); }
}

template<typename CharT>
basic_filebuf<CharT>::basic_filebuf(const wchar_t* fname, iomode mode)
    : basic_devbuf<CharT>(file_), file_(fname, mode) {
    if (file_.valid()) { this->initbuf(mode); }
}

template<typename CharT>
basic_filebuf<CharT>::~basic_filebuf() {
    this->freebuf();
}

template<typename CharT>
basic_filebuf<CharT>::basic_filebuf(basic_filebuf&& other) NOEXCEPT : basic_devbuf<CharT>(std::move(other)),
                                                                      file_(std::move(other.file_)) {
    this->setdev(&file_);
}

template<typename CharT>
basic_filebuf<CharT>& basic_filebuf<CharT>::operator=(basic_filebuf&& other) {
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
file_desc_t basic_filebuf<CharT>::detach() {
    this->freebuf();
    return file_.detach();
}

template<typename CharT>
bool basic_filebuf<CharT>::open(const char* fname, iomode mode) {
    this->freebuf();
    bool res = file_.open(fname, mode);
    if (res) { this->initbuf(mode); }
    return res;
}

template<typename CharT>
bool basic_filebuf<CharT>::open(const wchar_t* fname, iomode mode) {
    this->freebuf();
    bool res = file_.open(fname, mode);
    if (res) { this->initbuf(mode); }
    return res;
}

template<typename CharT>
void basic_filebuf<CharT>::close() {
    this->freebuf();
    file_.close();
}

}  // namespace uxs
