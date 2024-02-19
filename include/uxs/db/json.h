#pragma once

#include "database_error.h"

#include "uxs/format.h"

namespace uxs {
namespace db {
template<typename CharT, typename Alloc>
class basic_value;

namespace json {

enum class token_t : int {
    eof = 0,
    array = '[',
    object = '{',
    null = 256,
    true_value,
    false_value,
    integer_number,
    negative_integer_number,
    real_number,
    string
};

enum class next_action_type : int { step_into = 0, step_over, stop };

class reader {
 public:
    UXS_EXPORT explicit reader(ibuf& input);

    template<typename ValueFunc, typename ArrItemFunc, typename ObjItemFunc, typename PopFunc>
    void read(const ValueFunc& fn_value, const ArrItemFunc& fn_arr_item, const ObjItemFunc& fn_obj_item,
              const PopFunc& fn_pop, token_t tk_val = token_t::eof);

    template<typename CharT = char, typename Alloc = std::allocator<CharT>>
    UXS_EXPORT basic_value<CharT, Alloc> read(token_t tk_val = token_t::eof, const Alloc& al = Alloc());

 private:
    ibuf& input_;
    int n_ln_ = 1;
    inline_dynbuffer str_;
    inline_dynbuffer stash_;
    inline_basic_dynbuffer<std::int8_t> state_stack_;

    int parse_token(std::string_view& lval);
};

class writer {
 public:
    explicit writer(iobuf& output, unsigned indent_sz = 4, char indent_ch = ' ')
        : output_(output), indent_size_(indent_sz), indent_char_(indent_ch) {}

    template<typename CharT, typename Alloc>
    UXS_EXPORT void write(const basic_value<CharT, Alloc>& v, unsigned indent = 0);

 private:
    iobuf& output_;
    unsigned indent_size_;
    char indent_char_;
};

template<typename ValueFunc, typename ArrItemFunc, typename ObjItemFunc, typename PopFunc>
void reader::read(const ValueFunc& fn_value, const ArrItemFunc& fn_arr_item, const ObjItemFunc& fn_obj_item,
                  const PopFunc& fn_pop, token_t tk_val) {
    if (input_.peek() == ibuf::traits_type::eof()) { throw database_error("empty input"); }

    auto fn_value_checked = [this, &fn_value](int tt, std::string_view lval) -> next_action_type {
        if (tt >= static_cast<int>(token_t::null) || tt == '[' || tt == '{') {
            return fn_value(static_cast<token_t>(tt), lval);
        }
        throw database_error(format("{}: invalid value or unexpected character", n_ln_));
    };

    std::string_view lval;
    inline_basic_dynbuffer<char, 32> stack;
    if (tk_val == token_t::eof) {
        int tt = parse_token(lval);
        if (fn_value_checked(tt, lval) != next_action_type::step_into) { return; }
        tk_val = static_cast<token_t>(tt);
    }

    bool comma = false;
    if (tk_val >= token_t::null) { return; }

loop:
    int tt = parse_token(lval);
    if (tk_val == token_t::array) {
        if (comma || tt != ']') {
            while (true) {
                fn_arr_item();
                next_action_type ret = fn_value_checked(tt, lval);
                if (ret == next_action_type::step_into) {
                    if ((tk_val = static_cast<token_t>(tt)) < token_t::null) {
                        stack.push_back('[');
                        comma = false;
                        goto loop;
                    }
                } else if (ret == next_action_type::stop) {
                    return;
                }
                if ((tt = parse_token(lval)) == ']') { break; }
                if (tt != ',') { throw database_error(format("{}: expected `,` or `]`", n_ln_)); }
                tt = parse_token(lval);
            }
        }
    } else if (comma || tt != '}') {
        while (true) {
            if (tt != static_cast<int>(token_t::string)) {
                throw database_error(format("{}: expected valid string", n_ln_));
            }
            fn_obj_item(lval);
            if ((tt = parse_token(lval)) != ':') { throw database_error(format("{}: expected `:`", n_ln_)); }
            tt = parse_token(lval);
            next_action_type ret = fn_value_checked(tt, lval);
            if (ret == next_action_type::step_into) {
                if ((tk_val = static_cast<token_t>(tt)) < token_t::null) {
                    stack.push_back('{');
                    comma = false;
                    goto loop;
                }
            } else if (ret == next_action_type::stop) {
                return;
            }
            if ((tt = parse_token(lval)) == '}') { break; }
            if (tt != ',') { throw database_error(format("{}: expected `,` or `}}`", n_ln_)); }
            tt = parse_token(lval);
        }
    }

    while (!stack.empty()) {
        tk_val = static_cast<token_t>(stack.back());
        stack.pop_back();
        fn_pop();
        char close_char = tk_val == token_t::array ? ']' : '}';
        if ((tt = parse_token(lval)) != close_char) {
            if (tt != ',') { throw database_error(format("{}: expected `,` or `{}`", n_ln_, close_char)); }
            comma = true;
            goto loop;
        }
    }
}

}  // namespace json
}  // namespace db
}  // namespace uxs
