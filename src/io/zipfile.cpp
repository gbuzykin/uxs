#include "uxs/io/zipfile.h"

#include "uxs/string_util.h"

#if defined(UXS_USE_LIBZIP)

#    include <zip.h>

using namespace uxs;

bool zipfile::open(ziparch& arch, const char* fname, iomode mode) {
    if (!arch.zip_) { return false; }
    close();
    zip_t* zip = static_cast<zip_t*>(arch.zip_);
    if (!!(mode & iomode::out)) {
        mode &= ~iomode::in;
        writing_desc_t* wr_desc = new writing_desc_t{std::string{fname}, zip};
        zip_source_t* source = ::zip_source_buffer_create(nullptr, 0, 0, nullptr);
        if (!source || ::zip_source_begin_write(source) != 0) {
            ::zip_source_free(source);
            delete wr_desc;
            return false;
        }
        wr_desc->zip_source = source;
        zip_fdesc_ = wr_desc;
    } else if (!!(mode & iomode::in)) {
        zip_fdesc_ = ::zip_fopen(zip, fname, ZIP_FL_ENC_UTF_8 | ZIP_FL_UNCHANGED);
        if (!zip_fdesc_) { return false; }
    } else {
        return false;
    }
    mode_ = mode;
    return true;
}

bool zipfile::open(ziparch& arch, std::uint64_t index, iomode mode) {
    if (!arch.zip_) { return false; }
    close();
    if (!(mode & iomode::out) && !!(mode & iomode::in)) {
        zip_t* zip = static_cast<zip_t*>(arch.zip_);
        zip_fdesc_ = ::zip_fopen_index(zip, static_cast<zip_uint64_t>(index), ZIP_FL_UNCHANGED);
        if (!zip_fdesc_) { return false; }
    } else {
        return false;
    }
    mode_ = mode;
    return true;
}

void zipfile::close() noexcept {
    if (!zip_fdesc_) { return; }
    if (!!(mode_ & iomode::out)) {
        writing_desc_t* wr_desc = static_cast<writing_desc_t*>(zip_fdesc_);
        zip_t* zip = static_cast<zip_t*>(wr_desc->zip_arch);
        zip_source_t* source = static_cast<zip_source_t*>(wr_desc->zip_source);
        std::int64_t index = -1;
        if (::zip_source_commit_write(source) != 0 ||
            (index = ::zip_file_add(zip, wr_desc->fname.c_str(), source, ZIP_FL_ENC_UTF_8)) < 0) {
            ::zip_source_free(source);
        }
        if (index >= 0 && (wr_desc->zip_compr != zipfile_compression::deflate || wr_desc->zip_compr_level != 0)) {
            ::zip_set_file_compression(zip, index,
                                       wr_desc->zip_compr == zipfile_compression::store ? ZIP_CM_STORE : ZIP_CM_DEFLATE,
                                       static_cast<zip_uint32_t>(wr_desc->zip_compr_level));
        }
        delete wr_desc;
    } else {
        ::zip_fclose(static_cast<zip_file_t*>(zip_fdesc_));
    }
    zip_fdesc_ = nullptr;
}

int zipfile::read(void* data, std::size_t sz, std::size_t& n_read) {
    if (!zip_fdesc_ || !(mode_ & iomode::in)) { return -1; }
    const zip_int64_t result = ::zip_fread(static_cast<zip_file_t*>(zip_fdesc_), data, static_cast<zip_uint64_t>(sz));
    if (result < 0) { return -1; }
    n_read = static_cast<std::size_t>(result);
    return 0;
}

int zipfile::write(const void* data, std::size_t sz, std::size_t& n_written) {
    if (!zip_fdesc_ || !(mode_ & iomode::out)) { return -1; }
    const writing_desc_t* wr_desc = static_cast<const writing_desc_t*>(zip_fdesc_);
    zip_source_t* source = static_cast<zip_source_t*>(wr_desc->zip_source);
    const zip_int64_t result = ::zip_source_write(source, data, static_cast<zip_uint64_t>(sz));
    if (result < 0) { return -1; }
    n_written = static_cast<std::size_t>(result);
    return 0;
}

#else  // defined(UXS_USE_LIBZIP)

using namespace uxs;
bool zipfile::open(ziparch& /*arch*/, const char* /*fname*/, iomode /*mode*/) { return false; }
bool zipfile::open(ziparch& /*arch*/, std::uint64_t /*index*/, iomode /*mode*/) { return false; }
void zipfile::close() noexcept {}
int zipfile::read(void* /*data*/, std::size_t /*sz*/, std::size_t& /*n_read*/) { return -1; }
int zipfile::write(const void* /*data*/, std::size_t /*sz*/, std::size_t& /*n_written*/) { return -1; }

#endif  // defined(UXS_USE_LIBZIP)

bool zipfile::open(ziparch& arch, const wchar_t* fname, iomode mode) {
    return open(arch, from_wide_to_utf8(fname).c_str(), mode);
}

void zipfile::set_compression(zipfile_compression compr, unsigned level) {
    if (!zip_fdesc_) { return; }
    if (!!(mode_ & iomode::out)) {
        writing_desc_t* wr_desc = static_cast<writing_desc_t*>(zip_fdesc_);
        wr_desc->zip_compr = compr;
        wr_desc->zip_compr_level = level;
    }
}
