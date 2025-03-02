#include "uxs/io/ziparch.h"

#include "uxs/string_util.h"

#if defined(UXS_USE_LIBZIP)

#    include <zip.h>

using namespace uxs;

bool ziparch::open(const char* name, iomode mode) {
    int ziperr = 0;
    int oflag = ZIP_RDONLY;
    if (!!(mode & iomode::out)) {
        oflag &= ~ZIP_RDONLY;
        if (!!(mode & iomode::truncate)) { oflag |= ZIP_TRUNCATE; }
        if (!!(mode & iomode::create)) {
            oflag |= ZIP_CREATE;
            if (!!(mode & iomode::exclusive)) { oflag = (oflag & ~ZIP_TRUNCATE) | ZIP_EXCL; }
        }
    }
    if (zip_) { ::zip_close(static_cast<zip_t*>(zip_)); }
    return (zip_ = ::zip_open(name, oflag, &ziperr)) != nullptr;
}

void ziparch::close() noexcept {
    if (!zip_) { return; }
    ::zip_close(static_cast<zip_t*>(zip_));
    zip_ = nullptr;
}

bool ziparch::add_file(const char* fname, const void* buf, std::size_t sz) {
    if (!zip_) { return false; }
    zip_source_t* source = ::zip_source_buffer(static_cast<zip_t*>(zip_), buf, sz, 0);
    if (!source) { return false; }
    if (::zip_file_add(static_cast<zip_t*>(zip_), fname, source, ZIP_FL_ENC_UTF_8) >= 0) { return true; }
    ::zip_source_free(source);
    return false;
}

bool ziparch::stat_size(const char* fname, std::uint64_t& sz) {
    if (!zip_) { return false; }
    zip_stat_t stat;
    ::zip_stat_init(&stat);
    if (::zip_stat(static_cast<zip_t*>(zip_), fname, ZIP_STAT_SIZE, &stat) != 0) { return false; }
    sz = stat.size;
    return true;
}

bool ziparch::stat_crc(const char* fname, std::uint32_t& crc) {
    if (!zip_) { return false; }
    zip_stat_t stat;
    ::zip_stat_init(&stat);
    if (::zip_stat(static_cast<zip_t*>(zip_), fname, ZIP_STAT_CRC, &stat) != 0) { return false; }
    crc = stat.crc;
    return true;
}

#else  // defined(UXS_USE_LIBZIP)

using namespace uxs;
bool ziparch::open(const char* name, iomode mode) { return false; }
void ziparch::close() noexcept {}
bool ziparch::add_file(const char* fname, const void* buf, std::size_t sz) { return false; }
bool ziparch::stat_size(const char* fname, std::uint64_t& sz) { return false; }
bool ziparch::stat_crc(const char* fname, std::uint32_t& crc) { return false; }

#endif  // defined(UXS_USE_LIBZIP)

bool ziparch::add_file(const wchar_t* fname, const void* buf, std::size_t sz) {
    return add_file(from_wide_to_utf8(fname).c_str(), buf, sz);
}
bool ziparch::stat_size(const wchar_t* fname, std::uint64_t& sz) {
    return stat_size(from_wide_to_utf8(fname).c_str(), sz);
}
bool ziparch::stat_crc(const wchar_t* fname, std::uint32_t& crc) {
    return stat_crc(from_wide_to_utf8(fname).c_str(), crc);
}

bool ziparch::open(const wchar_t* name, iomode mode) { return open(from_wide_to_utf8(name).c_str(), mode); }
