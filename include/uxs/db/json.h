#pragma once

#include "value.h"

#include "uxs/io/iobuf.h"
#include "uxs/stringcvt.h"

namespace uxs {
namespace db {
namespace json {

class UXS_EXPORT reader {
 public:
    explicit reader(iobuf& input) : input_(input) {}

    template<typename CharT = char, typename Alloc = std::allocator<CharT>>
    basic_value<CharT, Alloc> read(const Alloc& al = Alloc());

 private:
    iobuf& input_;
    int n_ln_ = 0;
    inline_dynbuffer str_;
    inline_dynbuffer stash_;
    basic_inline_dynbuffer<int8_t> state_stack_;

    enum { kEof = 0, kNull = 256, kTrue, kFalse, kInteger, kNegInteger, kDouble, kString };

    void init_parser();
    int parse_token(std::string_view& lval);
};

class UXS_EXPORT writer {
 public:
    explicit writer(iobuf& output, unsigned indent_sz = 4, char indent_ch = ' ')
        : output_(output), indent_size_(indent_sz), indent_char_(indent_ch) {}

    template<typename CharT, typename Alloc>
    void write(const basic_value<CharT, Alloc>& v);

 private:
    iobuf& output_;
    unsigned indent_size_;
    char indent_char_;
};

}  // namespace json
}  // namespace db
}  // namespace uxs
