#pragma once

#include "value.h"

#include "uxs/stringcvt.h"

namespace uxs {
namespace db {
namespace json {

class reader {
 public:
    explicit reader(iobuf& input) : input_(input) {}
    value read();

 private:
    iobuf& input_;
    int n_ln_ = 0;
    inline_dynbuffer str_;
    inline_dynbuffer stash_;
    basic_inline_dynbuffer<int8_t> state_stack_;

    enum { kEof = 0, kNull = 256, kTrue, kFalse, kInteger, kNegInteger, kDouble, kString };

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
}  // namespace uxs
