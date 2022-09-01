#pragma once

#include "exception.h"

#include "uxs/format.h"
#include "uxs/io/iobuf.h"
#include "uxs/stringcvt.h"

namespace uxs {
namespace db {
template<typename CharT, typename Alloc>
class basic_value;

namespace json {

enum class token_t : int {
    kEof = 0,
    kArray = '[',
    kObject = '{',
    kNull = 256,
    kTrue,
    kFalse,
    kInteger,
    kNegInteger,
    kDouble,
    kString
};

enum class item_ret_code_t : int { kStepInto = 0, kStepOver, kBreak };

class UXS_EXPORT reader {
 public:
    explicit reader(iobuf& input);

    template<typename ValueFunc, typename ArrItemFunc, typename ObjItemFunc, typename PopFunc>
    void read(const ValueFunc& fn_value, const ArrItemFunc& fn_arr_item, const ObjItemFunc& fn_obj_item,
              const PopFunc& fn_pop, token_t tk_val = token_t::kEof);

    template<typename CharT = char, typename Alloc = std::allocator<CharT>>
    basic_value<CharT, Alloc> read(token_t tk_val = token_t::kEof, const Alloc& al = Alloc());

 private:
    iobuf& input_;
    int n_ln_ = 1;
    inline_dynbuffer str_;
    inline_dynbuffer stash_;
    basic_inline_dynbuffer<int8_t> state_stack_;

    int parse_token(std::string_view& lval);
};

class UXS_EXPORT writer {
 public:
    explicit writer(iobuf& output, unsigned indent_sz = 4, char indent_ch = ' ')
        : output_(output), indent_size_(indent_sz), indent_char_(indent_ch) {}

    template<typename CharT, typename Alloc>
    void write(const basic_value<CharT, Alloc>& v, unsigned indent = 0);

 private:
    iobuf& output_;
    unsigned indent_size_;
    char indent_char_;
};

template<typename ValueFunc, typename ArrItemFunc, typename ObjItemFunc, typename PopFunc>
void reader::read(const ValueFunc& fn_value, const ArrItemFunc& fn_arr_item, const ObjItemFunc& fn_obj_item,
                  const PopFunc& fn_pop, token_t tk_val) {
    if (input_.peek() == iobuf::traits_type::eof()) { throw exception("empty input"); }

    auto fn_value_checked = [this, &fn_value](int tt, std::string_view lval) -> item_ret_code_t {
        if (tt >= static_cast<int>(token_t::kNull) || tt == '[' || tt == '{') {
            return fn_value(static_cast<token_t>(tt), lval);
        }
        throw exception(format("{}: invalid value or unexpected character", n_ln_));
    };

    std::string_view lval;
    basic_inline_dynbuffer<char, 32> stack;
    if (tk_val == token_t::kEof) {
        int tt = parse_token(lval);
        if (fn_value_checked(tt, lval) != item_ret_code_t::kStepInto) { return; }
        tk_val = static_cast<token_t>(tt);
    }

    bool comma = false;
    if (tk_val >= token_t::kNull) { return; }

loop:
    int tt = parse_token(lval);
    if (tk_val == token_t::kArray) {
        if (comma || tt != ']') {
            while (true) {
                fn_arr_item();
                item_ret_code_t ret = fn_value_checked(tt, lval);
                if (ret == item_ret_code_t::kStepInto) {
                    if ((tk_val = static_cast<token_t>(tt)) < token_t::kNull) {
                        stack.push_back('[');
                        comma = false;
                        goto loop;
                    }
                } else if (ret == item_ret_code_t::kBreak) {
                    return;
                }
                if ((tt = parse_token(lval)) == ']') { break; }
                if (tt != ',') { throw exception(format("{}: expected `,` or `]`", n_ln_)); }
                tt = parse_token(lval);
            }
        }
    } else if (comma || tt != '}') {
        while (true) {
            if (tt != static_cast<int>(token_t::kString)) {
                throw exception(format("{}: expected valid string", n_ln_));
            }
            fn_obj_item(lval);
            if ((tt = parse_token(lval)) != ':') { throw exception(format("{}: expected `:`", n_ln_)); }
            tt = parse_token(lval);
            item_ret_code_t ret = fn_value_checked(tt, lval);
            if (ret == item_ret_code_t::kStepInto) {
                if ((tk_val = static_cast<token_t>(tt)) < token_t::kNull) {
                    stack.push_back('{');
                    comma = false;
                    goto loop;
                }
            } else if (ret == item_ret_code_t::kBreak) {
                return;
            }
            if ((tt = parse_token(lval)) == '}') { break; }
            if (tt != ',') { throw exception(format("{}: expected `,` or `}}`", n_ln_)); }
            tt = parse_token(lval);
        }
    }

    while (!stack.empty()) {
        tk_val = static_cast<token_t>(stack.back());
        stack.pop_back();
        fn_pop();
        char close_char = tk_val == token_t::kArray ? ']' : '}';
        if ((tt = parse_token(lval)) != close_char) {
            if (tt != ',') { throw exception(format("{}: expected `,` or `{}`", n_ln_, close_char)); }
            comma = true;
            goto loop;
        }
    }
}

}  // namespace json
}  // namespace db
}  // namespace uxs
