#include "uxs/format.h"
#include "uxs/impl/db/xml_impl.h"

#define LEXEGEN_DATA_DECLARATOR(type, name) type uxs::db::xml::lex_detail::name
#include "xml_lex_analyzer.inl"

uxs::db::xml::xml_error::xml_error(const char* message) : std::runtime_error(message) {}
uxs::db::xml::xml_error::xml_error(const std::string& message) : std::runtime_error(message) {}
const char* uxs::db::xml::xml_error::what() const noexcept { return std::runtime_error::what(); }

[[noreturn]] void uxs::db::xml::detail::report_error(unsigned ln, const char* message) {
    throw uxs::db::xml::xml_error(uxs::format("{}: {}", ln, message));
}

UXS_DB_XML_INSTANTIATE_LEXER_IMPLEMENTATION(char);
UXS_DB_XML_INSTANTIATE_IMPLEMENTATION(char, uxs::db::value);
#if UXS_USE_WCHAR_T != 0
UXS_DB_XML_INSTANTIATE_LEXER_IMPLEMENTATION(wchar_t);
UXS_DB_XML_INSTANTIATE_IMPLEMENTATION(char, uxs::db::wvalue);
UXS_DB_XML_INSTANTIATE_IMPLEMENTATION(wchar_t, uxs::db::value);
UXS_DB_XML_INSTANTIATE_IMPLEMENTATION(wchar_t, uxs::db::wvalue);
#endif  // UXS_USE_WCHAR_T != 0
