#include "uxs/format.h"
#include "uxs/impl/db/xml_impl.h"

void uxs::db::xml::detail::report_error(unsigned ln, const char* message) {
    throw uxs::db::database_error(uxs::format("{}: {}", ln, message));
}

namespace uxs {
namespace db {
namespace xml {
#define LEXEGEN_DATA_ATTRS
#define LEXEGEN_FUNC_ATTRS
namespace lex_detail {
#include "xml_lex_analyzer.inl"
}  // namespace lex_detail
template UXS_EXPORT value_class parser<char>::classify_value(const string_view_type&) noexcept;
template UXS_EXPORT value_class parser<wchar_t>::classify_value(const string_view_type&) noexcept;
template UXS_EXPORT value parser<char>::parse(string_view_type, const std::allocator<char>&);
template UXS_EXPORT wvalue parser<char>::parse(string_view_type, const std::allocator<wchar_t>&);
template UXS_EXPORT value parser<wchar_t>::parse(string_view_type, const std::allocator<char>&);
template UXS_EXPORT wvalue parser<wchar_t>::parse(string_view_type, const std::allocator<wchar_t>&);
namespace detail {
template UXS_EXPORT lex_token_t lexer<char>::lex(std::string_view&);
template UXS_EXPORT lex_token_t lexer<wchar_t>::lex(std::wstring_view&);
template UXS_EXPORT void write_impl(membuffer& out, const value&, std::string_view, xml_fmt_opts, unsigned);
template UXS_EXPORT void write_impl(membuffer& out, const wvalue&, std::wstring_view, xml_fmt_opts, unsigned);
template UXS_EXPORT void write_impl(wmembuffer& out, const value&, std::string_view, xml_fmt_opts, unsigned);
template UXS_EXPORT void write_impl(wmembuffer& out, const wvalue&, std::wstring_view, xml_fmt_opts, unsigned);
}  // namespace detail
}  // namespace xml
}  // namespace db
}  // namespace uxs
