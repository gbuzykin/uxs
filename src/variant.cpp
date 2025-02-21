#include "uxs/variant.h"

using namespace uxs;

UXS_IMPLEMENT_VARIANT_TYPE(std::string, nullptr, nullptr);
UXS_IMPLEMENT_VARIANT_TYPE_WITH_STRING_CONVERTER(bool);
UXS_IMPLEMENT_VARIANT_TYPE(std::int32_t, convert_from, convert_to);
UXS_IMPLEMENT_VARIANT_TYPE(std::uint32_t, convert_from, convert_to);
UXS_IMPLEMENT_VARIANT_TYPE(std::int64_t, convert_from, convert_to);
UXS_IMPLEMENT_VARIANT_TYPE(std::uint64_t, convert_from, convert_to);
UXS_IMPLEMENT_VARIANT_TYPE(double, convert_from, convert_to);

variant_error::variant_error(const char* message) : std::runtime_error(message) {}
variant_error::variant_error(const std::string& message) : std::runtime_error(message) {}
const char* variant_error::what() const noexcept { return std::runtime_error::what(); }

//---------------------------------------------------------------------------------
// Variant type implementation

/*static*/ detail::variant_vtable_t* variant::vtables_[max_type_id] = {};

variant::variant(variant_id type, const variant& v) : vtable_(get_vtable(type)) {
    if (!vtable_) { return; }
    if (vtable_ == v.vtable_) {
        vtable_->construct_copy(&data_, &v.data_);
    } else {
        void* tgt = vtable_->construct_default(&data_);
        if (!v.vtable_) { return; }
        assert(vtable_ && v.vtable_);
        try {
            if (vtable_->type > v.vtable_->type) {
                if (vtable_->convert_from) {
                    vtable_->convert_from(v.vtable_->type, tgt, v.vtable_->get_value_const_ptr(&v.data_));
                }
            } else if (v.vtable_->convert_to) {
                v.vtable_->convert_to(vtable_->type, tgt, v.vtable_->get_value_const_ptr(&v.data_));
            }
        } catch (...) {
            vtable_->destroy(&data_);
            throw;
        }
    }
}

variant& variant::operator=(const variant& v) {
    if (&v == this) { return *this; }
    if (vtable_ == v.vtable_) {
        if (vtable_) { vtable_->assign_copy(&data_, &v.data_); }
    } else {
        if (!vtable_) {
            assert(v.vtable_);
            v.vtable_->construct_copy(&data_, &v.data_);
        } else if (!v.vtable_) {
            assert(vtable_);
            vtable_->destroy(&data_);
        } else {
            detail::variant_storage_t tmp;
            assert(vtable_ && v.vtable_);
            v.vtable_->construct_copy(&tmp, &v.data_);
            vtable_->destroy(&data_);
            v.vtable_->construct_move(&data_, &tmp);
            v.vtable_->destroy(&tmp);
        }
        vtable_ = v.vtable_;
    }
    return *this;
}

variant& variant::operator=(variant&& v) noexcept {
    if (&v == this) { return *this; }
    if (vtable_ == v.vtable_) {
        if (vtable_) { vtable_->assign_move(&data_, &v.data_); }
    } else {
        if (vtable_) { vtable_->destroy(&data_); }
        if (v.vtable_) { v.vtable_->construct_move(&data_, &v.data_); }
        vtable_ = v.vtable_;
    }
    return *this;
}

bool variant::convert(variant_id type) {
    auto* tgt_vtable = get_vtable(type);
    if (vtable_ == tgt_vtable) { return true; }
    if (!tgt_vtable) {
        if (vtable_) { vtable_->destroy(&data_); }
    } else {
        if (!vtable_) {
            tgt_vtable->construct_default(&data_);
        } else {
            detail::variant_storage_t tmp;
            assert(vtable_ && tgt_vtable);
            void* tgt = tgt_vtable->construct_default(&tmp);
            try {
                if (vtable_->type > tgt_vtable->type) {
                    if (!vtable_->convert_to ||
                        !vtable_->convert_to(tgt_vtable->type, tgt, vtable_->get_value_const_ptr(&data_))) {
                        tgt_vtable->destroy(&tmp);
                        return false;
                    }
                } else {
                    if (!tgt_vtable->convert_from ||
                        !tgt_vtable->convert_from(vtable_->type, tgt, vtable_->get_value_const_ptr(&data_))) {
                        tgt_vtable->destroy(&tmp);
                        return false;
                    }
                }
            } catch (...) {
                tgt_vtable->destroy(&tmp);
                throw;
            }
            vtable_->destroy(&data_);
            tgt_vtable->construct_move(&data_, &tmp);
            tgt_vtable->destroy(&tmp);
        }
    }
    vtable_ = tgt_vtable;
    return true;
}

bool variant::is_equal_to(const variant& v) const {
    if (vtable_ == v.vtable_) { return !vtable_ || vtable_->is_equal(&data_, &v.data_); }
    if (!vtable_ || !v.vtable_) { return false; }
    detail::variant_storage_t tmp;
    bool result = false;
    assert(vtable_ && v.vtable_);
    if (vtable_->type > v.vtable_->type) {
        if (vtable_->convert_from) {
            void* tgt = vtable_->construct_default(&tmp);
            try {
                result = vtable_->convert_from(v.vtable_->type, tgt, v.vtable_->get_value_const_ptr(&v.data_)) &&
                         vtable_->is_equal(&tmp, &data_);
            } catch (...) {
                vtable_->destroy(&tmp);
                throw;
            }
            vtable_->destroy(&tmp);
        }
    } else if (v.vtable_->convert_from) {
        void* tgt = v.vtable_->construct_default(&tmp);
        try {
            result = v.vtable_->convert_from(vtable_->type, tgt, vtable_->get_value_const_ptr(&data_)) &&
                     v.vtable_->is_equal(&tmp, &v.data_);
        } catch (...) {
            v.vtable_->destroy(&tmp);
            throw;
        }
        v.vtable_->destroy(&tmp);
    }
    return result;
}

namespace uxs {
u8ibuf& operator>>(u8ibuf& is, variant& v) {
    variant_id type = variant_id::invalid;
    if (!(is >> type)) { return is; }
    auto* tgt_vtable = variant::get_vtable(type);
    if (v.vtable_ != tgt_vtable) {
        v.reset();
        if (tgt_vtable) { tgt_vtable->construct_default(&v.data_); }
        v.vtable_ = tgt_vtable;
    }
    if (v.vtable_) { v.vtable_->deserialize(is, &v.data_); }
    return is;
}

u8iobuf& operator<<(u8iobuf& os, const variant& v) {
    if (!v.vtable_) { return os << variant_id::invalid; }
    os << v.vtable_->type;
    v.vtable_->serialize(os, &v.data_);
    return os;
}
}  // namespace uxs

//---------------------------------------------------------------------------------
// Basic type convertors

bool variant_type_impl<std::int32_t>::convert_from(variant_id type, void* to, const void* from) {
    auto& result = *static_cast<std::int32_t*>(to);
    switch (type) {
        case variant_id::string: {
            return from_string(*static_cast<const std::string*>(from), result) != 0;
        } break;
        case variant_id::boolean: {
            result = *static_cast<const bool*>(from) ? 1 : 0;
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<std::int32_t>::convert_to(variant_id type, void* to, const void* from) {
    const auto& v = *static_cast<const std::int32_t*>(from);
    switch (type) {
        case variant_id::string: {
            *static_cast<std::string*>(to) = to_string(v);
        } break;
        case variant_id::boolean: {
            *static_cast<bool*>(to) = v != 0;
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<std::uint32_t>::convert_from(variant_id type, void* to, const void* from) {
    auto& result = *static_cast<std::uint32_t*>(to);
    switch (type) {
        case variant_id::string: {
            return from_string(*static_cast<const std::string*>(from), result) != 0;
        } break;
        case variant_id::boolean: {
            result = *static_cast<const bool*>(from) ? 1 : 0;
        } break;
        case variant_id::integer: {
            const auto& v = *static_cast<const std::int32_t*>(from);
            if (v < 0) { return false; }
            result = static_cast<std::uint32_t>(v);
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<std::uint32_t>::convert_to(variant_id type, void* to, const void* from) {
    const auto& v = *static_cast<const std::uint32_t*>(from);
    switch (type) {
        case variant_id::string: {
            *static_cast<std::string*>(to) = to_string(v);
        } break;
        case variant_id::boolean: {
            *static_cast<bool*>(to) = v != 0;
        } break;
        case variant_id::integer: {
            if (v > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) { return false; }
            *static_cast<std::int32_t*>(to) = static_cast<std::int32_t>(v);
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<std::int64_t>::convert_from(variant_id type, void* to, const void* from) {
    auto& result = *static_cast<std::int64_t*>(to);
    switch (type) {
        case variant_id::string: {
            return from_string(*static_cast<const std::string*>(from), result) != 0;
        } break;
        case variant_id::boolean: {
            result = *static_cast<const bool*>(from) ? 1 : 0;
        } break;
        case variant_id::integer: {
            result = *static_cast<const std::int32_t*>(from);
        } break;
        case variant_id::unsigned_integer: {
            result = static_cast<std::int64_t>(*static_cast<const std::uint32_t*>(from));
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<std::int64_t>::convert_to(variant_id type, void* to, const void* from) {
    const auto& v = *static_cast<const std::int64_t*>(from);
    switch (type) {
        case variant_id::string: {
            *static_cast<std::string*>(to) = to_string(v);
        } break;
        case variant_id::boolean: {
            *static_cast<bool*>(to) = v != 0;
        } break;
        case variant_id::integer: {
            if (v < std::numeric_limits<std::int32_t>::min() || v > std::numeric_limits<std::int32_t>::max()) {
                return false;
            }
            *static_cast<std::int32_t*>(to) = static_cast<std::int32_t>(v);
        } break;
        case variant_id::unsigned_integer: {
            if (v < 0 || v > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max())) { return false; }
            *static_cast<std::uint32_t*>(to) = static_cast<std::uint32_t>(v);
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<std::uint64_t>::convert_from(variant_id type, void* to, const void* from) {
    auto& result = *static_cast<std::uint64_t*>(to);
    switch (type) {
        case variant_id::string: {
            return from_string(*static_cast<const std::string*>(from), result) != 0;
        } break;
        case variant_id::boolean: {
            result = *static_cast<const bool*>(from) ? 1 : 0;
        } break;
        case variant_id::integer: {
            const auto& v = *static_cast<const std::int32_t*>(from);
            if (v < 0) { return false; }
            result = static_cast<std::uint64_t>(v);
        } break;
        case variant_id::unsigned_integer: {
            result = *static_cast<const std::uint32_t*>(from);
        } break;
        case variant_id::long_integer: {
            const auto& v = *static_cast<const std::int64_t*>(from);
            if (v < 0) { return false; }
            result = static_cast<std::uint64_t>(v);
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<std::uint64_t>::convert_to(variant_id type, void* to, const void* from) {
    const auto& v = *static_cast<const std::uint64_t*>(from);
    switch (type) {
        case variant_id::string: {
            *static_cast<std::string*>(to) = to_string(v);
        } break;
        case variant_id::boolean: {
            *static_cast<bool*>(to) = v != 0;
        } break;
        case variant_id::integer: {
            if (v > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) { return false; }
            *static_cast<std::int32_t*>(to) = static_cast<std::int32_t>(v);
        } break;
        case variant_id::unsigned_integer: {
            if (v > std::numeric_limits<std::uint32_t>::max()) { return false; }
            *static_cast<std::uint32_t*>(to) = static_cast<std::uint32_t>(v);
        } break;
        case variant_id::long_integer: {
            if (v > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) { return false; }
            *static_cast<std::int64_t*>(to) = static_cast<std::int64_t>(v);
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<double>::convert_from(variant_id type, void* to, const void* from) {
    auto& result = *static_cast<double*>(to);
    switch (type) {
        case variant_id::string: {
            return from_string(*static_cast<const std::string*>(from), result) != 0;
        } break;
        case variant_id::boolean: {
            result = *static_cast<const bool*>(from) ? 1. : 0.;
        } break;
        case variant_id::integer: {
            result = *static_cast<const std::int32_t*>(from);
        } break;
        case variant_id::unsigned_integer: {
            result = *static_cast<const std::uint32_t*>(from);
        } break;
        case variant_id::long_integer: {
            result = static_cast<double>(*static_cast<const std::int64_t*>(from));
        } break;
        case variant_id::unsigned_long_integer: {
            result = static_cast<double>(*static_cast<const std::uint64_t*>(from));
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<double>::convert_to(variant_id type, void* to, const void* from) {
    const auto& v = *static_cast<const double*>(from);
    switch (type) {
        case variant_id::string: {
            *static_cast<std::string*>(to) = to_string(v, fmt_opts{fmt_flags::json_compat});
        } break;
        case variant_id::boolean: {
            *static_cast<bool*>(to) = v != 0;
        } break;
        case variant_id::integer: {
            if (v < std::numeric_limits<std::int32_t>::min() || v > std::numeric_limits<std::int32_t>::max()) {
                return false;
            }
            *static_cast<std::int32_t*>(to) = static_cast<std::int32_t>(v);
        } break;
        case variant_id::unsigned_integer: {
            if (v < 0 || v > std::numeric_limits<std::uint32_t>::max()) { return false; }
            *static_cast<std::uint32_t*>(to) = static_cast<std::uint32_t>(v);
        } break;
        case variant_id::long_integer: {
            // Note that double(2^63 - 1) will be rounded up to 2^63, so maximum is excluded
            if (v < static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
                v >= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
                return false;
            }
            *static_cast<std::int64_t*>(to) = static_cast<std::int64_t>(v);
        } break;
        case variant_id::unsigned_long_integer: {
            // Note that double(2^64 - 1) will be rounded up to 2^64, so maximum is excluded
            if (v < 0 || v >= static_cast<double>(std::numeric_limits<std::uint64_t>::max())) { return false; }
            *static_cast<std::uint64_t*>(to) = static_cast<std::uint64_t>(v);
        } break;
        default: return false;
    }
    return true;
}
