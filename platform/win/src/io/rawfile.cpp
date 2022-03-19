#include "util/io/rawfile.h"

#include "util/stringalg.h"

#include <windows.h>

#include <cstring>

using namespace util;

rawfile::rawfile() : fd_(INVALID_HANDLE_VALUE) {}
rawfile::rawfile(file_desc_t fd) : fd_(fd) {}
rawfile::~rawfile() {
    if (fd_ != INVALID_HANDLE_VALUE) { ::CloseHandle(fd_); }
}

bool rawfile::valid() const { return fd_ != INVALID_HANDLE_VALUE; }

void rawfile::attach(file_desc_t fd) {
    if (fd == fd_) { return; }
    if (fd_ != INVALID_HANDLE_VALUE) { ::CloseHandle(fd_); }
    fd_ = fd;
}

rawfile::file_desc_t rawfile::detach() {
    file_desc_t fd = fd_;
    fd_ = INVALID_HANDLE_VALUE;
    return fd;
}

bool rawfile::open(const char* fname, iomode mode) {
    DWORD access = GENERIC_READ, creat_disp = OPEN_EXISTING;
    if (!!(mode & iomode::kOut)) {
        access = GENERIC_WRITE, creat_disp = CREATE_ALWAYS;
        if (!!(mode & iomode::kCreateNew)) {
            creat_disp = CREATE_NEW;
        } else if (!!(mode & iomode::kAppend)) {
            access |= FILE_APPEND_DATA, creat_disp = OPEN_ALWAYS;
        }
    }
    OFSTRUCT of;
    std::memset(&of, 0, sizeof(of));
    attach(::CreateFileW(from_utf8_to_wide(fname).c_str(), access, FILE_SHARE_READ, NULL, creat_disp,
                         FILE_ATTRIBUTE_NORMAL, NULL));
    return fd_ != INVALID_HANDLE_VALUE;
}

void rawfile::close() { ::CloseHandle(detach()); }

int rawfile::read(void* data, size_type sz, size_type& n_read) {
    DWORD n_read_native = 0;
    if (!::ReadFile(fd_, data, static_cast<DWORD>(sz), &n_read_native, NULL)) { return -1; }
    n_read = static_cast<size_type>(n_read_native);
    return 0;
}

int rawfile::write(const void* data, size_type sz, size_type& n_written) {
    DWORD n_written_native = 0;
    if (!::WriteFile(fd_, data, static_cast<DWORD>(sz), &n_written_native, NULL)) { return -1; }
    n_written = static_cast<size_type>(n_written_native);
    return 0;
}

int rawfile::ctrlesc_color(span<uint8_t> v) {
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

int rawfile::flush() { return 0; }
