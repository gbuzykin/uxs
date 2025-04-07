#include "uxs/io/filebuf.h"

#include <windows.h>

using namespace uxs;

struct stdfile_buffers {
    filebuf out;
    filebuf in;
    filebuf log;
    filebuf err;
    static stdfile_buffers& instance();
    stdfile_buffers()
        : out(::GetStdHandle(STD_OUTPUT_HANDLE), iomode::out | iomode::append | iomode::cr_lf | iomode::ctrl_esc),
          in(::GetStdHandle(STD_INPUT_HANDLE), iomode::in | iomode::cr_lf, &out),
          log(::GetStdHandle(STD_ERROR_HANDLE), iomode::out | iomode::append | iomode::cr_lf | iomode::ctrl_esc, &out),
          err(::GetStdHandle(STD_ERROR_HANDLE), iomode::out | iomode::append | iomode::cr_lf | iomode::ctrl_esc, &log) {
        ::SetConsoleCP(CP_UTF8);
        ::SetConsoleOutputCP(CP_UTF8);
    }
};

stdfile_buffers& stdfile_buffers::instance() {
    static stdfile_buffers instance;
    return instance;
}

ibuf& stdbuf::in() { return stdfile_buffers::instance().in; }
iobuf& stdbuf::out() { return stdfile_buffers::instance().out; }
iobuf& stdbuf::log() { return stdfile_buffers::instance().log; }
iobuf& stdbuf::err() { return stdfile_buffers::instance().err; }
