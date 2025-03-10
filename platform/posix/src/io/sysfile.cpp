#include "uxs/io/sysfile.h"

#include "uxs/string_alg.h"
#include "uxs/stringcvt.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace uxs;

sysfile::sysfile() noexcept : fd_(-1) {}
sysfile::sysfile(file_desc_t fd) noexcept : fd_(fd) {}

bool sysfile::valid() const noexcept { return fd_ >= 0; }

void sysfile::attach(file_desc_t fd) noexcept {
    if (fd == fd_) { return; }
    if (fd_ >= 0) { ::close(fd_); }
    fd_ = fd;
}

file_desc_t sysfile::detach() noexcept {
    const file_desc_t fd = fd_;
    fd_ = -1;
    return fd;
}

bool sysfile::open(const char* fname, iomode mode) {
    int oflag = O_RDONLY;
    if (!!(mode & iomode::out)) {
        oflag = !!(mode & iomode::in) ? O_RDWR : O_WRONLY;
        if (!!(mode & iomode::truncate)) {
            oflag |= O_TRUNC;
        } else if (!!(mode & iomode::append)) {
            oflag |= O_APPEND;
        }
        if (!!(mode & iomode::create)) {
            oflag |= O_CREAT;
            if (!!(mode & iomode::exclusive)) { oflag = (oflag & ~(O_TRUNC | O_APPEND)) | O_EXCL; }
        }
    }

    struct stat sb;
    if (::stat(fname, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        detach();
        return false;
    }

    attach(::open(fname, O_LARGEFILE | oflag, S_IREAD | S_IWRITE));
    return fd_ >= 0;
}

bool sysfile::open(const wchar_t* fname, iomode mode) { return open(from_wide_to_utf8(fname).c_str(), mode); }

void sysfile::close() noexcept { ::close(detach()); }

int sysfile::read(void* data, std::size_t sz, std::size_t& n_read) {
    const ssize_t result = ::read(fd_, data, sz);
    if (result < 0) { return -1; }
    n_read = static_cast<std::size_t>(result);
    return 0;
}

int sysfile::write(const void* data, std::size_t sz, std::size_t& n_written) {
    const ssize_t result = ::write(fd_, data, sz);
    if (result < 0) { return -1; }
    n_written = static_cast<std::size_t>(result);
    return 0;
}

std::int64_t sysfile::seek(std::int64_t off, seekdir dir) {
    int whence = SEEK_SET;
    switch (dir) {
        case seekdir::curr: whence = SEEK_CUR; break;
        case seekdir::end: whence = SEEK_END; break;
        default: break;
    }
    const off64_t result = ::lseek64(fd_, static_cast<off64_t>(off), whence);
    return result < 0 ? -1 : result;
}

int sysfile::ctrlesc_color(est::span<const std::uint8_t> v) {
    inline_dynbuffer buf;
    buf += "\033[";
    join_basic_strings(buf, v, ';', [](membuffer& s, std::uint8_t x) -> membuffer& { return to_basic_string(s, x); });
    buf += 'm';
    return ::write(fd_, buf.data(), buf.size()) < 0 ? -1 : 0;
}

int sysfile::truncate() {
    const off64_t pos = ::lseek64(fd_, 0, SEEK_CUR);
    if (pos < 0) { return -1; }
    return ::ftruncate64(fd_, pos) < 0 ? -1 : 0;
}

int sysfile::flush() { return 0; }

/*static*/ bool sysfile::remove(const char* fname) { return ::unlink(fname) == 0; }
/*static*/ bool sysfile::remove(const wchar_t* fname) { return remove(from_wide_to_utf8(fname).c_str()); }
