#pragma once

#include "util/stringcvt.h"
#include "value.h"

namespace util {
namespace db {
namespace json {

class reader {
 public:
    explicit reader(iobuf& input) : input_(input) {}
    value read();

 private:
    iobuf& input_;
    dynbuffer str_;
    dynbuffer stash_;
    basic_dynbuffer<int8_t> state_stack_;

    enum { kEof = 0, kNull = 1000, kTrue, kFalse, kInteger, kDouble, kString };

    int parse_token(std::string_view& lval);
};

class writer {
 public:
    explicit writer(iobuf& output, unsigned indent_sz = 4, char indent_ch = ' ')
        : output_(output), indent_size_(indent_sz), indent_char_(indent_ch) {}

    void write(const value& v);

 private:
    iobuf& output_;
    unsigned indent_size_;
    char indent_char_;
};

}  // namespace json
}  // namespace db
}  // namespace util
