#include "uxs/io/filebuf.h"

#include <windows.h>

using namespace uxs;

struct stdfile_buffers {
    filebuf out;
    filebuf in;
    filebuf log;
    filebuf err;
    stdfile_buffers()
        : out(::GetStdHandle(STD_OUTPUT_HANDLE), iomode::out | iomode::append | iomode::cr_lf | iomode::ctrl_esc),
          in(::GetStdHandle(STD_INPUT_HANDLE), iomode::in | iomode::cr_lf, &out),
          log(::GetStdHandle(STD_ERROR_HANDLE), iomode::out | iomode::append | iomode::cr_lf | iomode::ctrl_esc, &out),
          err(::GetStdHandle(STD_ERROR_HANDLE), iomode::out | iomode::append | iomode::cr_lf | iomode::ctrl_esc, &log) {
        ::SetConsoleCP(CP_UTF8);
        ::SetConsoleOutputCP(CP_UTF8);
    }
};

namespace {
stdfile_buffers& get_stdfile_buffers() {
    static stdfile_buffers bufs;
    return bufs;
}
}  // namespace

namespace stdbuf {
ibuf& in() { return get_stdfile_buffers().in; }
iobuf& out() { return get_stdfile_buffers().out; }
iobuf& log() { return get_stdfile_buffers().log; }
iobuf& err() { return get_stdfile_buffers().err; }
}  // namespace stdbuf
