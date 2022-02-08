#include "util/format.h"

using namespace util;

std::string sformat::str() const {
    std::string result;
    auto p = fmt_.begin(), p0 = p;
    while (p < fmt_.end()) {
        if (*p != '%') {
            ++p;
            continue;
        }
        result.append(p0, p++);
        if (p == fmt_.end()) {
            return result;
        } else if (std::isdigit(static_cast<unsigned char>(*p))) {
            size_t n = static_cast<size_t>(*p++ - '0');
            while (p < fmt_.end() && std::isdigit(static_cast<unsigned char>(*p))) {
                n = 10 * n + static_cast<size_t>(*p++ - '0');
            }
            if (n > 0 && n <= args_.size()) { result.append(args_[n - 1]); }
            p0 = p;
        } else {
            p0 = p++;
        }
    }
    result.append(p0, p);
    return result;
}
