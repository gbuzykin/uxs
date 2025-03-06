#include "uxs/io/zipfile.h"

#include "uxs/string_util.h"

#if defined(UXS_USE_LIBZIP)

#    include <zip.h>

using namespace uxs;

bool zipfile::open(ziparch& arch, const char* fname, iomode mode) {
    if (!arch.zip_) { return false; }
    close();
    if (!!(mode & iomode::out)) {
        zip_source_t* source = ::zip_source_buffer_create(nullptr, 0, 0, nullptr);
        if (!source) { return false; }
        if (::zip_source_begin_write(source) != 0) {
            ::zip_source_free(source);
            return false;
        }
        fname_ = fname;
        zip_arch_ = arch.zip_;
        zip_fd_ = source;
    } else if (!!(mode & iomode::in)) {
        zip_fd_ = ::zip_fopen(static_cast<zip_t*>(arch.zip_), fname, ZIP_FL_UNCHANGED);
        if (!zip_fd_) { return false; }
    }
    mode_ = mode;
    return true;
}

void zipfile::close() noexcept {
    if (!zip_fd_) { return; }
    if (!!(mode_ & iomode::out)) {
        zip_source_t* source = static_cast<zip_source_t*>(zip_fd_);
        if (::zip_source_commit_write(source) != 0 ||
            ::zip_file_add(static_cast<zip_t*>(zip_arch_), fname_.c_str(), source, ZIP_FL_ENC_UTF_8) < 0) {
            ::zip_source_free(source);
        }
    } else {
        ::zip_fclose(static_cast<zip_file_t*>(zip_fd_));
    }
    zip_arch_ = nullptr;
    zip_fd_ = nullptr;
}

int zipfile::read(void* data, std::size_t sz, std::size_t& n_read) {
    if (!zip_fd_ || !(mode_ & iomode::in)) { return -1; }
    const zip_int64_t result = ::zip_fread(static_cast<zip_file_t*>(zip_fd_), data, sz);
    if (result < 0) { return -1; }
    n_read = static_cast<std::size_t>(result);
    return 0;
}

int zipfile::write(const void* data, std::size_t sz, std::size_t& n_written) {
    if (!zip_fd_ || !(mode_ & iomode::out)) { return -1; }
    const zip_int64_t result = ::zip_source_write(static_cast<zip_source_t*>(zip_fd_), data, sz);
    if (result < 0) { return -1; }
    n_written = static_cast<std::size_t>(result);
    return 0;
}

#else  // defined(UXS_USE_LIBZIP)

using namespace uxs;
bool zipfile::open(ziparch& arch, const char* fname, iomode mode) { return false; }
void zipfile::close() noexcept {}
int zipfile::read(void* data, std::size_t sz, std::size_t& n_read) { return -1; }
int zipfile::write(const void* data, std::size_t sz, std::size_t& n_written) { return -1; }

#endif  // defined(UXS_USE_LIBZIP)

bool zipfile::open(ziparch& arch, const wchar_t* fname, iomode mode) {
    return open(arch, from_wide_to_utf8(fname).c_str(), mode);
}
