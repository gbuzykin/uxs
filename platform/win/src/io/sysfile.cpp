#include "uxs/io/sysfile.h"

#include "uxs/stringalg.h"

#include <windows.h>

#include <array>
#include <cstring>

using namespace uxs;

sysfile::sysfile() : fd_(INVALID_HANDLE_VALUE) {}
sysfile::sysfile(file_desc_t fd) : fd_(fd) {}
sysfile::~sysfile() {
    if (fd_ != INVALID_HANDLE_VALUE) { ::CloseHandle(fd_); }
}

bool sysfile::valid() const { return fd_ != INVALID_HANDLE_VALUE; }

void sysfile::attach(file_desc_t fd) {
    if (fd == fd_) { return; }
    if (fd_ != INVALID_HANDLE_VALUE) { ::CloseHandle(fd_); }
    fd_ = fd;
}

file_desc_t sysfile::detach() {
    file_desc_t fd = fd_;
    fd_ = INVALID_HANDLE_VALUE;
    return fd;
}

bool sysfile::open(const wchar_t* fname, iomode mode) {
    DWORD access = GENERIC_READ, creat_disp = OPEN_EXISTING;
    if (!!(mode & iomode::out)) {
        access |= GENERIC_WRITE;
        if (!!(mode & iomode::create)) {
            if (!!(mode & iomode::exclusive)) {
                creat_disp = CREATE_NEW;
            } else if (!!(mode & iomode::truncate)) {
                creat_disp = CREATE_ALWAYS;
            } else {
                creat_disp = OPEN_ALWAYS;
            }
        } else if (!!(mode & iomode::truncate)) {
            creat_disp = TRUNCATE_EXISTING;
        }
    }
    OFSTRUCT of;
    std::memset(&of, 0, sizeof(of));
    attach(::CreateFileW(fname, access, FILE_SHARE_READ, NULL, creat_disp, FILE_ATTRIBUTE_NORMAL, NULL));
    if (fd_ != INVALID_HANDLE_VALUE) {
        if (!!(mode & iomode::append)) {
            if (::SetFilePointer(fd_, 0, NULL, FILE_END) == INVALID_SET_FILE_POINTER) {
                ::CloseHandle(detach());
                return false;
            }
        }
        return true;
    }
    return false;
}

bool sysfile::open(const char* fname, iomode mode) { return open(from_utf8_to_wide(fname).c_str(), mode); }

void sysfile::close() { ::CloseHandle(detach()); }

int sysfile::read(void* data, std::size_t sz, std::size_t& n_read) {
    DWORD n_read_native = 0;
    if (!::ReadFile(fd_, data, static_cast<DWORD>(sz), &n_read_native, NULL)) { return -1; }
    n_read = static_cast<std::size_t>(n_read_native);
    return 0;
}

int sysfile::write(const void* data, std::size_t sz, std::size_t& n_written) {
    DWORD n_written_native = 0;
    if (!::WriteFile(fd_, data, static_cast<DWORD>(sz), &n_written_native, NULL)) { return -1; }
    n_written = static_cast<std::size_t>(n_written_native);
    return 0;
}

int sysfile::ctrlesc_color(uxs::span<const std::uint8_t> v) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    std::memset(&info, 0, sizeof(info));
    if (!::GetConsoleScreenBufferInfo(fd_, &info)) { return -1; }
    static const std::array<WORD, 8> fg_color = {0,
                                                 FOREGROUND_RED,
                                                 FOREGROUND_GREEN,
                                                 FOREGROUND_RED | FOREGROUND_GREEN,
                                                 FOREGROUND_BLUE,
                                                 FOREGROUND_BLUE | FOREGROUND_RED,
                                                 FOREGROUND_BLUE | FOREGROUND_GREEN,
                                                 FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN};
    static const std::array<WORD, 8> bg_color = {0,
                                                 BACKGROUND_RED,
                                                 BACKGROUND_GREEN,
                                                 BACKGROUND_RED | BACKGROUND_GREEN,
                                                 BACKGROUND_BLUE,
                                                 BACKGROUND_BLUE | BACKGROUND_RED,
                                                 BACKGROUND_BLUE | BACKGROUND_GREEN,
                                                 BACKGROUND_BLUE | BACKGROUND_RED | BACKGROUND_GREEN};
    for (std::uint8_t c : v) {
        if (c == 0) {
            info.wAttributes = fg_color[7];
        } else if (c == 1) {
            info.wAttributes |= FOREGROUND_INTENSITY;
        } else if (c >= 30 && c <= 37) {
            info.wAttributes = (info.wAttributes & ~fg_color[7]) | fg_color[c - 30];
        } else if (c >= 40 && c <= 47) {
            info.wAttributes = (info.wAttributes & ~bg_color[7]) | bg_color[c - 40];
        } else if (c >= 90 && c <= 97) {
            info.wAttributes = (info.wAttributes & ~fg_color[7]) | fg_color[c - 90] | FOREGROUND_INTENSITY;
        } else if (c >= 100 && c <= 107) {
            info.wAttributes = (info.wAttributes & ~bg_color[7]) | bg_color[c - 100] | BACKGROUND_INTENSITY;
        }
    }
    ::SetConsoleTextAttribute(fd_, info.wAttributes);
    return 0;
}

std::int64_t sysfile::seek(std::int64_t off, seekdir dir) {
    DWORD method = FILE_BEGIN;
    LONG pos_hi = static_cast<long>(off >> 32);
    switch (dir) {
        case seekdir::curr: method = FILE_CURRENT; break;
        case seekdir::end: method = FILE_END; break;
        default: break;
    }
    DWORD pos_lo = ::SetFilePointer(fd_, static_cast<long>(off), &pos_hi, method);
    if (pos_lo == INVALID_SET_FILE_POINTER) { return -1; }
    return (static_cast<std::int64_t>(pos_hi) << 32) | pos_lo;
}

int sysfile::flush() { return 0; }

/*static*/ bool sysfile::remove(const wchar_t* fname) { return !!::DeleteFileW(fname); }
/*static*/ bool sysfile::remove(const char* fname) { return remove(from_utf8_to_wide(fname).c_str()); }
