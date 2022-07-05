#include "uxs/io/ziparch.h"

#include "uxs/stringalg.h"

#if defined(UXS_USE_LIBZIP)

#    include <zip.h>

using namespace uxs;

bool ziparch::open(const char* name, iomode mode) {
    int ziperr = 0;
    int oflag = ZIP_RDONLY;
    if (!!(mode & iomode::kOut)) {
        oflag &= ~ZIP_RDONLY;
        if (!!(mode & iomode::kTruncate)) { oflag |= ZIP_TRUNCATE; }
        if (!!(mode & iomode::kCreate)) {
            oflag |= ZIP_CREATE;
            if (!!(mode & iomode::kExcl)) { oflag = (oflag & ~ZIP_TRUNCATE) | ZIP_EXCL; }
        }
    }
    if (zip_) { zip_close(reinterpret_cast<zip_t*>(zip_)); }
    return (zip_ = zip_open(name, oflag, &ziperr)) != nullptr;
}

void ziparch::close() {
    if (!zip_) { return; }
    zip_close(reinterpret_cast<zip_t*>(zip_));
    zip_ = nullptr;
}

bool ziparch::stat_size(const char* fname, uint64_t& sz) {
    if (!zip_) { return false; }
    zip_stat_t stat;
    zip_stat_init(&stat);
    if (zip_stat(reinterpret_cast<zip_t*>(zip_), fname, ZIP_STAT_SIZE, &stat) != 0) { return false; }
    sz = stat.size;
    return true;
}

bool ziparch::stat_crc(const char* fname, uint32_t& crc) {
    if (!zip_) { return false; }
    zip_stat_t stat;
    zip_stat_init(&stat);
    if (zip_stat(reinterpret_cast<zip_t*>(zip_), fname, ZIP_STAT_CRC, &stat) != 0) { return false; }
    crc = stat.crc;
    return true;
}

#else  // defined(UXS_USE_LIBZIP)

using namespace uxs;
bool ziparch::open(const char* name, iomode mode) { return false; }
void ziparch::close() {}
bool ziparch::stat_size(const char* fname, uint64_t& sz) { return false; }
bool ziparch::stat_crc(const char* fname, uint32_t& crc) { return false; }

#endif  // defined(UXS_USE_LIBZIP)

bool ziparch::stat_size(const wchar_t* fname, uint64_t& sz) {
    return stat_size(uxs::from_wide_to_utf8(fname).c_str(), sz);
}
bool ziparch::stat_crc(const wchar_t* fname, uint32_t& crc) {
    return stat_crc(uxs::from_wide_to_utf8(fname).c_str(), crc);
}

bool ziparch::open(const wchar_t* name, iomode mode) { return open(uxs::from_wide_to_utf8(name).c_str(), mode); }
