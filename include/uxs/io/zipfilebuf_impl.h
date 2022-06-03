#pragma once

#include "uxs/io/zipfilebuf.h"

namespace uxs {

template<typename CharT>
basic_zipfilebuf<CharT>::basic_zipfilebuf(ziparch& arch, const char* fname, iomode mode)
    : basic_devbuf<CharT>(zip_file_), zip_file_(arch, fname) {
    if (zip_file_.valid()) { this->initbuf(mode); }
}

template<typename CharT>
basic_zipfilebuf<CharT>::basic_zipfilebuf(ziparch& arch, const wchar_t* fname, iomode mode)
    : basic_devbuf<CharT>(zip_file_), zip_file_(arch, fname) {
    if (zip_file_.valid()) { this->initbuf(mode); }
}

template<typename CharT>
basic_zipfilebuf<CharT>::~basic_zipfilebuf() {
    this->freebuf();
}

template<typename CharT>
basic_zipfilebuf<CharT>::basic_zipfilebuf(basic_zipfilebuf&& other) NOEXCEPT : basic_devbuf<CharT>(std::move(other)),
                                                                               zip_file_(std::move(other.zip_file_)) {
    this->setdev(&zip_file_);
}

template<typename CharT>
basic_zipfilebuf<CharT>& basic_zipfilebuf<CharT>::operator=(basic_zipfilebuf&& other) NOEXCEPT {
    if (&other == this) { return *this; }
    static_cast<basic_devbuf<CharT>&>(*this) = std::move(other);
    zip_file_ = std::move(other.zip_file_);
    this->setdev(&zip_file_);
    return *this;
}

template<typename CharT>
bool basic_zipfilebuf<CharT>::open(ziparch& arch, const char* fname, iomode mode) {
    this->freebuf();
    bool res = zip_file_.open(arch, fname);
    if (res) { this->initbuf(mode); }
    return res;
}

template<typename CharT>
bool basic_zipfilebuf<CharT>::open(ziparch& arch, const wchar_t* fname, iomode mode) {
    this->freebuf();
    bool res = zip_file_.open(arch, fname);
    if (res) { this->initbuf(mode); }
    return res;
}

template<typename CharT>
void basic_zipfilebuf<CharT>::close() {
    this->freebuf();
    zip_file_.close();
}

}  // namespace uxs