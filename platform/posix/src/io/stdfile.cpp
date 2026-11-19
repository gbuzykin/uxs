#include "uxs/io/filebuf.h"

#include <stdio.h>
#include <unistd.h>

using namespace uxs;

struct stdfile_buffers {
    filebuf out;
    filebuf in;
    filebuf log;
    filebuf err;
    static stdfile_buffers& instance();
    stdfile_buffers()
        : out(file_desc_t(::fileno(stdout)),
              iomode::out | iomode::append | (::isatty(::fileno(stdout)) ? iomode::none : iomode::skip_ctrl_esc)),
          in(file_desc_t(::fileno(stdin)), iomode::in, &out),
          log(file_desc_t(::fileno(stderr)),
              iomode::out | iomode::append | (::isatty(::fileno(stderr)) ? iomode::none : iomode::skip_ctrl_esc), &out),
          err(file_desc_t(::fileno(stderr)),
              iomode::out | iomode::append | (::isatty(::fileno(stderr)) ? iomode::none : iomode::skip_ctrl_esc),
              &log) {}
    ~stdfile_buffers() {
        err.detach();
        log.detach();
        in.detach();
        out.detach();
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
