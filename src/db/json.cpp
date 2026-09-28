#include "uxs/format.h"
#include "uxs/impl/db/json_impl.h"

[[noreturn]] void uxs::db::json::detail::report_error(unsigned ln, const char* message) {
    throw uxs::db::database_error(uxs::format("{}: {}", ln, message));
}

#define LEXEGEN_DATA_DECLARATOR(type, name) type uxs::db::json::lex_detail::name
#include "json_lex_analyzer.inl"
#undef LEXEGEN_DATA_DECLARATOR

UXS_DB_JSON_INSTANTIATE_LEXER_IMPLEMENTATION(char);
UXS_DB_JSON_INSTANTIATE_IMPLEMENTATION(char, uxs::db::value);
#if UXS_USE_WCHAR_T != 0
UXS_DB_JSON_INSTANTIATE_LEXER_IMPLEMENTATION(wchar_t);
UXS_DB_JSON_INSTANTIATE_IMPLEMENTATION(char, uxs::db::wvalue);
UXS_DB_JSON_INSTANTIATE_IMPLEMENTATION(wchar_t, uxs::db::value);
UXS_DB_JSON_INSTANTIATE_IMPLEMENTATION(wchar_t, uxs::db::wvalue);
#endif  // UXS_USE_WCHAR_T != 0
