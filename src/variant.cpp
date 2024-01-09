#include "uxs/variant.h"

using namespace uxs;

UXS_IMPLEMENT_VARIANT_TYPE(std::string, nullptr, nullptr);
UXS_IMPLEMENT_VARIANT_TYPE_WITH_STRING_CONVERTER(bool);
UXS_IMPLEMENT_VARIANT_TYPE(int32_t, convert_from, convert_to);
UXS_IMPLEMENT_VARIANT_TYPE(uint32_t, convert_from, convert_to);
UXS_IMPLEMENT_VARIANT_TYPE(int64_t, convert_from, convert_to);
UXS_IMPLEMENT_VARIANT_TYPE(uint64_t, convert_from, convert_to);
UXS_IMPLEMENT_VARIANT_TYPE(float, convert_from, convert_to);
UXS_IMPLEMENT_VARIANT_TYPE(double, convert_from, convert_to);

variant_error::variant_error(const char* message) : std::runtime_error(message) {}
variant_error::variant_error(const std::string& message) : std::runtime_error(message) {}
const char* variant_error::what() const noexcept { return std::runtime_error::what(); }

//---------------------------------------------------------------------------------
// Variant type implementation

/*static*/ variant::vtable_t* variant::vtables_[max_type_id] = {};

variant::variant(variant_id type, const variant& v) : vtable_(get_vtable(type)) {
    if (!vtable_) { return; }
    if (vtable_ == v.vtable_) {
        vtable_->construct_copy(&data_, &v.data_);
    } else {
        void* tgt = vtable_->construct_default(&data_);
        if (!v.vtable_) { return; }
        assert(vtable_ && v.vtable_);
        try {
            if (vtable_->type > v.vtable_->type && vtable_->convert_from) {
                vtable_->convert_from(v.vtable_->type, tgt, v.vtable_->get_value_const_ptr(&v.data_));
            } else if (v.vtable_->convert_to) {
                assert(v.vtable_->type > vtable_->type);
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
            storage_t tmp;
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
    vtable_t* tgt_vtable = get_vtable(type);
    if (vtable_ == tgt_vtable) { return true; }
    if (!tgt_vtable) {
        if (vtable_) { vtable_->destroy(&data_); }
    } else {
        if (!vtable_) {
            tgt_vtable->construct_default(&data_);
        } else {
            storage_t tmp;
            assert(vtable_ && tgt_vtable);
            void* tgt = tgt_vtable->construct_default(&tmp);
            try {
                if (vtable_->type > tgt_vtable->type && vtable_->convert_to) {
                    if (!vtable_->convert_to(tgt_vtable->type, tgt, vtable_->get_value_const_ptr(&data_))) {
                        tgt_vtable->destroy(&tmp);
                        return false;
                    }
                } else if (tgt_vtable->convert_from) {
                    assert(tgt_vtable->type > vtable_->type);
                    if (!tgt_vtable->convert_from(vtable_->type, tgt, vtable_->get_value_const_ptr(&data_))) {
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
    if (vtable_ == v.vtable_) {
        return !vtable_ || vtable_->is_equal(&data_, &v.data_);
    } else if (!vtable_ || !v.vtable_) {
        return false;
    }
    storage_t tmp;
    bool result = false;
    assert(vtable_ && v.vtable_);
    if (vtable_->type > v.vtable_->type && vtable_->convert_from) {
        void* tgt = vtable_->construct_default(&tmp);
        try {
            result = vtable_->convert_from(v.vtable_->type, tgt, v.vtable_->get_value_const_ptr(&v.data_)) &&
                     vtable_->is_equal(&tmp, &data_);
        } catch (...) {
            vtable_->destroy(&tmp);
            throw;
        }
        vtable_->destroy(&tmp);
    } else if (v.vtable_->convert_from) {
        assert(v.vtable_->type > vtable_->type);
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

#ifdef USE_QT
void variant::serialize(QDataStream& os) const {
    if (vtable_) {
        os << static_cast<unsigned>(vtable_->type);
        vtable_->serialize_qt(os, &data_);
    } else {
        os << static_cast<unsigned>(variant_id::invalid);
    }
}

void variant::deserialize(QDataStream& is) {
    unsigned type = 0;
    is >> type;
    auto* tgt_vtable = variant::get_vtable(static_cast<variant_id>(type));
    if (vtable_ != tgt_vtable) {
        reset();
        if (tgt_vtable) { tgt_vtable->construct_default(&data_); }
        vtable_ = tgt_vtable;
    }
    if (vtable_) { vtable_->deserialize_qt(is, &data_); }
}
#endif  // USE_QT

//---------------------------------------------------------------------------------
// Basic type convertors

bool variant_type_impl<int32_t>::convert_from(variant_id type, void* to, const void* from) {
    auto& result = *static_cast<int32_t*>(to);
    switch (type) {
        case variant_id::string: {
            return uxs::stoval(*static_cast<const std::string*>(from), result) != 0;
        } break;
        case variant_id::boolean: {
            result = *static_cast<const bool*>(from) ? 1 : 0;
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<int32_t>::convert_to(variant_id type, void* to, const void* from) {
    const auto& v = *static_cast<const int32_t*>(from);
    switch (type) {
        case variant_id::string: {
            *static_cast<std::string*>(to) = uxs::to_string(v);
        } break;
        case variant_id::boolean: {
            *static_cast<bool*>(to) = v != 0;
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<uint32_t>::convert_from(variant_id type, void* to, const void* from) {
    auto& result = *static_cast<uint32_t*>(to);
    switch (type) {
        case variant_id::string: {
            return uxs::stoval(*static_cast<const std::string*>(from), result) != 0;
        } break;
        case variant_id::boolean: {
            result = *static_cast<const bool*>(from) ? 1 : 0;
        } break;
        case variant_id::integer: {
            const auto& v = *static_cast<const int32_t*>(from);
            if (v < 0) { return false; }
            result = static_cast<uint32_t>(v);
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<uint32_t>::convert_to(variant_id type, void* to, const void* from) {
    const auto& v = *static_cast<const uint32_t*>(from);
    switch (type) {
        case variant_id::string: {
            *static_cast<std::string*>(to) = uxs::to_string(v);
        } break;
        case variant_id::boolean: {
            *static_cast<bool*>(to) = v != 0;
        } break;
        case variant_id::integer: {
            if (v > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) { return false; }
            *static_cast<int32_t*>(to) = static_cast<int32_t>(v);
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<int64_t>::convert_from(variant_id type, void* to, const void* from) {
    auto& result = *static_cast<int64_t*>(to);
    switch (type) {
        case variant_id::string: {
            return uxs::stoval(*static_cast<const std::string*>(from), result) != 0;
        } break;
        case variant_id::boolean: {
            result = *static_cast<const bool*>(from) ? 1 : 0;
        } break;
        case variant_id::integer: {
            result = *static_cast<const int32_t*>(from);
        } break;
        case variant_id::unsigned_integer: {
            result = static_cast<int64_t>(*static_cast<const uint32_t*>(from));
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<int64_t>::convert_to(variant_id type, void* to, const void* from) {
    const auto& v = *static_cast<const int64_t*>(from);
    switch (type) {
        case variant_id::string: {
            *static_cast<std::string*>(to) = uxs::to_string(v);
        } break;
        case variant_id::boolean: {
            *static_cast<bool*>(to) = v != 0;
        } break;
        case variant_id::integer: {
            if (v < std::numeric_limits<int32_t>::min() || v > std::numeric_limits<int32_t>::max()) { return false; }
            *static_cast<int32_t*>(to) = static_cast<int32_t>(v);
        } break;
        case variant_id::unsigned_integer: {
            if (v < 0 || v > static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) { return false; }
            *static_cast<uint32_t*>(to) = static_cast<uint32_t>(v);
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<uint64_t>::convert_from(variant_id type, void* to, const void* from) {
    auto& result = *static_cast<uint64_t*>(to);
    switch (type) {
        case variant_id::string: {
            return uxs::stoval(*static_cast<const std::string*>(from), result) != 0;
        } break;
        case variant_id::boolean: {
            result = *static_cast<const bool*>(from) ? 1 : 0;
        } break;
        case variant_id::integer: {
            const auto& v = *static_cast<const int32_t*>(from);
            if (v < 0) { return false; }
            result = static_cast<uint64_t>(v);
        } break;
        case variant_id::unsigned_integer: {
            result = *static_cast<const uint32_t*>(from);
        } break;
        case variant_id::long_integer: {
            const auto& v = *static_cast<const int64_t*>(from);
            if (v < 0) { return false; }
            result = static_cast<uint64_t>(v);
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<uint64_t>::convert_to(variant_id type, void* to, const void* from) {
    const auto& v = *static_cast<const uint64_t*>(from);
    switch (type) {
        case variant_id::string: {
            *static_cast<std::string*>(to) = uxs::to_string(v);
        } break;
        case variant_id::boolean: {
            *static_cast<bool*>(to) = v != 0;
        } break;
        case variant_id::integer: {
            if (v > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) { return false; }
            *static_cast<int32_t*>(to) = static_cast<int32_t>(v);
        } break;
        case variant_id::unsigned_integer: {
            if (v > std::numeric_limits<uint32_t>::max()) { return false; }
            *static_cast<uint32_t*>(to) = static_cast<uint32_t>(v);
        } break;
        case variant_id::long_integer: {
            if (v > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) { return false; }
            *static_cast<int64_t*>(to) = static_cast<int64_t>(v);
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<float>::convert_from(variant_id type, void* to, const void* from) {
    auto& result = *static_cast<float*>(to);
    switch (type) {
        case variant_id::string: {
            return uxs::stoval(*static_cast<const std::string*>(from), result) != 0;
        } break;
        case variant_id::boolean: {
            result = *static_cast<const bool*>(from) ? 1.f : 0.f;
        } break;
        case variant_id::integer: {
            result = static_cast<float>(*static_cast<const int32_t*>(from));
        } break;
        case variant_id::unsigned_integer: {
            result = static_cast<float>(*static_cast<const uint32_t*>(from));
        } break;
        case variant_id::long_integer: {
            result = static_cast<float>(*static_cast<const int64_t*>(from));
        } break;
        case variant_id::unsigned_long_integer: {
            result = static_cast<float>(*static_cast<const uint64_t*>(from));
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<float>::convert_to(variant_id type, void* to, const void* from) {
    const auto& v = *static_cast<const float*>(from);
    switch (type) {
        case variant_id::string: {
            *static_cast<std::string*>(to) = uxs::to_string(v, fmt_flags::json_compat);
        } break;
        case variant_id::boolean: {
            *static_cast<bool*>(to) = v != 0;
        } break;
        case variant_id::integer: {
            // Note that float(2^31 - 1) will be rounded up to 2^31, so maximum is excluded
            if (v < static_cast<float>(std::numeric_limits<int32_t>::min()) ||
                v >= static_cast<float>(std::numeric_limits<int32_t>::max())) {
                return false;
            }
            *static_cast<int32_t*>(to) = static_cast<int32_t>(v);
        } break;
        case variant_id::unsigned_integer: {
            // Note that float(2^32 - 1) will be rounded up to 2^32, so maximum is excluded
            if (v < 0 || v >= static_cast<float>(std::numeric_limits<uint32_t>::max())) { return false; }
            *static_cast<uint32_t*>(to) = static_cast<uint32_t>(v);
        } break;
        case variant_id::long_integer: {
            // Note that float(2^63 - 1) will be rounded up to 2^63, so maximum is excluded
            if (v < static_cast<float>(std::numeric_limits<int64_t>::min()) ||
                v >= static_cast<float>(std::numeric_limits<int64_t>::max())) {
                return false;
            }
            *static_cast<int64_t*>(to) = static_cast<int64_t>(v);
        } break;
        case variant_id::unsigned_long_integer: {
            // Note that float(2^64 - 1) will be rounded up to 2^64, so maximum is excluded
            if (v < 0 || v >= static_cast<float>(std::numeric_limits<uint64_t>::max())) { return false; }
            *static_cast<uint64_t*>(to) = static_cast<uint64_t>(v);
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<double>::convert_from(variant_id type, void* to, const void* from) {
    auto& result = *static_cast<double*>(to);
    switch (type) {
        case variant_id::string: {
            return uxs::stoval(*static_cast<const std::string*>(from), result) != 0;
        } break;
        case variant_id::boolean: {
            result = *static_cast<const bool*>(from) ? 1. : 0.;
        } break;
        case variant_id::integer: {
            result = *static_cast<const int32_t*>(from);
        } break;
        case variant_id::unsigned_integer: {
            result = *static_cast<const uint32_t*>(from);
        } break;
        case variant_id::long_integer: {
            result = static_cast<double>(*static_cast<const int64_t*>(from));
        } break;
        case variant_id::unsigned_long_integer: {
            result = static_cast<double>(*static_cast<const uint64_t*>(from));
        } break;
        case variant_id::single_precision: {
            result = *static_cast<const float*>(from);
        } break;
        default: return false;
    }
    return true;
}

bool variant_type_impl<double>::convert_to(variant_id type, void* to, const void* from) {
    const auto& v = *static_cast<const double*>(from);
    switch (type) {
        case variant_id::string: {
            *static_cast<std::string*>(to) = uxs::to_string(v, fmt_flags::json_compat);
        } break;
        case variant_id::boolean: {
            *static_cast<bool*>(to) = v != 0;
        } break;
        case variant_id::integer: {
            if (v < std::numeric_limits<int32_t>::min() || v > std::numeric_limits<int32_t>::max()) { return false; }
            *static_cast<int32_t*>(to) = static_cast<int32_t>(v);
        } break;
        case variant_id::unsigned_integer: {
            if (v < 0 || v > std::numeric_limits<uint32_t>::max()) { return false; }
            *static_cast<uint32_t*>(to) = static_cast<uint32_t>(v);
        } break;
        case variant_id::long_integer: {
            // Note that double(2^63 - 1) will be rounded up to 2^63, so maximum is excluded
            if (v < static_cast<double>(std::numeric_limits<int64_t>::min()) ||
                v >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
                return false;
            }
            *static_cast<int64_t*>(to) = static_cast<int64_t>(v);
        } break;
        case variant_id::unsigned_long_integer: {
            // Note that double(2^64 - 1) will be rounded up to 2^64, so maximum is excluded
            if (v < 0 || v >= static_cast<double>(std::numeric_limits<uint64_t>::max())) { return false; }
            *static_cast<uint64_t*>(to) = static_cast<uint64_t>(v);
        } break;
        case variant_id::single_precision: {
            *static_cast<float*>(to) = static_cast<float>(v);
        } break;
        default: return false;
    }
    return true;
}
