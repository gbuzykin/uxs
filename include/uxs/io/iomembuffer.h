#pragma once

#include "iobuf.h"

#include "uxs/membuffer.h"

namespace uxs {

template<typename Ty>
class basic_iomembuffer : public basic_membuffer<Ty> {
 public:
    using size_type = typename basic_membuffer<Ty>::size_type;

    explicit basic_iomembuffer(basic_iobuf<Ty>& out) noexcept
        : basic_membuffer<Ty>(out.first(), out.pos(), out.capacity(), try_grow_impl), out_(out) {}
    ~basic_iomembuffer() { flush(); }
    void flush() noexcept { out_.setpos(this->size()); }

 private:
    basic_iobuf<Ty>& out_;

    void reset(Ty* data, size_type size, size_type capacity) noexcept {
        basic_membuffer<Ty>::reset(data, size, capacity);
    }

    static size_type try_grow_impl(basic_membuffer<Ty>& buf, size_type /*extra*/, bool /*track_size*/) {
        auto& iomembuf = static_cast<basic_iomembuffer&>(buf);
        iomembuf.flush();
        if (!iomembuf.out_.reserve().good()) { return 0; }
        iomembuf.reset(iomembuf.out_.first(), iomembuf.out_.pos(), iomembuf.out_.capacity());
        return buf.avail();
    }
};

using iomembuffer = basic_iomembuffer<char>;
using wiomembuffer = basic_iomembuffer<wchar_t>;

}  // namespace uxs
