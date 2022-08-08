#include "uxs/io/filebuf.h"

#include <unistd.h>

using namespace uxs;

static filebuf g_outbuf(fileno(stdout), iomode::kOut | iomode::kAppend |
                                            (isatty(fileno(stdout)) ? iomode::kNone : iomode::kSkipCtrlEsc));
static filebuf g_inbuf(fileno(stdin), iomode::kIn, &stdbuf::out);
static filebuf g_logbuf(fileno(stderr),
                        iomode::kOut | iomode::kAppend | (isatty(fileno(stderr)) ? iomode::kNone : iomode::kSkipCtrlEsc),
                        &stdbuf::out);
static filebuf g_errbuf(fileno(stderr),
                        iomode::kOut | iomode::kAppend | (isatty(fileno(stderr)) ? iomode::kNone : iomode::kSkipCtrlEsc),
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
