#pragma once

#include "string_ext.h"

#include <iostream>

#if __cplusplus < 201703L
template<typename Ty>
std::basic_ostream<Ty>& operator<<(std::basic_ostream<Ty>& os, std::basic_string_view<Ty> s) {
    os.write(s.data(), s.size());
    return os;
}
#endif  // __cplusplus < 201703L

#ifdef USE_QT
inline QDataStream& operator<<(QDataStream& os, std::string_view s) {
    os << s.size();
    os.writeRawData(s.data(), static_cast<int>(s.size()));
    return os;
}

inline QDataStream& operator<<(QDataStream& os, const std::string& s) { return os << static_cast<std::string_view>(s); }

CORE_EXPORT QDataStream& operator>>(QDataStream& is, std::string& s);
#endif  // USE_QT

namespace util {}
