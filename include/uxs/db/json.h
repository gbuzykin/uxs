#pragma once

#include "database_error.h"

#include "uxs/io/iomembuffer.h"

namespace uxs {
namespace db {
template<typename CharT, typename Alloc>
class basic_value;

namespace json {

enum class token_t : int {
    eof = 0,
    array = '[',
    object = '{',
    null_value = 256,
    true_value,
    false_value,
    integer_number,
    negative_integer_number,
    floating_point_number,
    string,
};

enum class parse_step { into = 0, over, stop };

namespace detail {
struct parser {
    ibuf& in;
    unsigned ln = 1;
    inline_dynbuffer str;
    inline_dynbuffer stash;
    inline_basic_dynbuffer<std::int8_t> stack;
    UXS_EXPORT explicit parser(ibuf& in);
    UXS_EXPORT token_t lex(std::string_view& lval);
};
}  // namespace detail

template<typename ValueFunc, typename ArrItemFunc, typename ObjItemFunc, typename PopFunc>
void read(ibuf& in, const ValueFunc& fn_value, const ArrItemFunc& fn_arr_item, const ObjItemFunc& fn_obj_item,
          const PopFunc& fn_pop) {
    if (in.peek() == ibuf::traits_type::eof()) { throw database_error("empty input"); }

    std::string_view lval;
    detail::parser state(in);
    inline_basic_dynbuffer<char, 32> stack;

    auto fn_value_checked = [&state, &fn_value](token_t tt, std::string_view lval) -> parse_step {
        if (tt >= token_t::null_value || tt == token_t::array || tt == token_t::object) { return fn_value(tt, lval); }
        throw database_error(to_string(state.ln) + ": invalid value or unexpected character");
    };

    auto tt = state.lex(lval);
    if (fn_value_checked(tt, lval) != parse_step::into) { return; }
    if (tt >= token_t::null_value) { return; }

    bool comma = false;
    auto current = tt;

loop:
    tt = state.lex(lval);
    if (current == token_t::array) {
        if (comma || tt != token_t(']')) {
            while (true) {
                fn_arr_item();
                const auto ret = fn_value_checked(tt, lval);
                if (ret == parse_step::into) {
                    if (tt < token_t::null_value) {
                        stack += '[', current = tt;
                        comma = false;
                        goto loop;
                    }
                } else if (ret == parse_step::stop) {
                    return;
                }
                if ((tt = state.lex(lval)) == token_t(']')) { break; }
                if (tt != token_t(',')) { throw database_error(to_string(state.ln) + ": expected `,` or `]`"); }
                tt = state.lex(lval);
            }
        }
    } else if (comma || tt != token_t('}')) {
        while (true) {
            if (tt != token_t::string) { throw database_error(to_string(state.ln) + ": expected valid string"); }
            fn_obj_item(lval);
            if ((tt = state.lex(lval)) != token_t(':')) {
                throw database_error(to_string(state.ln) + ": expected `:`");
            }
            tt = state.lex(lval);
            const auto ret = fn_value_checked(tt, lval);
            if (ret == parse_step::into) {
                if (tt < token_t::null_value) {
                    stack += '{', current = tt;
                    comma = false;
                    goto loop;
                }
            } else if (ret == parse_step::stop) {
                return;
            }
            if ((tt = state.lex(lval)) == token_t('}')) { break; }
            if (tt != token_t(',')) { throw database_error(to_string(state.ln) + ": expected `,` or `}`"); }
            tt = state.lex(lval);
        }
    }

    while (!stack.empty()) {
        current = token_t(stack.back());
        stack.pop_back();
        fn_pop();
        const char close_char = current == token_t::array ? ']' : '}';
        if ((tt = state.lex(lval)) != token_t(close_char)) {
            if (tt != token_t(',')) {
                throw database_error(to_string(state.ln) + ": expected `,` or `" + close_char + "`");
            }
            comma = true;
            goto loop;
        }
    }
}

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
UXS_EXPORT basic_value<CharT, Alloc> read(ibuf& in, const Alloc& al = Alloc());

namespace detail {
template<typename CharT>
struct writer {
    basic_membuffer<CharT>& out;
    unsigned indent_size;
    char object_ws_char;
    char array_ws_char;
    char indent_char;
    template<typename ValueCharT, typename Alloc>
    UXS_EXPORT void do_write(const basic_value<ValueCharT, Alloc>& v, unsigned indent);
};
}  // namespace detail

template<typename CharT, typename ValueCharT, typename Alloc>
void write(basic_membuffer<CharT>& out, const basic_value<ValueCharT, Alloc>& v, unsigned indent_size = 0,
           char object_ws_char = ' ', char array_ws_char = ' ', char indent_char = ' ', unsigned indent = 0) {
    detail::writer<CharT> writer{out, indent_size, object_ws_char, array_ws_char, indent_char};
    writer.do_write(v, indent);
}

template<typename CharT, typename ValueCharT, typename Alloc>
void write(basic_iobuf<CharT>& out, const basic_value<ValueCharT, Alloc>& v, unsigned indent_size = 0,
           char object_ws_char = ' ', char array_ws_char = ' ', char indent_char = ' ', unsigned indent = 0) {
    basic_iomembuffer<CharT> buf(out);
    detail::writer<CharT> writer{buf, indent_size, object_ws_char, array_ws_char, indent_char};
    writer.do_write(v, indent);
}

}  // namespace json
}  // namespace db

template<typename CharT, typename ValueCharT, typename Alloc>
struct formatter<db::basic_value<ValueCharT, Alloc>, CharT> {
 private:
    unsigned indent_size_ = 0;
    char object_ws_char_ = ' ';
    char array_ws_char_ = ' ';
    char indent_char_ = ' ';
    std::size_t indent_size_arg_id_ = dynamic_extent;

 public:
    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        if (it == ctx.end() || *it != ':') { return it; }
        if (++it == ctx.end()) { return it; }
        if (*it == '.') {
            auto it0 = it++;
            if (!ParseCtx::parse_integral_parameter(ctx, it, indent_size_, indent_size_arg_id_)) { return it0; }
            if (++it == ctx.end()) { return it; }
        }
        switch (*it) {
            case 't': {
                indent_char_ = '\t';
                if (++it == ctx.end() || (*it != 'a' && *it != 'A')) { ParseCtx::syntax_error(); }
                object_ws_char_ = '\n';
                if (*it == 'A') { array_ws_char_ = '\n'; }
            } break;
            case 'a': object_ws_char_ = '\n'; break;
            case 'A': object_ws_char_ = array_ws_char_ = '\n'; break;
            default: return it;
        }
        return it + 1;
    }

    template<typename FmtCtx>
    void format(FmtCtx& ctx, const db::basic_value<ValueCharT, Alloc>& val) const {
        unsigned indent_size = indent_size_;
        if (indent_size_arg_id_ != dynamic_extent) {
            indent_size = ctx.arg(indent_size_arg_id_).template get_unsigned<decltype(indent_size)>();
        }
        db::json::write(ctx.out(), val, indent_size_, object_ws_char_, array_ws_char_, indent_char_);
    }
};

}  // namespace uxs
