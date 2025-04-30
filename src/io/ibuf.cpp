#include "uxs/impl/io/ibuf_impl.h"

namespace uxs {
iomode detail::iomode_from_str(const char* mode, iomode default_mode) noexcept {
    iomode result = default_mode;
    while (*mode) {
        switch (*mode) {
            case 'r': {
                result |= iomode::in;
                if (*(mode + 1) == '+') { result |= iomode::out, ++mode; }
            } break;
            case 'w': {
                result |= iomode::out | iomode::create | iomode::truncate;
                if (*(mode + 1) == '+') { result |= iomode::in, ++mode; }
            } break;
            case 'a': {
                result |= iomode::out | iomode::create | iomode::append;
                if (*(mode + 1) == '+') { result |= iomode::in, ++mode; }
            } break;
            case 'x': result |= iomode::exclusive; break;
            case 't': result |= iomode::text; break;
            case 'b': result &= ~iomode::text; break;
            case 'z': {
                result |= iomode::z_compr;
                const char level = *(mode + 1);
                if (level >= '0' && level <= '9') {
                    result |= static_cast<iomode>((level - '0') * static_cast<int>(iomode::z_compr_level));
                    ++mode;
                }
            } break;
            default: break;
        }
        ++mode;
    }
    return result;
}

template class basic_ibuf<char>;
template class basic_ibuf<wchar_t>;
template class basic_ibuf<std::uint8_t>;
}  // namespace uxs
