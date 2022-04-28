#pragma once

#include "iodevice.h"

namespace util {

#if defined(WIN32)
using file_desc_t = void*;
#elif defined(__linux__)
using file_desc_t = int;
#endif

class UTIL_EXPORT rawfile : public iodevice {
 public:
    using size_type = iodevice::size_type;

    rawfile();
    explicit rawfile(file_desc_t fd);
    rawfile(const char* fname, iomode mode) : rawfile() { open(fname, mode); }
    ~rawfile() override;
    rawfile(rawfile&& other) NOEXCEPT : fd_(other.detach()) {}
    rawfile& operator=(rawfile&& other) {
        attach(other.detach());
        return *this;
    }

    bool valid() const;
    void attach(file_desc_t fd);
    file_desc_t detach();

    bool open(const char* fname, iomode mode);
    void close();

    int read(void* buf, size_type sz, size_type& n_read) override;
    int write(const void* buf, size_type sz, size_type& n_written) override;
    int64_t seek(int64_t off, seekdir dir) override;
    int ctrlesc_color(span<uint8_t> v) override;
    int flush() override;

 private:
    file_desc_t fd_;
};

}  // namespace util
