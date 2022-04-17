#include "util/io/rawfile.h"

#include "util/stringalg.h"
#include "util/stringcvt.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace util;

rawfile::rawfile() : fd_(-1) {}
rawfile::rawfile(file_desc_t fd) : fd_(fd) {}
rawfile::~rawfile() {
    if (fd_ >= 0) { ::close(fd_); }
}

bool rawfile::valid() const { return fd_ >= 0; }

void rawfile::attach(file_desc_t fd) {
    if (fd == fd_) { return; }
    if (fd_ >= 0) { ::close(fd_); }
    fd_ = fd;
}

rawfile::file_desc_t rawfile::detach() {
    file_desc_t fd = fd_;
    fd_ = -1;
    return fd;
}

bool rawfile::open(const char* fname, iomode mode) {
    int oflag = O_RDONLY;
    if (!!(mode & iomode::kOut)) {
        oflag = O_WRONLY | O_CREAT;
        if (!!(mode & iomode::kCreateNew)) {
            oflag |= O_EXCL;
        } else if (!!(mode & iomode::kAppend)) {
            oflag |= O_APPEND;
        } else {
            oflag |= O_TRUNC;
        }
    }
    attach(::open(fname, O_LARGEFILE | oflag, S_IREAD | S_IWRITE));
    return fd_ >= 0;
}

void rawfile::close() { ::close(detach()); }

int rawfile::read(void* data, size_type sz, size_type& n_read) {
    ssize_t result = ::read(fd_, data, sz);
    if (result < 0) { return -1; }
    n_read = static_cast<size_type>(result);
    return 0;
}

int rawfile::write(const void* data, size_type sz, size_type& n_written) {
    ssize_t result = ::write(fd_, data, sz);
    if (result < 0) { return -1; }
    n_written = static_cast<size_type>(result);
    return 0;
}

int64_t rawfile::seek(int64_t off, seekdir dir) {
    int whence = SEEK_SET;
    switch (dir) {
        case seekdir::kCurr: whence = SEEK_CUR; break;
        case seekdir::kEnd: whence = SEEK_END; break;
        default: break;
    }
    off_t result = ::lseek(fd_, off, whence);
    return result < 0 ? -1 : result;
}

int rawfile::ctrlesc_color(span<uint8_t> v) {
    using namespace std::placeholders;
    util::dynbuf_appender buf;
    buf += "\033[";
    join_strings_append(buf, v, ';', std::bind(to_string_append<uint8_t, util::dynbuf_appender>, _1, _2, fmt_state()));
    buf += 'm';
    return ::write(fd_, buf.data(), buf.size()) < 0 ? -1 : 0;
}

int rawfile::flush() { return 0; }
