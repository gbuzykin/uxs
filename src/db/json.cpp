#include "uxs/format.h"
#include "uxs/impl/db/json_impl.h"

void uxs::db::json::detail::report_error(unsigned ln, const char* message) {
    throw uxs::db::database_error(uxs::format("{}: {}", ln, message));
}

namespace uxs {
namespace db {
namespace json {
#define LEXEGEN_DATA_ATTRS
#define LEXEGEN_FUNC_ATTRS
namespace lex_detail {
#include "json_lex_analyzer.inl"
}
template UXS_EXPORT value parse(ibuf&, const std::allocator<char>&);
template UXS_EXPORT wvalue parse(ibuf&, const std::allocator<wchar_t>&);
template UXS_EXPORT value parse(wibuf&, const std::allocator<char>&);
template UXS_EXPORT wvalue parse(wibuf&, const std::allocator<wchar_t>&);
namespace detail {
template UXS_EXPORT token_t lexer<char>::lex(std::string_view&);
template UXS_EXPORT token_t lexer<wchar_t>::lex(std::wstring_view&);
template UXS_EXPORT void write_impl(membuffer& out, const value&);
template UXS_EXPORT void write_impl(membuffer& out, const wvalue&);
template UXS_EXPORT void write_impl(wmembuffer& out, const value&);
template UXS_EXPORT void write_impl(wmembuffer& out, const wvalue&);
template UXS_EXPORT void write_formatted_impl(membuffer& out, const value&, json_fmt_opts, unsigned);
template UXS_EXPORT void write_formatted_impl(membuffer& out, const wvalue&, json_fmt_opts, unsigned);
template UXS_EXPORT void write_formatted_impl(wmembuffer& out, const value&, json_fmt_opts, unsigned);
template UXS_EXPORT void write_formatted_impl(wmembuffer& out, const wvalue&, json_fmt_opts, unsigned);
}  // namespace detail
}  // namespace json
}  // namespace db
}  // namespace uxs
