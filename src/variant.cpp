#include "uxs/variant.h"

#include <cstring>

using namespace uxs;

static variant_type_impl<bool> g_bool_variant_type;
static variant_type_impl<int> g_int_variant_type;
static variant_type_impl<unsigned> g_uint_variant_type;
static variant_type_impl<double> g_double_variant_type;
static variant_type_impl<std::string> g_string_variant_type;

//---------------------------------------------------------------------------------
// Variant type implementation

/*static*/ variant::vtable_t* variant::get_vtable_raw(id_t type) {
    static struct implementations_t {
        std::array<vtable_t, kMaxTypeId> vtable;
        implementations_t() { std::memset(&vtable, 0, sizeof(vtable)); }
    } impl_list;
    assert(static_cast<unsigned>(type) < kMaxTypeId);
    return &impl_list.vtable.data()[static_cast<unsigned>(type)];
}

variant::variant(id_t type, const variant& v) : vtable_(get_vtable(type)) {
    if (!vtable_) { return; }
    if (vtable_ == v.vtable_) {
        vtable_->construct_copy(&data_, &v.data_);
    } else {
        auto* tgt = vtable_->construct_default(&data_);
        if (!v.vtable_) { return; }
        if (auto cvt_func = vtable_->get_cvt(v.vtable_->type)) {
            try {
                cvt_func(tgt, v.vtable_->get_value_ptr(&v.data_));
            } catch (...) {
                tidy();
                throw;
            }
        }
    }
}

variant& variant::operator=(const variant& v) {
    if (&v == this) { return *this; }
    if (vtable_ == v.vtable_) {
        if (vtable_) { vtable_->assign_copy(&data_, &v.data_); }
    } else {
        tidy();
        if (v.vtable_) { v.vtable_->construct_copy(&data_, &v.data_); }
        vtable_ = v.vtable_;
    }
    return *this;
}

variant& variant::operator=(variant&& v) NOEXCEPT {
    if (&v == this) { return *this; }
    if (vtable_ == v.vtable_) {
        if (vtable_) { vtable_->assign_move(&data_, &v.data_); }
    } else {
        tidy();
        if (v.vtable_) { v.vtable_->construct_move(&data_, &v.data_); }
        vtable_ = v.vtable_;
    }
    return *this;
}

bool variant::can_convert(id_t type) const {
    auto* tgt_vtable = get_vtable(type);
    return vtable_ && tgt_vtable && (vtable_ == tgt_vtable || tgt_vtable->get_cvt(vtable_->type));
}

void variant::convert(id_t type) {
    auto* tgt_vtable = get_vtable(type);
    if (vtable_ == tgt_vtable) { return; }
    if (!tgt_vtable) {
        tidy();
    } else {
        if (!vtable_) {
            tgt_vtable->construct_default(&data_);
        } else if (auto cvt_func = tgt_vtable->get_cvt(vtable_->type)) {
            storage_t tmp;
            auto* tgt = tgt_vtable->construct_default(&tmp);
            try {
                cvt_func(tgt, vtable_->get_value_ptr(&data_));
            } catch (...) {
                tgt_vtable->destroy(&tmp);
                throw;
            }
            tidy();
            tgt_vtable->construct_move(&data_, &tmp);
            tgt_vtable->destroy(&tmp);
        } else {
            tidy();
            tgt_vtable->construct_default(&data_);
        }
        vtable_ = tgt_vtable;
    }
}

bool variant::is_equal_to(const variant& v) const {
    if (vtable_ == v.vtable_) {
        return !vtable_ || vtable_->equal(&data_, &v.data_);
    } else if (!vtable_ || !v.vtable_) {
        return false;
    } else if (vtable_->type > v.vtable_->type) {
        return v.is_equal_to(*this);
    } else if (auto cvt_func = v.vtable_->get_cvt(vtable_->type)) {
        storage_t tmp;
        auto* tgt = v.vtable_->construct_default(&tmp);
        try {
            cvt_func(tgt, vtable_->get_value_ptr(&data_));
        } catch (...) {
            v.vtable_->destroy(&tmp);
            throw;
        }
        bool result = v.vtable_->equal(&tmp, &v.data_);
        v.vtable_->destroy(&tmp);
        return result;
    }
    return false;
}

#ifdef USE_QT
void variant::serialize(QDataStream& os) const {
    if (vtable_) {
        os << static_cast<unsigned>(vtable_->type);
        vtable_->serialize(os, &data_);
    } else {
        os << static_cast<unsigned>(id_t::kInvalid);
    }
}

void variant::deserialize(QDataStream& is) {
    unsigned type = 0;
    is >> type;
    auto* tgt_vtable = variant::get_vtable(static_cast<variant_id>(type));
    if (vtable_ != tgt_vtable) {
        tidy();
        if (tgt_vtable) { tgt_vtable->construct_default(&data_); }
        vtable_ = tgt_vtable;
    }
    if (vtable_) { vtable_->deserialize(is, &data_); }
}
#endif  // USE_QT
