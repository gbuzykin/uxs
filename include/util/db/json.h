#pragma once

#include "util/io/iobuf.h"
#include "value.h"

#include <vector>

namespace util {
namespace db {
namespace json {

class reader {
 public:
    explicit reader(iobuf& input);
    int read(value& v);

 private:
    iobuf& input_;
    char* first_ = nullptr;
    std::vector<char> buf_;
    std::vector<int> state_stack_;

    struct token_val_t {
        std::string str;
        uint64_t u64;
        double dbl;
    };

    enum { kEof = 0, kNull = 1000, kTrue, kFalse, kPosInt, kNegInt, kDouble, kString };

    int parse_value(value& v, int tk, token_val_t& tk_val);
    int parse_array(value& v, token_val_t& tk_val);
    int parse_object(value& v, token_val_t& tk_val);
    int parse_token(token_val_t& tk_val);
    static value token_to_value(int tk, const token_val_t& tk_val);
};

class writer {
 public:
    explicit writer(iobuf& output) : output_(output) {}

    void write(const value& v, unsigned indent = 0);

 private:
    iobuf& output_;

    void fmt_array(const value& v, unsigned indent);
    void fmt_object(const value& v, unsigned indent);
};

}  // namespace json
}  // namespace db
}  // namespace util
