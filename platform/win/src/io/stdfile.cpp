#include "uxs/io/filebuf.h"

#include <windows.h>

using namespace uxs;

static filebuf g_outbuf(::GetStdHandle(STD_OUTPUT_HANDLE),
                        iomode::out | iomode::append | iomode::cr_lf | iomode::ctrl_esc);
static filebuf g_inbuf(::GetStdHandle(STD_INPUT_HANDLE), iomode::in | iomode::cr_lf, &stdbuf::out);
static filebuf g_logbuf(::GetStdHandle(STD_ERROR_HANDLE),
                        iomode::out | iomode::append | iomode::cr_lf | iomode::ctrl_esc, &stdbuf::out);
static filebuf g_errbuf(::GetStdHandle(STD_ERROR_HANDLE),
                        iomode::out | iomode::append | iomode::cr_lf | iomode::ctrl_esc, &stdbuf::log);
iobuf& stdbuf::out = g_outbuf;
iobuf& stdbuf::in = g_inbuf;
iobuf& stdbuf::log = g_logbuf;
iobuf& stdbuf::err = g_errbuf;

static struct stdfile_initializer {
    stdfile_initializer() {
        ::SetConsoleCP(CP_UTF8);
        ::SetConsoleOutputCP(CP_UTF8);
    }
    ~stdfile_initializer() {
        g_errbuf.detach();
        g_logbuf.detach();
        g_inbuf.detach();
        g_outbuf.detach();
    }
} g_stdinit;
