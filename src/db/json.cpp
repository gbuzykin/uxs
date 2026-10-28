#include "uxs/format.h"
#include "uxs/impl/db/json_impl.h"

#define LEXEGEN_DATA_DECLARATOR(type, name) type uxs::db::json::lex_detail::name
#include "json_lex_analyzer.inl"

uxs::db::json::json_error::json_error(const char* message) : std::runtime_error(message) {}
uxs::db::json::json_error::json_error(const std::string& message) : std::runtime_error(message) {}
const char* uxs::db::json::json_error::what() const noexcept { return std::runtime_error::what(); }

[[noreturn]] void uxs::db::json::detail::report_error(unsigned ln, const char* message) {
    throw uxs::db::json::json_error(uxs::format("{}: {}", ln, message));
}

UXS_DB_JSON_INSTANTIATE_LEXER_IMPLEMENTATION(char);
UXS_DB_JSON_INSTANTIATE_IMPLEMENTATION(char, uxs::db::value);
#if UXS_USE_WCHAR_T != 0
UXS_DB_JSON_INSTANTIATE_LEXER_IMPLEMENTATION(wchar_t);
UXS_DB_JSON_INSTANTIATE_IMPLEMENTATION(char, uxs::db::wvalue);
UXS_DB_JSON_INSTANTIATE_IMPLEMENTATION(wchar_t, uxs::db::value);
UXS_DB_JSON_INSTANTIATE_IMPLEMENTATION(wchar_t, uxs::db::wvalue);
#endif  // UXS_USE_WCHAR_T != 0
