#pragma once

#include "util/io/filebuf.h"

using namespace util;

static iomode mode_from_string(const char* mode) {
    iomode result = iomode::kIn;
    while (*mode) {
        switch (*mode) {
            case 'w': result |= iomode::kOut; break;
            case 'a': result |= iomode::kOut | iomode::kAppend; break;
            case 'x': result |= iomode::kOut | iomode::kCreateNew; break;
            case 't': result |= iomode::kText; break;
            default: break;
        }
        ++mode;
    }
    return result;
}

template<typename CharT>
basic_filebuf<CharT>::basic_filebuf(rawfile::file_desc_t fd, iomode mode, basic_iobuf<CharT>* tie)
    : basic_devbuf<CharT>(file_), file_(fd) {
    this->settie(tie);
    if (file_.valid()) { this->initbuf(mode); }
}

template<typename CharT>
basic_filebuf<CharT>::basic_filebuf(const char* fname, iomode mode) : basic_devbuf<CharT>(file_), file_(fname, mode) {
    if (file_.valid()) { this->initbuf(mode); }
}

template<typename CharT>
basic_filebuf<CharT>::basic_filebuf(const char* fname, const char* mode)
    : basic_filebuf<CharT>(fname, mode_from_string(mode)) {}

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
basic_filebuf<CharT>& basic_filebuf<CharT>::operator=(basic_filebuf&& other) NOEXCEPT {
    if (&other == this) { return *this; }
    static_cast<basic_devbuf<CharT>&>(*this) = std::move(other);
    file_ = std::move(other.file_);
    this->setdev(&file_);
    return *this;
}

template<typename CharT>
void basic_filebuf<CharT>::attach(rawfile::file_desc_t fd, iomode mode) {
    this->initbuf(mode);
    file_.attach(fd);
}

template<typename CharT>
rawfile::file_desc_t basic_filebuf<CharT>::detach() {
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
bool basic_filebuf<CharT>::open(const char* fname, const char* mode) {
    return open(fname, mode_from_string(mode));
}

template<typename CharT>
void basic_filebuf<CharT>::close() {
    this->freebuf();
    file_.close();
}
