#include "uxs/io/ziparch.h"

#include "uxs/string_util.h"

#include <cstdio>

#if defined(UXS_USE_LIBZIP)

#    include <zip.h>

using namespace uxs;

bool ziparch::open(const char* name, iomode mode) {
    int flags = ZIP_RDONLY;
    if (!!(mode & iomode::out)) {
        flags &= ~ZIP_RDONLY;
        if (!!(mode & iomode::truncate)) { flags |= ZIP_TRUNCATE; }
        if (!!(mode & iomode::create)) {
            flags |= ZIP_CREATE;
            if (!!(mode & iomode::exclusive)) { flags = (flags & ~ZIP_TRUNCATE) | ZIP_EXCL; }
        }
    }
    close();
    return (zip_ = ::zip_open(name, flags, nullptr)) != nullptr;
}

bool ziparch::open_sourced(const void* data, std::size_t sz) {
    close();
    zip_source_t* source = ::zip_source_buffer_create(nullptr, 0, 0, nullptr);
    if (!source) { return false; }
    int flags = 0;
    if (!data) {
        flags |= ZIP_TRUNCATE;
    } else if (::zip_source_begin_write(source) != 0 ||
               ::zip_source_write(source, data, static_cast<zip_uint64_t>(sz)) != static_cast<zip_int64_t>(sz) ||
               ::zip_source_commit_write(source) != 0) {
        ::zip_source_free(source);
        return false;
    }
    ::zip_source_keep(source);
    zip_ = ::zip_open_from_source(source, flags, nullptr);
    if (!zip_) {
        ::zip_source_free(source);
        return false;
    }
    zip_source_ = source;
    return true;
}

ziparch_source ziparch::release_source() {
    if (!zip_ || !zip_source_) { return {}; }
    ::zip_close(static_cast<zip_t*>(zip_));
    zip_source_t* source = static_cast<zip_source_t*>(zip_source_);
    ziparch_source result;
    if (::zip_source_open(source) == 0) {
        ::zip_source_seek(source, 0, SEEK_END);
        if (const zip_int64_t sz = ::zip_source_tell(source)) {
            if (void* data = std::malloc(sz)) {
                ::zip_source_seek(source, 0, SEEK_SET);
                if (::zip_source_read(source, data, sz) == sz) {
                    result.data_ = data, result.size_ = static_cast<std::size_t>(sz);
                } else {
                    std::free(data);
                }
            }
        }
        ::zip_source_close(source);
    }
    ::zip_source_free(source);
    zip_ = zip_source_ = nullptr;
    return result;
}

void ziparch::close() noexcept {
    if (!zip_) { return; }
    ::zip_close(static_cast<zip_t*>(zip_));
    ::zip_source_free(static_cast<zip_source_t*>(zip_source_));
    zip_ = zip_source_ = nullptr;
}

std::int64_t ziparch::add_file(const char* fname, const void* data, std::size_t sz, zipfile_compression compr,
                               unsigned level) {
    if (!zip_) { return -1; }
    zip_t* zip = static_cast<zip_t*>(zip_);
    zip_source_t* source = ::zip_source_buffer_create(nullptr, 0, 0, nullptr);
    std::int64_t index = -1;
    if (!source || ::zip_source_begin_write(source) != 0 ||
        ::zip_source_write(source, data, static_cast<zip_uint64_t>(sz)) != static_cast<zip_int64_t>(sz) ||
        ::zip_source_commit_write(source) != 0 || (index = ::zip_file_add(zip, fname, source, ZIP_FL_ENC_UTF_8)) < 0) {
        ::zip_source_free(source);
        return -1;
    }
    if (compr != zipfile_compression::deflate || level != 0) {
        ::zip_set_file_compression(zip, index, compr == zipfile_compression::store ? ZIP_CM_STORE : ZIP_CM_DEFLATE,
                                   static_cast<zip_uint32_t>(level));
    }
    return static_cast<std::int64_t>(index);
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
