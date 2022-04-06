#include "util/format_impl.h"

using namespace util;

namespace util {
namespace detail {
template std::string& fmt_append_string(std::string_view, std::string&, fmt_state&);
template char_buf_appender& fmt_append_string(std::string_view, char_buf_appender&, fmt_state&);
template char_n_buf_appender& fmt_append_string(std::string_view, char_n_buf_appender&, fmt_state&);
}  // namespace detail

template std::string& format_append_v(std::string_view, std::string&,
                                      span<const detail::fmt_arg_list_item<std::string>>);
template char_buf_appender& format_append_v(std::string_view, char_buf_appender&,
                                            span<const detail::fmt_arg_list_item<char_buf_appender>>);
template char_n_buf_appender& format_append_v(std::string_view, char_n_buf_appender&,
                                              span<const detail::fmt_arg_list_item<char_n_buf_appender>>);

namespace detail {
template std::wstring& fmt_append_string(std::wstring_view, std::wstring&, fmt_state&);
template wchar_buf_appender& fmt_append_string(std::wstring_view, wchar_buf_appender&, fmt_state&);
template wchar_n_buf_appender& fmt_append_string(std::wstring_view, wchar_n_buf_appender&, fmt_state&);
}  // namespace detail

template std::wstring& format_append_v(std::wstring_view, std::wstring&,
                                       span<const detail::fmt_arg_list_item<std::wstring>>);
template wchar_buf_appender& format_append_v(std::wstring_view, wchar_buf_appender&,
                                             span<const detail::fmt_arg_list_item<wchar_buf_appender>>);
template wchar_n_buf_appender& format_append_v(std::wstring_view, wchar_n_buf_appender&,
                                               span<const detail::fmt_arg_list_item<wchar_n_buf_appender>>);

}  // namespace util
