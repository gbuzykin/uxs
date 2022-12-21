#include "uxs/io/sysfile.h"

#include "uxs/stringalg.h"
#include "uxs/stringcvt.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace uxs;

sysfile::sysfile() : fd_(-1) {}
sysfile::sysfile(file_desc_t fd) : fd_(fd) {}
sysfile::~sysfile() {
    if (fd_ >= 0) { ::close(fd_); }
}

bool sysfile::valid() const { return fd_ >= 0; }

void sysfile::attach(file_desc_t fd) {
    if (fd == fd_) { return; }
    if (fd_ >= 0) { ::close(fd_); }
    fd_ = fd;
}

file_desc_t sysfile::detach() {
    file_desc_t fd = fd_;
    fd_ = -1;
    return fd;
}

bool sysfile::open(const char* fname, iomode mode) {
    int oflag = O_RDONLY;
    if (!!(mode & iomode::kOut)) {
        oflag = !!(mode & iomode::kIn) ? O_RDWR : O_WRONLY;
        if (!!(mode & iomode::kTruncate)) {
            oflag |= O_TRUNC;
        } else if (!!(mode & iomode::kAppend)) {
            oflag |= O_APPEND;
        }
        if (!!(mode & iomode::kCreate)) {
            oflag |= O_CREAT;
            if (!!(mode & iomode::kExcl)) { oflag = (oflag & ~(O_TRUNC | O_APPEND)) | O_EXCL; }
        }
    }
    attach(::open(fname, O_LARGEFILE | oflag, S_IREAD | S_IWRITE));
    return fd_ >= 0;
}

bool sysfile::open(const wchar_t* fname, iomode mode) { return open(from_wide_to_utf8(fname).c_str(), mode); }

void sysfile::close() { ::close(detach()); }

int sysfile::read(void* data, size_t sz, size_t& n_read) {
    ssize_t result = ::read(fd_, data, sz);
    if (result < 0) { return -1; }
    n_read = static_cast<size_t>(result);
    return 0;
}

int sysfile::write(const void* data, size_t sz, size_t& n_written) {
    ssize_t result = ::write(fd_, data, sz);
    if (result < 0) { return -1; }
    n_written = static_cast<size_t>(result);
    return 0;
}

int64_t sysfile::seek(int64_t off, seekdir dir) {
    int whence = SEEK_SET;
    switch (dir) {
        case seekdir::kCurr: whence = SEEK_CUR; break;
        case seekdir::kEnd: whence = SEEK_END; break;
        default: break;
    }
    off_t result = ::lseek(fd_, off, whence);
    return result < 0 ? -1 : result;
}

int sysfile::ctrlesc_color(span<uint8_t> v) {
    uxs::inline_dynbuffer buf;
    buf += "\033[";
    join_basic_strings(
        buf, v, ';',
        std::bind(to_basic_string<uxs::dynbuffer, uint8_t>, std::placeholders::_1, std::placeholders::_2, fmt_opts{}));
    buf += 'm';
    return ::write(fd_, buf.data(), buf.size()) < 0 ? -1 : 0;
}

int sysfile::flush() { return 0; }

/*static*/ bool sysfile::remove(const char* fname) { return ::unlink(fname) == 0; }
/*static*/ bool sysfile::remove(const wchar_t* fname) { return remove(from_wide_to_utf8(fname).c_str()); }
