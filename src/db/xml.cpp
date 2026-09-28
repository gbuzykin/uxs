#include "uxs/format.h"
#include "uxs/impl/db/xml_impl.h"

[[noreturn]] void uxs::db::xml::detail::report_error(unsigned ln, const char* message) {
    throw uxs::db::database_error(uxs::format("{}: {}", ln, message));
}

#define LEXEGEN_DATA_DECLARATOR(type, name) type uxs::db::xml::lex_detail::name
#include "xml_lex_analyzer.inl"
#undef LEXEGEN_DATA_DECL_PREFIX

UXS_DB_XML_INSTANTIATE_LEXER_IMPLEMENTATION(char);
UXS_DB_XML_INSTANTIATE_IMPLEMENTATION(char, uxs::db::value);
#if UXS_USE_WCHAR_T != 0
UXS_DB_XML_INSTANTIATE_LEXER_IMPLEMENTATION(wchar_t);
UXS_DB_XML_INSTANTIATE_IMPLEMENTATION(char, uxs::db::wvalue);
UXS_DB_XML_INSTANTIATE_IMPLEMENTATION(wchar_t, uxs::db::value);
UXS_DB_XML_INSTANTIATE_IMPLEMENTATION(wchar_t, uxs::db::wvalue);
#endif  // UXS_USE_WCHAR_T != 0
