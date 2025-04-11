#include "uxs/impl/db/xml_impl.h"
#include "uxs/string_alg.h"

namespace lex_detail {
#include "xml_lex_defs.h"
}
namespace lex_detail {
#include "xml_lex_analyzer.inl"
}

namespace uxs {
namespace db {
namespace xml {

parser::parser(ibuf& in) : in_(in), name_cache_(16), token_{token_t::none, {}} {
    stack_.push_back(lex_detail::sc_initial);
}

std::pair<token_t, std::string_view> parser::next_impl() {
    if (is_end_element_pending_) {
        is_end_element_pending_ = false;
        return {token_t::end_element, name_cache_.front()};
    }
    if (!in_) { return {token_t::eof, {}}; }
    while (in_.avail() || in_.peek() != ibuf::traits_type::eof()) {
        const char* first = in_.first_avail();
        const char* last = in_.last_avail();
        std::string_view lval;
        if (*first == '<') {  // found '<'
            auto name_cache_it = name_cache_.begin();
            auto name_cache_prev_it = name_cache_it;

            attrs_.clear();

            const auto read_attribute = [this, &name_cache_it, &name_cache_prev_it](std::string_view lval) {
                if (name_cache_it != name_cache_.end()) {
                    name_cache_it->assign(lval.data(), lval.size());
                } else {
                    name_cache_it = name_cache_.emplace_after(name_cache_prev_it, lval);
                }
                if (lex(lval) != lex_token_t::eq) { throw database_error(to_string(ln_) + ": expected `=`"); }
                if (lex(lval) != lex_token_t::string) {
                    throw database_error(to_string(ln_) + ": expected valid attribute value");
                }
                name_cache_prev_it = name_cache_it;
                attrs_.emplace(*name_cache_it++, lval);
            };

            switch (lex(lval)) {
                case lex_token_t::start_element_open: {  // <name n1=v1 n2=v2...> or <name n1=v1 n2=v2.../>
                    name_cache_it->assign(lval.data(), lval.size());
                    ++name_cache_it;
                    while (true) {
                        auto tt = lex(lval);
                        if (tt == lex_token_t::name) {
                            read_attribute(lval);
                        } else if (tt == lex_token_t::close) {
                            return {token_t::start_element, name_cache_.front()};
                        } else if (tt == lex_token_t::end_element_close) {
                            is_end_element_pending_ = true;
                            return {token_t::start_element, name_cache_.front()};
                        } else {
                            throw database_error(to_string(ln_) + ": expected name, `>` or `/>`");
                        }
                    }
                } break;
                case lex_token_t::end_element_open: {  // </name>
                    if (lex(lval) != lex_token_t::close) { throw database_error(to_string(ln_) + ": expected `>`"); }
                    return {token_t::end_element, lval};
                } break;
                case lex_token_t::pi_open: {  // <?xml n1=v1 n2=v2...?>
                    if (compare_strings_nocase(lval, string_literal<char, 'x', 'm', 'l'>{}()) != 0) {
                        throw database_error(to_string(ln_) + ": invalid document declaration");
                    }
                    name_cache_it->assign(lval.data(), lval.size());
                    ++name_cache_it;
                    while (true) {
                        auto tt = lex(lval);
                        if (tt == lex_token_t::name) {
                            read_attribute(lval);
                        } else if (tt == lex_token_t::pi_close) {
                            return {token_t::preamble, name_cache_.front()};
                        } else {
                            throw database_error(to_string(ln_) + ": expected name or `?>`");
                        }
                    }
                } break;
                case lex_token_t::comment: {  // comment <!--....-->: skip till `-->`
                    int ch = in_.get();
                    do {
                        while (in_ && ch != '-') {
                            if (ch == '\0') { return {token_t::eof, {}}; }
                            if (ch == '\n') { ++ln_; }
                            ch = in_.get();
                        }
                        ch = in_.get();
                    } while (in_ && (ch != '-' || in_.peek() != '>'));
                    if (!in_) { break; }
                    in_.advance(1);
                } break;
                default: break;
            }
        } else if (*first == '&') {  // parse entity
            if (lex(lval) == lex_token_t::predef_entity) { return {token_t::plain_text, lval}; }
            return {token_t::entity, lval};
        } else if (*first != 0) {
            last = std::find_if(first, last, [this](char ch) {
                if (ch != '\n') {
                    return !!(uxs::detail::char_tbl_t{}.flags()[static_cast<std::uint8_t>(ch)] &
                              uxs::detail::char_bits::is_xml_special);
                }
                ++ln_;
                return false;
            });
            in_.advance(last - first);
            return {token_t::plain_text, to_string_view(first, last)};
        } else {
            return {token_t::eof, {}};
        }
    }
    return {token_t::eof, {}};
}

parser::lex_token_t parser::lex(std::string_view& lval) {
    while (true) {
        int pat = 0;
        unsigned llen = 0;
        const char* first = in_.first_avail();
        while (true) {
            bool stack_limitation = false;
            const char* last = in_.last_avail();
            if (stack_.avail() < static_cast<std::size_t>(last - first)) {
                last = first + stack_.avail();
                stack_limitation = true;
            }
            auto* sptr = stack_.curr();
            pat = lex_detail::lex(first, last, &sptr, &llen, stack_limitation || in_);
            stack_.advance(sptr - stack_.curr());
            if (pat >= lex_detail::predef_pat_default) { break; }
            if (stack_limitation) {
                // enlarge state stack and continue analysis
                stack_.reserve(llen);
                first = last;
                continue;
            }
            if (!in_) { return lex_token_t::eof; }  // end of sequence, first_ == last_
            if (in_.avail()) {
                // append read buffer to stash
                stash_.append(in_.first_avail(), in_.last_avail());
                in_.advance(in_.avail());
            }
            // read more characters from input
            in_.peek();
            first = in_.first_avail();
        }
        const char* lexeme = in_.first_avail();
        if (stash_.empty()) {  // the stash is empty
            in_.advance(llen);
        } else {
            if (llen >= stash_.size()) {
                // all characters in stash buffer are used
                // concatenate full lexeme in stash
                const std::size_t len_rest = llen - stash_.size();
                stash_.append(in_.first_avail(), len_rest);
                in_.advance(len_rest);
            } else {
                // at least one character in stash is yet unused
                // put unused chars back to `ibuf`
                for (std::size_t n = 0; n < stash_.size() - llen; ++n) { in_.unget(); }
            }
            lexeme = stash_.data();
            stash_.clear();  // it resets end pointer, but retains the contents
        }
        switch (pat) {
            // ------ specials
            case lex_detail::pat_amp: {
                if (stack_.back() == lex_detail::sc_initial) {
                    lval = string_literal<char, '&'>{}();
                    return lex_token_t::predef_entity;
                }
                str_ += '&';
            } break;
            case lex_detail::pat_lt: {
                if (stack_.back() == lex_detail::sc_initial) {
                    lval = string_literal<char, '<'>{}();
                    return lex_token_t::predef_entity;
                }
                str_ += '<';
            } break;
            case lex_detail::pat_gt: {
                if (stack_.back() == lex_detail::sc_initial) {
                    lval = string_literal<char, '>'>{}();
                    return lex_token_t::predef_entity;
                }
                str_ += '>';
            } break;
            case lex_detail::pat_apos: {
                if (stack_.back() == lex_detail::sc_initial) {
                    lval = string_literal<char, '\''>{}();
                    return lex_token_t::predef_entity;
                }
                str_ += '\'';
            } break;
            case lex_detail::pat_quot: {
                if (stack_.back() == lex_detail::sc_initial) {
                    lval = string_literal<char, '\"'>{}();
                    return lex_token_t::predef_entity;
                }
                str_ += '\"';
            } break;
            case lex_detail::pat_entity: {
                if (stack_.back() == lex_detail::sc_initial) {
                    lval = std::string_view(lexeme + 1, llen - 2);
                    return lex_token_t::entity;
                }
                throw database_error(to_string(ln_) + ": unknown entity name");
            } break;
            case lex_detail::pat_dcode: {
                unsigned unicode = 0;
                for (const char ch : std::string_view(lexeme + 2, llen - 3)) { unicode = 10 * unicode + dig_v(ch); }
                if (stack_.back() == lex_detail::sc_initial) {
                    lval = std::string_view(str_.data(), to_utf8(unicode, str_.data()));
                    return lex_token_t::entity;
                }
                to_utf8(unicode, std::back_inserter(str_));
            } break;
            case lex_detail::pat_hcode: {
                unsigned unicode = 0;
                for (const char ch : std::string_view(lexeme + 3, llen - 4)) { unicode = (unicode << 4) + dig_v(ch); }
                if (stack_.back() == lex_detail::sc_initial) {
                    lval = std::string_view(str_.data(), to_utf8(unicode, str_.data()));
                    return lex_token_t::entity;
                }
                to_utf8(unicode, std::back_inserter(str_));
            } break;

            // ------ strings
            case lex_detail::pat_string_nl: {
                ++ln_;
                str_.push_back('\n');
            } break;
            case lex_detail::pat_string_other: return lex_token_t::eof;
            case lex_detail::pat_string_seq_quot:
            case lex_detail::pat_string_seq_apos: {
                str_.append(lexeme, llen);
            } break;
            case lex_detail::pat_string_close_quot:
            case lex_detail::pat_string_close_apos: {
                if (str_.empty()) {
                    lval = std::string_view(lexeme, llen - 1);
                } else {
                    str_.append(lexeme, llen - 1);
                    lval = std::string_view(str_.data(), str_.size());
                    str_.clear();  // it resets end pointer, but retains the contents
                }
                stack_.pop_back();
                return lex_token_t::string;
            } break;

            case lex_detail::pat_name: lval = std::string_view(lexeme, llen); return lex_token_t::name;
            case lex_detail::pat_start_element_open: {
                lval = std::string_view(lexeme + 1, llen - 1);
                return lex_token_t::start_element_open;
            } break;
            case lex_detail::pat_end_element_open: {
                lval = std::string_view(lexeme + 2, llen - 2);
                return lex_token_t::end_element_open;
            } break;
            case lex_detail::pat_end_element_close: return lex_token_t::end_element_close;
            case lex_detail::pat_pi_open: lval = std::string_view(lexeme + 2, llen - 2); return lex_token_t::pi_open;
            case lex_detail::pat_pi_close: return lex_token_t::pi_close;
            case lex_detail::pat_comment: return lex_token_t::comment;

            case lex_detail::pat_whitespace: break;
            case lex_detail::predef_pat_default: {
                switch (lexeme[0]) {
                    case '\n': ++ln_; break;
                    case '\"': stack_.push_back(lex_detail::sc_string_quot); break;
                    case '\'': stack_.push_back(lex_detail::sc_string_apos); break;
                    default: return static_cast<lex_token_t>(static_cast<std::uint8_t>(*first));
                }
            } break;
            default: UXS_UNREACHABLE_CODE;
        }
    }
}

/*static*/ value_class parser::classify_value(const std::string_view& sval) {
    int state = lex_detail::sc_value;
    for (const std::uint8_t ch : sval) {
        state = lex_detail::Dtran[lex_detail::dtran_width * state + lex_detail::symb2meta[ch]];
    }
    switch (lex_detail::accept[state]) {
        case lex_detail::pat_null: return value_class::null_value;
        case lex_detail::pat_true: return value_class::true_value;
        case lex_detail::pat_false: return value_class::false_value;
        case lex_detail::pat_decimal: return value_class::integer_number;
        case lex_detail::pat_neg_decimal: return value_class::negative_integer_number;
        case lex_detail::pat_real: return value_class::floating_point_number;
        case lex_detail::pat_ws_with_nl: return value_class::ws_with_nl;
        case lex_detail::pat_other_value: return value_class::other;
        default: return value_class::empty;
    }
}

template UXS_EXPORT basic_value<char> parser::read(std::string_view, const std::allocator<char>&);
template UXS_EXPORT basic_value<wchar_t> parser::read(std::string_view, const std::allocator<wchar_t>&);
template UXS_EXPORT void detail::writer<char>::do_write(const basic_value<char>&, std::string_view, unsigned);
template UXS_EXPORT void detail::writer<char>::do_write(const basic_value<wchar_t>&, std::wstring_view, unsigned);
template UXS_EXPORT void detail::writer<wchar_t>::do_write(const basic_value<char>&, std::string_view, unsigned);
template UXS_EXPORT void detail::writer<wchar_t>::do_write(const basic_value<wchar_t>&, std::wstring_view, unsigned);
}  // namespace xml
}  // namespace db
}  // namespace uxs
