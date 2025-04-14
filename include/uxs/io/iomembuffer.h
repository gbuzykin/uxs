#pragma once

#include "iobuf.h"

#include "uxs/string_cvt.h"

namespace uxs {

template<typename Ty>
class basic_iomembuffer final : public basic_membuffer<Ty> {
 public:
    explicit basic_iomembuffer(basic_iobuf<Ty>& out) noexcept
        : basic_membuffer<Ty>(out.first(), out.pos(), out.capacity()), out_(out) {}
    ~basic_iomembuffer() override { flush(); }
    void flush() noexcept { out_.setpos(this->size()); }

 private:
    basic_iobuf<Ty>& out_;

    std::size_t try_grow(std::size_t /*extra*/) override {
        flush();
        if (!out_.reserve().good()) { return 0; }
        this->reset(out_.first(), out_.pos(), out_.capacity());
        return this->avail();
    }
};

using iomembuffer = basic_iomembuffer<char>;
using wiomembuffer = basic_iomembuffer<wchar_t>;

}  // namespace uxs
