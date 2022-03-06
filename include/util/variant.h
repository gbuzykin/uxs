#pragma once

#include "alignment.h"
#include "cow_ptr.h"
#include "stream.h"
#include "string_cvt.h"

#include <array>
#include <cstring>

namespace util {

// Order of identifiers can affect comparison result.
// Two variant are compared as values of type with greater identifier.
enum class variant_id : unsigned {
    kInvalid = 0,
    kString,
    kBoolean,
    kUInteger,
    kInteger,
    kDouble,
    kVector2D,
    kVector3D,
    kVector4D,
    kQuaternion,
    kMatrix4x4,
    kUser0
};

template<typename Ty>
struct variant_type_impl;
template<typename Ty, variant_id TypeId, typename = void>
struct variant_type_base_impl;

class UTIL_EXPORT variant {
 private:
    template<typename... Ts>
    using aligned_storage_t = typename std::aligned_storage<size_of<Ts...>::value, alignment_of<Ts...>::value>::type;
    using storage_t = aligned_storage_t<double, std::string, void*>;
    using id_t = variant_id;

 public:
    enum : unsigned { kMaxTypeId = 32 };
    enum : size_t { kStorageSize = sizeof(storage_t) };

    variant() NOEXCEPT {}
    explicit variant(id_t type) : vtable_(get_vtable(type)) {
        if (vtable_) { vtable_->construct_default(&data_); }
    }

    variant(const variant& v) : vtable_(v.vtable_) {
        if (vtable_) { vtable_->construct_copy(&data_, &v.data_); }
    }
    variant(variant&& v) NOEXCEPT : vtable_(v.vtable_) {
        if (vtable_) { vtable_->construct_move(&data_, &v.data_); }
    }
    variant(id_t type, const variant& v);

    ~variant() { tidy(); }

    template<typename Ty, typename = std::void_t<typename variant_type_impl<Ty>::is_variant_type_impl>>
    variant(const Ty& val) : vtable_(variant_type_impl<Ty>::vtable()) {
        variant_type_impl<Ty>::construct(&data_, val);
    }

    template<typename Ty, typename = std::void_t<typename variant_type_impl<Ty>::is_variant_type_impl>>
    variant(Ty&& val) : vtable_(variant_type_impl<Ty>::vtable()) {
        variant_type_impl<Ty>::construct(&data_, std::move(val));
    }

    template<typename Ty, typename = std::void_t<typename variant_type_impl<Ty>::is_variant_type_impl>>
    variant(id_t type, const Ty& val);

    variant& operator=(const variant& v);
    variant& operator=(variant&& v) NOEXCEPT;

    template<typename Ty, typename = std::void_t<typename variant_type_impl<Ty>::is_variant_type_impl>>
    variant& operator=(const Ty& val);

    template<typename Ty, typename = std::void_t<typename variant_type_impl<Ty>::is_variant_type_impl>>
    variant& operator=(Ty&& val);

    variant(std::string_view s);
    variant(id_t type, std::string_view s);
    variant& operator=(std::string_view s);

    variant(const char* cstr);
    variant(id_t type, const char* cstr);
    variant& operator=(const char* cstr);

    bool valid() const { return !!vtable_; }
    id_t type() const { return vtable_ ? static_cast<id_t>(vtable_->type) : id_t::kInvalid; }

    template<typename Ty, typename = std::void_t<typename variant_type_impl<Ty>::is_variant_type_impl>>
    Ty value() const;
    template<typename Ty, typename = std::void_t<typename variant_type_impl<Ty>::is_variant_type_impl>>
    bool can_convert() const {
        return can_convert(variant_type_impl<Ty>::type_id);
    }
    template<typename Ty, typename = std::void_t<typename variant_type_impl<Ty>::is_variant_type_impl>>
    void convert() {
        convert(variant_type_impl<Ty>::type_id);
    }
    template<typename ValT, typename Ty = ValT,
             typename = std::void_t<typename variant_type_impl<Ty>::is_variant_type_impl>>
    bool is_equal_to(const ValT& val) const;
    bool is_equal_to(std::string_view s) const;
    bool is_equal_to(const char* cstr) const;

    bool can_convert(id_t type) const;
    void convert(id_t type);
    bool is_equal_to(const variant& v) const;

#ifdef USE_QT
    void serialize(QDataStream& os) const;
    void deserialize(QDataStream& is);
#endif  // USE_QT

 private:
    struct vtable_t {
        id_t type;
        void* (*construct_default)(void*);
        void (*construct_copy)(void*, const void*);
        void (*construct_move)(void*, void*);
        void (*destroy)(void*);
        void (*assign_copy)(void*, const void*);
        void (*assign_move)(void*, void*);
        const void* (*get_value_ptr)(const void*);
        bool (*equal)(const void*, const void*);
#ifdef USE_QT
        void (*serialize)(QDataStream&, const void*);
        void (*deserialize)(QDataStream&, void*);
#endif  // USE_QT
        using cvt_func_t = void (*)(void*, const void*);
        std::array<cvt_func_t, kMaxTypeId> cvt;
        cvt_func_t get_cvt(id_t type) {
            assert(static_cast<unsigned>(type) < kMaxTypeId);
            return cvt[static_cast<unsigned>(type)];
        }
        void set_cvt(id_t type, cvt_func_t fn) {
            assert(static_cast<unsigned>(type) < kMaxTypeId);
            cvt[static_cast<unsigned>(type)] = fn;
        }
    };

    template<typename, variant_id, typename>
    friend struct variant_type_base_impl;

    vtable_t* vtable_ = nullptr;
    storage_t data_;

    void tidy() {
        if (vtable_) { vtable_->destroy(&data_); }
        vtable_ = nullptr;
    }

    static vtable_t* get_vtable_raw(id_t type);
    static vtable_t* get_vtable(id_t type) {
        auto* vtable = get_vtable_raw(type);
        return vtable->type != id_t::kInvalid ? vtable : nullptr;
    }
    template<typename ImplT>
    static void init_vtable() {
        auto* vtable = get_vtable_raw(ImplT::type_id);
        vtable->type = ImplT::type_id;
        vtable->construct_default = ImplT::construct_default;
        vtable->construct_copy = ImplT::construct_copy;
        vtable->construct_move = ImplT::construct_move;
        vtable->destroy = ImplT::destroy;
        vtable->assign_copy = ImplT::assign_copy;
        vtable->assign_move = ImplT::assign_move;
        vtable->get_value_ptr = ImplT::get_value_ptr;
        vtable->equal = ImplT::equal;
#ifdef USE_QT
        vtable->serialize = ImplT::serialize;
        vtable->deserialize = ImplT::deserialize;
#endif  // USE_QT
    }
};

template<typename Ty, typename>
variant::variant(id_t type, const Ty& val) : vtable_(get_vtable(type)) {
    if (!vtable_) { return; }
    auto* val_vtable = variant_type_impl<Ty>::vtable();
    if (vtable_ == val_vtable) {
        variant_type_impl<Ty>::construct(&data_, val);
    } else {
        auto* tgt = vtable_->construct_default(&data_);
        if (auto cvt_func = vtable_->get_cvt(val_vtable->type)) {
            try {
                cvt_func(tgt, &val);
            } catch (...) {
                tidy();
                throw;
            }
        }
    }
}

template<typename Ty, typename>
variant& variant::operator=(const Ty& val) {
    auto* val_vtable = variant_type_impl<Ty>::vtable();
    if (vtable_ == val_vtable) {
        variant_type_impl<Ty>::assign(&data_, val);
    } else {
        tidy();
        variant_type_impl<Ty>::construct(&data_, val);
        vtable_ = val_vtable;
    }
    return *this;
}

template<typename Ty, typename>
variant& variant::operator=(Ty&& val) {
    auto* val_vtable = variant_type_impl<Ty>::vtable();
    if (vtable_ == val_vtable) {
        variant_type_impl<Ty>::assign(&data_, std::move(val));
    } else {
        tidy();
        variant_type_impl<Ty>::construct(&data_, std::move(val));
        vtable_ = val_vtable;
    }
    return *this;
}

template<typename Ty, typename>
Ty variant::value() const {
    if (!vtable_) { return {}; }
    auto* val_vtable = variant_type_impl<Ty>::vtable();
    if (vtable_ == val_vtable) {
        return *reinterpret_cast<const Ty*>(vtable_->get_value_ptr(&data_));
    } else if (auto cvt_func = val_vtable->get_cvt(vtable_->type)) {
        Ty result;
        cvt_func(&result, vtable_->get_value_ptr(&data_));
        return result;
    }
    return {};
}

template<typename ValT, typename Ty, typename>
bool variant::is_equal_to(const ValT& val) const {
    if (!vtable_) { return false; }
    auto* val_vtable = variant_type_impl<Ty>::vtable();
    if (vtable_ == val_vtable) {
        return *reinterpret_cast<const Ty*>(vtable_->get_value_ptr(&data_)) == val;
    } else if (auto cvt_func = val_vtable->get_cvt(vtable_->type)) {
        Ty tmp;
        cvt_func(&tmp, vtable_->get_value_ptr(&data_));
        return tmp == val;
    }
    return false;
}

namespace detail {
// Use `detail::cref_wrapper` to avoid implicit conversion to `variant`
template<typename Ty>
struct cref_wrapper {
    const Ty& v;
    cref_wrapper(const Ty& in_v) : v(in_v) {}
};
}  // namespace detail

template<typename Ty, typename = std::enable_if_t<std::is_convertible<Ty, variant>::value>>
bool operator==(detail::cref_wrapper<variant> lhs, const Ty& rhs) {
    return lhs.v.is_equal_to(rhs);
}
template<typename Ty, typename = std::enable_if_t<std::is_convertible<Ty, variant>::value>>
bool operator!=(detail::cref_wrapper<variant> lhs, const Ty& rhs) {
    return !lhs.v.is_equal_to(rhs);
}

template<typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, variant>::value && !std::is_same<Ty, variant>::value>>
bool operator==(const Ty& lhs, detail::cref_wrapper<variant> rhs) {
    return rhs.v.is_equal_to(lhs);
}
template<typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, variant>::value && !std::is_same<Ty, variant>::value>>
bool operator!=(const Ty& lhs, detail::cref_wrapper<variant> rhs) {
    return !rhs.v.is_equal_to(lhs);
}

template<typename Ty, variant_id TypeId, typename>
struct variant_type_base_impl {
    using is_variant_type_impl = int;
    static const variant_id type_id = TypeId;
    static variant::vtable_t* vtable() { return variant::get_vtable_raw(type_id); }
    variant_type_base_impl() { variant::init_vtable<variant_type_base_impl>(); }
    static const Ty& deref(const void* p) { return *reinterpret_cast<const Ty*>(p); }
    static Ty& deref(void* p) { return *reinterpret_cast<Ty*>(p); }

    template<typename Arg>
    static void construct(void* p, Arg&& val) {
        new (p) Ty(std::forward<Arg>(val));
    }
    static void* construct_default(void* p) { return new (p) Ty(); }
    static void construct_copy(void* p, const void* src) { new (p) Ty(deref(src)); }
    static void construct_move(void* p, void* src) { new (p) Ty(std::move(deref(src))); }
    static void destroy(void* p) { deref(p).~Ty(); }

    template<typename Arg>
    static void assign(void* p, Arg&& val) {
        deref(p) = std::forward<Arg>(val);
    }
    static void assign_copy(void* p, const void* src) { deref(p) = deref(src); }
    static void assign_move(void* p, void* src) { deref(p) = std::move(deref(src)); }
    static const void* get_value_ptr(const void* p) { return p; }
    static bool equal(const void* lhs, const void* rhs) { return deref(lhs) == deref(rhs); }
    template<typename Ty2>
    static void cast_cvt(void* to, const void* from) {
        *reinterpret_cast<Ty*>(to) = static_cast<Ty>(*reinterpret_cast<const Ty2*>(from));
    }
#ifdef USE_QT
    static void serialize(QDataStream& os, const void* p) { os << deref(p); }
    static void deserialize(QDataStream& is, void* p) { is >> deref(p); }
#endif  // USE_QT
};

template<typename Ty, variant_id TypeId>
struct variant_type_base_impl<
    Ty, TypeId,
    std::enable_if_t<(sizeof(Ty) > variant::kStorageSize) || !std::is_nothrow_move_constructible<Ty>::value ||
                     !std::is_nothrow_move_assignable<Ty>::value>> {
    using is_variant_type_impl = int;
    static const variant_id type_id = TypeId;
    static variant::vtable_t* vtable() { return variant::get_vtable_raw(type_id); }
    variant_type_base_impl() { variant::init_vtable<variant_type_base_impl>(); }
    static const cow_ptr<Ty>& deref(const void* p) { return *reinterpret_cast<const cow_ptr<Ty>*>(p); }
    static cow_ptr<Ty>& deref(void* p) { return *reinterpret_cast<cow_ptr<Ty>*>(p); }

    template<typename Arg>
    static void construct(void* p, Arg&& val) {
        new (p) cow_ptr<Ty>(make_cow<Ty>(std::forward<Arg>(val)));
    }
    static void* construct_default(void* p) { return std::addressof(*deref(new (p) cow_ptr<Ty>())); }
    static void construct_copy(void* p, const void* src) { new (p) cow_ptr<Ty>(deref(src)); }
    static void construct_move(void* p, void* src) { new (p) cow_ptr<Ty>(std::move(deref(src))); }
    static void destroy(void* p) { deref(p).~cow_ptr<Ty>(); }

    template<typename Arg>
    static void assign(void* p, Arg&& val) {
        if (deref(p)) {
            *deref(p) = std::forward<Arg>(val);
        } else {
            deref(p) = make_cow<Ty>(std::forward<Arg>(val));
        }
    }
    static void assign_copy(void* p, const void* src) { deref(p) = deref(src); }
    static void assign_move(void* p, void* src) { deref(p) = std::move(deref(src)); }
    static const void* get_value_ptr(const void* p) { return std::addressof(*deref(p)); }
    static bool equal(const void* lhs, const void* rhs) { return *deref(lhs) == *deref(rhs); }
    template<typename Ty2>
    static void cast_cvt(void* to, const void* from) {
        *reinterpret_cast<Ty*>(to) = static_cast<Ty>(*reinterpret_cast<const Ty2*>(from));
    }
#ifdef USE_QT
    static void serialize(QDataStream& os, const void* p) { os << *deref(p); }
    static void deserialize(QDataStream& is, void* p) {
        if (!deref(p)) { deref(p) = make_cow<Ty>(); }
        is >> *deref(p);
    }
#endif  // USE_QT
};

template<>
struct variant_type_impl<std::string> : variant_type_base_impl<std::string, variant_id::kString> {};

inline variant::variant(std::string_view s) : variant(static_cast<std::string>(s)) {}
inline variant::variant(id_t type, std::string_view s) : variant(type, static_cast<std::string>(s)) {}
inline variant& variant::operator=(std::string_view s) { return operator=(static_cast<std::string>(s)); }

inline variant::variant(const char* cstr) : variant(std::string(cstr)) {}
inline variant::variant(id_t type, const char* cstr) : variant(type, std::string(cstr)) {}
inline variant& variant::operator=(const char* cstr) { return operator=(std::string(cstr)); }

inline bool variant::is_equal_to(std::string_view s) const { return is_equal_to<std::string_view, std::string>(s); }
inline bool variant::is_equal_to(const char* cstr) const { return is_equal_to<const char*, std::string>(cstr); }

template<typename Ty, variant_id TypeId>
struct variant_type_with_string_converter_impl : variant_type_base_impl<Ty, TypeId> {
    using super = variant_type_base_impl<Ty, TypeId>;
    variant_type_with_string_converter_impl() {
        super::vtable()->set_cvt(variant_id::kString, from_string_cvt);
        variant_type_impl<std::string>::vtable()->set_cvt(super::type_id, to_string_cvt);
    }
    static void from_string_cvt(void* to, const void* from) {
        *reinterpret_cast<Ty*>(to) = from_string<Ty>(*reinterpret_cast<const std::string*>(from));
    }
    static void to_string_cvt(void* to, const void* from) {
        *reinterpret_cast<std::string*>(to) = to_string(*reinterpret_cast<const Ty*>(from));
    }
};

template<>
struct variant_type_impl<bool> : variant_type_with_string_converter_impl<bool, variant_id::kBoolean> {
    template<typename Ty>
    static void to_bool_cvt(void* to, const void* from) {
        *reinterpret_cast<bool*>(to) = !!*reinterpret_cast<const Ty*>(from);
    }
    variant_type_impl() {
        vtable()->set_cvt(variant_id::kInteger, to_bool_cvt<int>);
        vtable()->set_cvt(variant_id::kUInteger, to_bool_cvt<unsigned>);
        vtable()->set_cvt(variant_id::kDouble, to_bool_cvt<double>);
    }
};

template<>
struct variant_type_impl<int> : variant_type_with_string_converter_impl<int, variant_id::kInteger> {
    variant_type_impl() {
        vtable()->set_cvt(variant_id::kBoolean, cast_cvt<bool>);
        vtable()->set_cvt(variant_id::kUInteger, cast_cvt<unsigned>);
        vtable()->set_cvt(variant_id::kDouble, cast_cvt<double>);
    }
};

template<>
struct variant_type_impl<unsigned> : variant_type_with_string_converter_impl<unsigned, variant_id::kUInteger> {
    variant_type_impl() {
        vtable()->set_cvt(variant_id::kBoolean, cast_cvt<bool>);
        vtable()->set_cvt(variant_id::kInteger, cast_cvt<int>);
        vtable()->set_cvt(variant_id::kDouble, cast_cvt<double>);
    }
};

template<>
struct variant_type_impl<double> : variant_type_with_string_converter_impl<double, variant_id::kDouble> {
    variant_type_impl() {
        vtable()->set_cvt(variant_id::kBoolean, cast_cvt<bool>);
        vtable()->set_cvt(variant_id::kInteger, cast_cvt<int>);
        vtable()->set_cvt(variant_id::kUInteger, cast_cvt<unsigned>);
    }
};

}  // namespace util
