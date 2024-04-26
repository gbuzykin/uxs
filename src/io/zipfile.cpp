#include "uxs/io/zipfile.h"

#include "uxs/stringalg.h"

#if defined(UXS_USE_LIBZIP)

#    include <zip.h>

using namespace uxs;

bool zipfile::open(ziparch& arch, const char* fname) {
    if (!arch.zip_) { return false; }
    if (zip_fd_) { zip_fclose(static_cast<zip_file_t*>(zip_fd_)); }
    return (zip_fd_ = zip_fopen(static_cast<zip_t*>(arch.zip_), fname, ZIP_FL_UNCHANGED)) != nullptr;
}

void zipfile::close() noexcept {
    if (!zip_fd_) { return; }
    zip_fclose(static_cast<zip_file_t*>(zip_fd_));
    zip_fd_ = nullptr;
}

int zipfile::read(void* buf, std::size_t sz, std::size_t& n_read) {
    if (!zip_fd_) { return -1; }
    zip_int64_t result = zip_fread(static_cast<zip_file_t*>(zip_fd_), buf, sz);
    if (result < 0) { return -1; }
    n_read = static_cast<std::size_t>(result);
    return 0;
}

#else  // defined(UXS_USE_LIBZIP)

using namespace uxs;
bool zipfile::open(ziparch& arch, const char* fname) { return false; }
void zipfile::close() noexcept {}
int zipfile::read(void* buf, std::size_t sz, std::size_t& n_read) { return -1; }

#endif  // defined(UXS_USE_LIBZIP)

bool zipfile::open(ziparch& arch, const wchar_t* fname) { return open(arch, uxs::from_wide_to_utf8(fname).c_str()); }
