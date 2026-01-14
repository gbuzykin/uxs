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

struct json_fmt_opts {
    char object_ws_char = ' ';
    char array_ws_char = ' ';
    char indent_char = ' ';
    unsigned indent_size = 2;
};

namespace detail {
struct lexer {
    ibuf& in;
    unsigned ln = 1;
    inline_dynbuffer str;
    inline_basic_dynbuffer<char, 32> stash;
    inline_basic_dynbuffer<std::int8_t, 32> stack;
    UXS_EXPORT explicit lexer(ibuf& in);
    UXS_EXPORT token_t lex(std::string_view& lval);
};
}  // namespace detail

template<typename ValueFunc, typename ArrItemFunc, typename ObjItemFunc, typename PopFunc>
void read(ibuf& in, const ValueFunc& fn_value, const ArrItemFunc& fn_arr_item, const ObjItemFunc& fn_obj_item,
          const PopFunc& fn_pop) {
    detail::lexer lexer(in);
    inline_basic_dynbuffer<char, 32> stack;

    const auto fn_value_checked = [&lexer, &fn_value](token_t tt, std::string_view lval) -> parse_step {
        if (tt >= token_t::null_value || tt == token_t('[') || tt == token_t('{')) { return fn_value(tt, lval); }
        throw database_error(to_string(lexer.ln) + ": invalid value or unexpected character");
    };

    std::string_view lval;
    auto tt = lexer.lex(lval);
    if (fn_value_checked(tt, lval) != parse_step::into) { return; }
    if (tt >= token_t::null_value) { return; }

    bool comma = false;
    auto current = tt;

loop:
    tt = lexer.lex(lval);
    if (current == token_t('[')) {
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
                if ((tt = lexer.lex(lval)) == token_t(']')) { break; }
                if (tt != token_t(',')) { throw database_error(to_string(lexer.ln) + ": expected `,` or `]`"); }
                tt = lexer.lex(lval);
            }
        }
    } else if (comma || tt != token_t('}')) {
        while (true) {
            if (tt != token_t::string) { throw database_error(to_string(lexer.ln) + ": expected valid string"); }
            fn_obj_item(lval);
            if (lexer.lex(lval) != token_t(':')) { throw database_error(to_string(lexer.ln) + ": expected `:`"); }
            tt = lexer.lex(lval);
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
            if ((tt = lexer.lex(lval)) == token_t('}')) { break; }
            if (tt != token_t(',')) { throw database_error(to_string(lexer.ln) + ": expected `,` or `}`"); }
            tt = lexer.lex(lval);
        }
    }

    while (!stack.empty()) {
        current = token_t(stack.back());
        stack.pop_back();
        fn_pop();
        const char close_char = current == token_t('[') ? ']' : '}';
        if ((tt = lexer.lex(lval)) != token_t(close_char)) {
            if (tt != token_t(',')) {
                throw database_error(to_string(lexer.ln) + ": expected `,` or `" + close_char + "`");
            }
            comma = true;
            goto loop;
        }
    }
}

template<typename CharT = char, typename Alloc = std::allocator<CharT>>
UXS_EXPORT basic_value<CharT, Alloc> read(ibuf& in, const Alloc& al = Alloc());

template<typename CharT, typename ValueCharT, typename Alloc>
UXS_EXPORT void write(basic_membuffer<CharT>& out, const basic_value<ValueCharT, Alloc>& v);

template<typename CharT, typename ValueCharT, typename Alloc>
UXS_EXPORT void write_formatted(basic_membuffer<CharT>& out, const basic_value<ValueCharT, Alloc>& v,
                                json_fmt_opts opts = {}, unsigned indent = 0);

template<typename CharT, typename ValueCharT, typename Alloc>
void write(basic_iobuf<CharT>& out, const basic_value<ValueCharT, Alloc>& v) {
    basic_iomembuffer<CharT> buf(out);
    write(buf, v);
}

template<typename CharT, typename ValueCharT, typename Alloc>
void write_formatted(basic_iobuf<CharT>& out, const basic_value<ValueCharT, Alloc>& v, json_fmt_opts opts = {},
                     unsigned indent = 0) {
    basic_iomembuffer<CharT> buf(out);
    write_formatted(buf, v, opts, indent);
}

}  // namespace json
}  // namespace db

template<typename CharT, typename ValueCharT, typename Alloc>
struct formatter<db::basic_value<ValueCharT, Alloc>, CharT> {
 private:
    db::json::json_fmt_opts opts_;
    std::size_t indent_size_arg_id_ = unspecified_size;
    bool use_condensed_ = false;

 public:
    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        if (it == ctx.end() || *it != ':') { return it; }
        if (++it == ctx.end()) { return it; }
        if (*it == '.') {
            auto it0 = it++;
            if (!ParseCtx::parse_integral_parameter(ctx, it, opts_.indent_size, indent_size_arg_id_)) { return it0; }
            if (++it == ctx.end()) { return it; }
        }
        switch (*it) {
            case 't': {
                opts_.indent_char = '\t';
                if (++it == ctx.end() || (*it != 'a' && *it != 'A')) { ParseCtx::syntax_error(); }
                opts_.object_ws_char = '\n';
                if (*it == 'A') { opts_.array_ws_char = '\n'; }
            } break;
            case 'c': use_condensed_ = true; break;
            case 'a': opts_.object_ws_char = '\n'; break;
            case 'A': opts_.object_ws_char = opts_.array_ws_char = '\n'; break;
            default: return it;
        }
        return it + 1;
    }

    template<typename FmtCtx>
    void format(FmtCtx& ctx, const db::basic_value<ValueCharT, Alloc>& val) const {
        unsigned indent_size = opts_.indent_size;
        if (indent_size_arg_id_ != unspecified_size) {
            indent_size = ctx.arg(indent_size_arg_id_).template get_unsigned<decltype(indent_size)>();
        }
        return use_condensed_ ? db::json::write(ctx.out(), val) : db::json::write_formatted(ctx.out(), val, opts_);
    }
};

}  // namespace uxs
