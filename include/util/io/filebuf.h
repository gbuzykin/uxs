#pragma once

#include "devbuf.h"
#include "rawfile.h"

namespace util {

template<typename CharT>
class basic_filebuf : public basic_devbuf<CharT> {
 public:
    basic_filebuf() : basic_devbuf<CharT>(file_) {}
    basic_filebuf(rawfile::file_desc_t fd, iomode mode, basic_iobuf<CharT>* tie = nullptr);
    basic_filebuf(const char* fname, iomode mode);
    basic_filebuf(const char* fname, const char* mode);
    ~basic_filebuf() override;
    basic_filebuf(basic_filebuf&& other) NOEXCEPT;
    basic_filebuf& operator=(basic_filebuf&& other) NOEXCEPT;

    void attach(rawfile::file_desc_t fd, iomode mode);
    rawfile::file_desc_t detach();

    bool open(const char* fname, iomode mode);
    bool open(const char* fname, const char* mode);
    void close();

 private:
    rawfile file_;
};

using filebuf = basic_filebuf<char>;
using wfilebuf = basic_filebuf<wchar_t>;
using u8filebuf = basic_filebuf<uint8_t>;

}  // namespace util
