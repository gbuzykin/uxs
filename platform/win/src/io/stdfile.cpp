#include "uxs/io/filebuf.h"

#include <windows.h>

using namespace uxs;

struct stdfile_buffers {
    filebuf out;
    filebuf in;
    filebuf log;
    filebuf err;
    UINT prev_cp;
    UINT prev_output_cp;
    static stdfile_buffers& instance();
    stdfile_buffers()
        : out(::GetStdHandle(STD_OUTPUT_HANDLE), iomode::out | iomode::append | iomode::cr_lf | iomode::ctrl_esc),
          in(::GetStdHandle(STD_INPUT_HANDLE), iomode::in | iomode::cr_lf, &out),
          log(::GetStdHandle(STD_ERROR_HANDLE), iomode::out | iomode::append | iomode::cr_lf | iomode::ctrl_esc, &out),
          err(::GetStdHandle(STD_ERROR_HANDLE), iomode::out | iomode::append | iomode::cr_lf | iomode::ctrl_esc, &log),
          prev_cp(::GetConsoleCP()), prev_output_cp(::GetConsoleOutputCP()) {
        ::SetConsoleCP(CP_UTF8);
        ::SetConsoleOutputCP(CP_UTF8);
    }
    ~stdfile_buffers() {
        err.detach();
        log.detach();
        in.detach();
        out.detach();
        ::SetConsoleCP(prev_cp);
        ::SetConsoleOutputCP(prev_output_cp);
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
