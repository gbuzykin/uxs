#include "uxs/io/filebuf.h"

#include <unistd.h>

using namespace uxs;

static filebuf g_outbuf(fileno(stdout),
                        iomode::out | iomode::append | (isatty(fileno(stdout)) ? iomode::none : iomode::skip_ctrl_esc));
static filebuf g_inbuf(fileno(stdin), iomode::in, &stdbuf::out);
static filebuf g_logbuf(fileno(stderr),
                        iomode::out | iomode::append | (isatty(fileno(stderr)) ? iomode::none : iomode::skip_ctrl_esc),
                        &stdbuf::out);
static filebuf g_errbuf(fileno(stderr),
                        iomode::out | iomode::append | (isatty(fileno(stderr)) ? iomode::none : iomode::skip_ctrl_esc),
                        &stdbuf::log);
iobuf& stdbuf::out = g_outbuf;
iobuf& stdbuf::in = g_inbuf;
iobuf& stdbuf::log = g_logbuf;
iobuf& stdbuf::err = g_errbuf;

static struct stdfile_initializer {
    stdfile_initializer() {}
    ~stdfile_initializer() {
        g_errbuf.detach();
        g_logbuf.detach();
        g_inbuf.detach();
        g_outbuf.detach();
    }
} g_stdinit;
