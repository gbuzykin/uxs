#include "uxs/io/ziparch.h"

#include "uxs/string_util.h"

#include <cstdlib>
#include <cstring>

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

std::int64_t ziparch::add_file(const char* fname, const void* data, std::size_t sz, zipfile_compression compr,
                               unsigned level) {
    if (!zip_) { return -1; }
    void* data_copy = std::malloc(sz);
    if (!data_copy) { return -1; }
    std::memcpy(data_copy, data, sz);
    zip_source_t* source = ::zip_source_buffer_create(data_copy, static_cast<zip_uint64_t>(sz), 1, nullptr);
    if (!source) {
        std::free(data_copy);
        return -1;
    }
    zip_t* zip = static_cast<zip_t*>(zip_);
    const std::int64_t index = ::zip_file_add(zip, fname, source, ZIP_FL_ENC_UTF_8);
    if (index >= 0) {
        if (compr != zipfile_compression::deflate || level != 0) {
            ::zip_set_file_compression(zip, index, compr == zipfile_compression::store ? ZIP_CM_STORE : ZIP_CM_DEFLATE,
                                       static_cast<zip_uint32_t>(level));
        }
        return static_cast<std::int64_t>(index);
    }
    ::zip_source_free(source);
    return -1;
}

bool ziparch::stat_file(const char* fname, zipfile_info& info) const {
    if (!zip_) { return false; }
    zip_t* zip = static_cast<zip_t*>(zip_);
    zip_stat_t stat;
    ::zip_stat_init(&stat);
    if (::zip_stat(zip, fname, ZIP_FL_ENC_UTF_8 | ZIP_FL_UNCHANGED, &stat) != 0) { return false; }
    info.name = stat.name;
    info.index = static_cast<std::uint64_t>(stat.index);
    info.size = static_cast<std::uint64_t>(stat.size);
    info.crc = static_cast<std::uint32_t>(stat.crc);
    return true;
}

bool ziparch::stat_file(std::uint64_t index, zipfile_info& info) const {
    if (!zip_) { return false; }
    zip_t* zip = static_cast<zip_t*>(zip_);
    zip_stat_t stat;
    ::zip_stat_init(&stat);
    if (::zip_stat_index(zip, static_cast<zip_uint64_t>(index), ZIP_FL_UNCHANGED, &stat) != 0) { return false; }
    info.name = stat.name;
    info.index = static_cast<std::uint64_t>(stat.index);
    info.size = static_cast<std::uint64_t>(stat.size);
    info.crc = static_cast<std::uint32_t>(stat.crc);
    return true;
}

#else  // defined(UXS_USE_LIBZIP)

using namespace uxs;
bool ziparch::open(const char* /*name*/, iomode /*mode*/) { return false; }
void ziparch::close() noexcept {}
std::int64_t ziparch::add_file(const char* /*fname*/, const void* /*data*/, std::size_t /*sz*/,
                               zipfile_compression /*compr*/, unsigned /*level*/) {
    return -1;
}
bool ziparch::stat_file(const char* /*fname*/, zipfile_info& /*info*/) const { return false; }
bool ziparch::stat_file(std::uint64_t /*index*/, zipfile_info& /*info*/) const { return false; }

#endif  // defined(UXS_USE_LIBZIP)

bool ziparch::open(const wchar_t* name, iomode mode) { return open(from_wide_to_utf8(name).c_str(), mode); }
std::int64_t ziparch::add_file(const wchar_t* fname, const void* data, std::size_t sz, zipfile_compression compr,
                               unsigned level) {
    return add_file(from_wide_to_utf8(fname).c_str(), data, sz, compr, level);
}
bool ziparch::stat_file(const wchar_t* fname, zipfile_info& info) const {
    return stat_file(from_wide_to_utf8(fname).c_str(), info);
}
