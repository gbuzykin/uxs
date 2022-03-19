#include "util/io/filebuf.h"

#include <unistd.h>

using namespace util;

static filebuf g_outbuf(fileno(stdout), isatty(fileno(stdout)) ? iomode::kOut : iomode::kOut | iomode::kSkipCtrlEsc);
static filebuf g_inbuf(fileno(stdin), iomode::kIn, &stdbuf::out);
static filebuf g_logbuf(fileno(stderr), isatty(fileno(stderr)) ? iomode::kOut : iomode::kOut | iomode::kSkipCtrlEsc,
                        &stdbuf::out);
static filebuf g_errbuf(fileno(stderr), isatty(fileno(stderr)) ? iomode::kOut : iomode::kOut | iomode::kSkipCtrlEsc,
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
