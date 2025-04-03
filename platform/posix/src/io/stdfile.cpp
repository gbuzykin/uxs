#include "uxs/io/filebuf.h"

#include <stdio.h>
#include <unistd.h>

using namespace uxs;

struct stdfile_buffers {
    filebuf out;
    filebuf in;
    filebuf log;
    filebuf err;
    stdfile_buffers()
        : out(fileno(stdout),
              iomode::out | iomode::append | (isatty(fileno(stdout)) ? iomode::none : iomode::skip_ctrl_esc)),
          in(fileno(stdin), iomode::in, &out),
          log(fileno(stderr),
              iomode::out | iomode::append | (isatty(fileno(stderr)) ? iomode::none : iomode::skip_ctrl_esc), &out),
          err(fileno(stderr),
              iomode::out | iomode::append | (isatty(fileno(stderr)) ? iomode::none : iomode::skip_ctrl_esc), &log) {}
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
