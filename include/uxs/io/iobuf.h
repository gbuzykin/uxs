#pragma once

#include "iostate.h"

#include "uxs/span.h"
#include "uxs/string_view.h"

namespace uxs {

template<typename CharT>
class UXS_EXPORT basic_iobuf : public iostate {
    static_assert(std::is_integral<CharT>::value, "uxs::basic_iobuf must have integral character type");

 public:
    using char_type = CharT;
    using traits_type = std::char_traits<CharT>;
    using size_type = size_t;
    using int_type = typename traits_type::int_type;
    using pos_type = typename traits_type::pos_type;
    using off_type = typename traits_type::off_type;

    basic_iobuf() = default;
    explicit basic_iobuf(iomode mode) : iostate(mode) {}
    basic_iobuf(iomode mode, iostate_bits state) : iostate(mode, state) {}
    virtual ~basic_iobuf() = default;
    basic_iobuf(const basic_iobuf&) = delete;
    basic_iobuf& operator=(const basic_iobuf&) = delete;
    basic_iobuf(basic_iobuf&& other) NOEXCEPT;
    basic_iobuf& operator=(basic_iobuf&& other) NOEXCEPT;

    size_type in_avail() const { return last_ - curr_; }
    bool in_avail_empty() const { return curr_ == last_; }
    const char_type* in_avail_first() const { return curr_; }
    const char_type* in_avail_last() const { return last_; }
    span<const char_type> in_avail_view() const { return as_span(curr_, in_avail()); }

    int_type peek() {
        if (curr_ == last_ && (!this->good() || underflow() < 0)) {
            this->setstate(iostate_bits::kEof | iostate_bits::kFail);
            return traits_type::eof();
        }
        return traits_type::to_int_type(*curr_);
    }

    int_type get() {
        if (curr_ == last_ && (!this->good() || underflow() < 0)) {
            this->setstate(iostate_bits::kEof | iostate_bits::kFail);
            return traits_type::eof();
        }
        return traits_type::to_int_type(*curr_++);
    }

    basic_iobuf& unget() {
        this->setstate(this->rdstate() & ~iostate_bits::kEof);
        if (curr_ == first_) {
            this->setstate(iostate_bits::kFail);
        } else {
            --curr_;
        }
        return *this;
    }

    basic_iobuf& unget(char_type ch) {
        this->setstate(this->rdstate() & ~iostate_bits::kEof);
        if (curr_ == first_ && (!this->good() || ungetfail(ch) < 0)) {
            this->setstate(iostate_bits::kFail);
        } else {
            *--curr_ = ch;
        }
        return *this;
    }

    basic_iobuf& put(char_type ch) {
        if (curr_ != last_) {
            *curr_++ = ch;
        } else if (!this->good() || overflow(ch) < 0) {
            this->setstate(iostate_bits::kBad);
        }
        return *this;
    }

    basic_iobuf& bump(size_type n) {
        assert(n <= static_cast<size_type>(last_ - curr_));
        curr_ += n;
        return *this;
    }

    size_type read(span<char_type> s);
    size_type skip(size_type n);
    basic_iobuf& write(span<const char_type> s);
    basic_iobuf& write(const char_type* cstr) { return write(std::basic_string_view<char_type>(cstr)); }
    basic_iobuf& fill_n(size_type n, char_type ch);
    basic_iobuf& flush();
    basic_iobuf& endl() { return put('\n').flush(); }

    pos_type seek(off_type off, seekdir dir = seekdir::kBeg);
    pos_type tell() {
        if (this->fail()) { return -1; }
        return seekimpl(0, seekdir::kCurr);
    }

 protected:
    virtual int underflow();
    virtual int ungetfail(char_type ch);
    virtual int overflow(char_type ch);
    virtual pos_type seekimpl(off_type off, seekdir dir);
    virtual int sync();

    char_type* first() const { return first_; }
    char_type* curr() const { return curr_; }
    char_type* last() const { return last_; }

    void setcurr(char_type* curr) { curr_ = curr; }
    void putcurr(char_type ch) { *curr_++ = ch; }
    void setview(char_type* first, char_type* curr, char_type* last) {
        first_ = first;
        curr_ = curr;
        last_ = last;
    }

 private:
    char_type* first_ = nullptr;
    char_type* curr_ = nullptr;
    char_type* last_ = nullptr;
};

using iobuf = basic_iobuf<char>;
using wiobuf = basic_iobuf<wchar_t>;
using u8iobuf = basic_iobuf<uint8_t>;

template<typename CharT>
basic_iobuf<CharT>& print_quoted_text(basic_iobuf<CharT>& out, std::basic_string_view<CharT> text) {
    const CharT *p1 = text.data(), *pend = text.data() + text.size();
    out.put('\"');
    for (const CharT* p2 = text.data(); p2 != pend; ++p2) {
        std::string_view esc;
        switch (*p2) {
            case '\"': esc = "\\\""; break;
            case '\\': esc = "\\\\"; break;
            case '\a': esc = "\\a"; break;
            case '\b': esc = "\\b"; break;
            case '\f': esc = "\\f"; break;
            case '\n': esc = "\\n"; break;
            case '\r': esc = "\\r"; break;
            case '\t': esc = "\\t"; break;
            case '\v': esc = "\\v"; break;
            default: continue;
        }
        out.write(as_span(p1, p2 - p1));
        for (char ch : esc) { out.put(ch); }
        p1 = p2 + 1;
    }
    out.write(as_span(p1, pend - p1));
    out.put('\"');
    return out;
}

namespace stdbuf {
extern UXS_EXPORT iobuf& out;
extern UXS_EXPORT iobuf& in;
extern UXS_EXPORT iobuf& log;
extern UXS_EXPORT iobuf& err;
}  // namespace stdbuf

}  // namespace uxs
