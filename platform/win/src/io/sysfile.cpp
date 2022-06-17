#include "uxs/io/sysfile.h"

#include "uxs/stringalg.h"

#include <windows.h>

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
    if (!!(mode & iomode::kOut)) {
        access |= GENERIC_WRITE;
        if (!!(mode & iomode::kCreate)) {
            if (!!(mode & iomode::kExcl)) {
                creat_disp = CREATE_NEW;
            } else if (!!(mode & iomode::kTruncate)) {
                creat_disp = CREATE_ALWAYS;
            } else {
                creat_disp = OPEN_ALWAYS;
            }
        } else if (!!(mode & iomode::kTruncate)) {
            creat_disp = TRUNCATE_EXISTING;
        }
    }
    OFSTRUCT of;
    std::memset(&of, 0, sizeof(of));
    attach(::CreateFileW(fname, access, FILE_SHARE_READ, NULL, creat_disp, FILE_ATTRIBUTE_NORMAL, NULL));
    if (fd_ != INVALID_HANDLE_VALUE) {
        if (!!(mode & iomode::kAppend)) {
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

int sysfile::read(void* data, size_t sz, size_t& n_read) {
    DWORD n_read_native = 0;
    if (!::ReadFile(fd_, data, static_cast<DWORD>(sz), &n_read_native, NULL)) { return -1; }
    n_read = static_cast<size_t>(n_read_native);
    return 0;
}

int sysfile::write(const void* data, size_t sz, size_t& n_written) {
    DWORD n_written_native = 0;
    if (!::WriteFile(fd_, data, static_cast<DWORD>(sz), &n_written_native, NULL)) { return -1; }
    n_written = static_cast<size_t>(n_written_native);
    return 0;
}

int sysfile::ctrlesc_color(span<uint8_t> v) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    std::memset(&info, 0, sizeof(info));
    if (!::GetConsoleScreenBufferInfo(fd_, &info)) { return -1; }
    const WORD fg_wh = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    const WORD bg_wh = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
    for (uint8_t c : v) {
        switch (c) {
            case 0: info.wAttributes = fg_wh; break;
            case 1: info.wAttributes |= FOREGROUND_INTENSITY; break;
            case 30: info.wAttributes &= fg_wh; break;
            case 40: info.wAttributes &= bg_wh; break;
            case 31: {
                info.wAttributes = (info.wAttributes & ~fg_wh) | FOREGROUND_RED;
            } break;
            case 32: {
                info.wAttributes = (info.wAttributes & ~fg_wh) | FOREGROUND_GREEN;
            } break;
            case 33: {
                info.wAttributes = (info.wAttributes & ~fg_wh) | FOREGROUND_RED | FOREGROUND_GREEN;
            } break;
            case 34: {
                info.wAttributes = (info.wAttributes & ~fg_wh) | FOREGROUND_BLUE;
            } break;
            case 35: {
                info.wAttributes = (info.wAttributes & ~fg_wh) | FOREGROUND_BLUE | FOREGROUND_RED;
            } break;
            case 36: {
                info.wAttributes = (info.wAttributes & ~fg_wh) | FOREGROUND_BLUE | FOREGROUND_GREEN;
            } break;
            case 37: {
                info.wAttributes = (info.wAttributes & ~fg_wh) | FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN;
            } break;
            case 41: {
                info.wAttributes = (info.wAttributes & ~bg_wh) | BACKGROUND_RED;
            } break;
            case 42: {
                info.wAttributes = (info.wAttributes & ~bg_wh) | BACKGROUND_GREEN;
            } break;
            case 43: {
                info.wAttributes = (info.wAttributes & ~bg_wh) | BACKGROUND_RED | BACKGROUND_GREEN;
            } break;
            case 44: {
                info.wAttributes = (info.wAttributes & ~bg_wh) | BACKGROUND_BLUE;
            } break;
            case 45: {
                info.wAttributes = (info.wAttributes & ~bg_wh) | BACKGROUND_BLUE | BACKGROUND_RED;
            } break;
            case 46: {
                info.wAttributes = (info.wAttributes & ~bg_wh) | BACKGROUND_BLUE | BACKGROUND_GREEN;
            } break;
            case 47: {
                info.wAttributes = (info.wAttributes & ~bg_wh) | BACKGROUND_BLUE | BACKGROUND_RED | BACKGROUND_GREEN;
            } break;
        }
    }
    ::SetConsoleTextAttribute(fd_, info.wAttributes);
    return 0;
}

int64_t sysfile::seek(int64_t off, seekdir dir) {
    DWORD method = FILE_BEGIN;
    LONG pos_hi = static_cast<long>(off >> 32);
    switch (dir) {
        case seekdir::kCurr: method = FILE_CURRENT; break;
        case seekdir::kEnd: method = FILE_END; break;
        default: break;
    }
    DWORD pos_lo = ::SetFilePointer(fd_, static_cast<long>(off), &pos_hi, method);
    if (pos_lo == INVALID_SET_FILE_POINTER) { return -1; }
    return (static_cast<int64_t>(pos_hi) << 32) | pos_lo;
}

int sysfile::flush() { return 0; }

/*static*/ bool sysfile::remove(const wchar_t* fname) { return !!::DeleteFileW(fname); }
/*static*/ bool sysfile::remove(const char* fname) { return remove(from_utf8_to_wide(fname).c_str()); }
