#include "util/variant.h"

using namespace util;

static variant_type_impl<bool> g_bool_variant_type;
static variant_type_impl<int> g_int_variant_type;
static variant_type_impl<unsigned> g_uint_variant_type;
static variant_type_impl<double> g_double_variant_type;
static variant_type_impl<std::string> g_string_variant_type;

//---------------------------------------------------------------------------------
// Variant type implementation

/*static*/ variant::vtable_t* variant::get_vtable(id_t type) {
    static struct implementations_t {
        std::array<vtable_t, kMaxTypeId> vtable;
        implementations_t() { std::memset(&vtable, 0, sizeof(vtable)); }
    } impl_list;
    assert(static_cast<unsigned>(type) < kMaxTypeId);
    return &impl_list.vtable.data()[static_cast<unsigned>(type)];
}

variant::variant(id_t type, const variant& v) : vtable_(get_vtable(type)) {
    if (vtable_->type == id_t::kInvalid) {
        return;
    } else if (vtable_ == v.vtable_) {
        vtable_->construct_copy(&data_, &v.data_);
        return;
    }
    auto* tgt = vtable_->construct_default(&data_);
    if (auto cvt_func = vtable_->get_cvt(v.vtable_->type)) {
        if (const auto* pval = v.vtable_->get_value_ptr(&v.data_)) { cvt_func(tgt, pval); }
    }
}

variant& variant::operator=(const variant& v) {
    if (&v == this) { return *this; }
    if (vtable_ == v.vtable_) {
        if (vtable_->type != id_t::kInvalid) { vtable_->assign_copy(&data_, &v.data_); }
        return *this;
    } else if (vtable_->type != id_t::kInvalid) {
        vtable_->destroy(&data_);
    }
    vtable_ = v.vtable_;
    if (vtable_->type != id_t::kInvalid) { vtable_->construct_copy(&data_, &v.data_); }
    return *this;
}

variant& variant::operator=(variant&& v) NOEXCEPT {
    if (&v == this) { return *this; }
    if (vtable_ == v.vtable_) {
        if (vtable_->type != id_t::kInvalid) { vtable_->assign_move(&data_, &v.data_); }
        return *this;
    } else if (vtable_->type != id_t::kInvalid) {
        vtable_->destroy(&data_);
    }
    vtable_ = v.vtable_;
    if (vtable_->type != id_t::kInvalid) { vtable_->construct_move(&data_, &v.data_); }
    return *this;
}

bool variant::can_convert(id_t type) const {
    auto* vtable = get_vtable(type);
    if (vtable_ == vtable) { return true; }
    return vtable->get_cvt(vtable_->type) != nullptr;
}

void variant::convert(id_t type) {
    auto* src_vtable = vtable_;
    vtable_ = get_vtable(type);
    if (vtable_ == src_vtable) {
        return;
    } else if (vtable_->type == id_t::kInvalid) {
        src_vtable->destroy(&data_);
        return;
    } else if (src_vtable->type == id_t::kInvalid) {
        vtable_->construct_default(&data_);
        return;
    } else if (auto cvt_func = vtable_->get_cvt(src_vtable->type)) {
        if (const auto* pval = src_vtable->get_value_ptr(&data_)) {
            storage_t tmp;
            cvt_func(vtable_->construct_default(&tmp), pval);
            src_vtable->destroy(&data_);
            vtable_->construct_move(&data_, &tmp);
            vtable_->destroy(&tmp);
            return;
        }
    }
    src_vtable->destroy(&data_);
    vtable_->construct_default(&data_);
}

#ifdef USE_QT
QDataStream& util::operator<<(QDataStream& os, const variant& v) {
    os << static_cast<unsigned>(v.vtable_->type);
    if (v.vtable_->type != variant_id::kInvalid) { v.vtable_->serialize(os, &v.data_); }
    return os;
}

QDataStream& util::operator>>(QDataStream& is, variant& v) {
    unsigned type = 0;
    is >> type;
    auto* vtable = variant::get_vtable(static_cast<variant_id>(type));
    if (v.vtable_ != vtable) {
        if (v.vtable_->type != variant_id::kInvalid) { v.vtable_->destroy(&v.data_); }
        v.vtable_ = vtable;
        if (vtable->type != variant_id::kInvalid) { vtable->construct_default(&v.data_); }
    }
    if (vtable->type != variant_id::kInvalid) { vtable->deserialize(is, &v.data_); }
    return is;
}
#endif  // USE_QT
