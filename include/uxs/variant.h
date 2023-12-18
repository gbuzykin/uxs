#pragma once

#include "alignment.h"
#include "cow_ptr.h"
#include "optional.h"
#include "stringcvt.h"

#define UXS_DECLARE_VARIANT_TYPE(ty, id) \
    template<> \
    struct variant_type_impl<ty> : variant_type_base_impl<ty, id> { \
        static variant::vtable_t vtable; \
        static bool convert_from(variant_id, void*, const void*); \
        static bool convert_to(variant_id, void*, const void*); \
        variant_type_impl(); \
    }

#define UXS_IMPLEMENT_VARIANT_STRING_CONVERTER(ty) \
    bool uxs::variant_type_impl<ty>::convert_from(variant_id type, void* to, const void* from) { \
        if (type != variant_id::string) { return false; } \
        return uxs::stoval(*static_cast<const std::string*>(from), *static_cast<ty*>(to)) != 0; \
    } \
    bool uxs::variant_type_impl<ty>::convert_to(variant_id type, void* to, const void* from) { \
        if (type != variant_id::string) { return false; } \
        *static_cast<std::string*>(to) = uxs::to_string(*static_cast<const ty*>(from)); \
        return true; \
    }

#ifdef USE_QT
#    define UXS_IMPLEMENT_VARIANT_TYPE(ty, ...) \
        uxs::variant::vtable_t uxs::variant_type_impl<ty>::vtable{ \
            type_id,      construct_default, construct_copy,      construct_move, destroy, \
            assign_copy,  assign_move,       get_value_const_ptr, get_value_ptr,  is_equal, \
            serialize_qt, deserialize_qt,    __VA_ARGS__}; \
        uxs::variant_type_impl<ty>::variant_type_impl() { \
            static_assert(static_cast<unsigned>(type_id) < uxs::variant::max_type_id, "bad variant identifier"); \
            assert(vtable.type == type_id); \
            assert(!uxs::variant::vtables_[static_cast<unsigned>(type_id)]); \
            uxs::variant::vtables_[static_cast<unsigned>(type_id)] = &vtable; \
        } \
        static uxs::variant_type_impl<ty> TOKENPASTE2(g_variant_type_impl_, __LINE__)
#else
#    define UXS_IMPLEMENT_VARIANT_TYPE(ty, ...) \
        uxs::variant::vtable_t uxs::variant_type_impl<ty>::vtable{ \
            type_id,     construct_default,   construct_copy, construct_move, destroy,    assign_copy, \
            assign_move, get_value_const_ptr, get_value_ptr,  is_equal,       __VA_ARGS__}; \
        uxs::variant_type_impl<ty>::variant_type_impl() { \
            static_assert(static_cast<unsigned>(type_id) < uxs::variant::max_type_id, "bad variant identifier"); \
            assert(vtable.type == type_id); \
            assert(!uxs::variant::vtables_[static_cast<unsigned>(type_id)]); \
            uxs::variant::vtables_[static_cast<unsigned>(type_id)] = &vtable; \
        } \
        static uxs::variant_type_impl<ty> TOKENPASTE2(g_variant_type_impl_, __LINE__)
#endif  // USE_QT

#define UXS_IMPLEMENT_VARIANT_TYPE_WITH_STRING_CONVERTER(ty) \
    UXS_IMPLEMENT_VARIANT_STRING_CONVERTER(ty) \
    UXS_IMPLEMENT_VARIANT_TYPE(ty, convert_from, convert_to)

namespace uxs {

// Two variant are compared as values of type with greater identifier.
enum class variant_id : unsigned {
    invalid = 0,
    string,
    boolean,
    integer,
    unsigned_integer,
    long_integer,
    unsigned_long_integer,
    single_precision,
    double_precision,
    vector2d,
    vector3d,
    vector4d,
    quaternion,
    matrix4x4,
    custom0
};
CONSTEXPR variant_id operator+(variant_id lhs, unsigned rhs) {
    return static_cast<variant_id>(static_cast<unsigned>(lhs) + rhs);
}
CONSTEXPR variant_id operator+(unsigned lhs, variant_id rhs) {
    return static_cast<variant_id>(lhs + static_cast<unsigned>(rhs));
}

class variant_error : public std::runtime_error {
 public:
    explicit variant_error(const char* message) : std::runtime_error(message) {}
    explicit variant_error(const std::string& message) : std::runtime_error(message) {}
};

template<typename Ty>
struct variant_type_impl;

class variant {
 private:
    template<typename... Ts>
    using aligned_storage_t = typename std::aligned_storage<size_of<Ts...>::value, alignment_of<Ts...>::value>::type;
    using storage_t = aligned_storage_t<int64_t, double, void*, std::string>;

 public:
    enum : unsigned { max_type_id = 256 };
    enum : size_t { storage_size = sizeof(storage_t) };
    enum : size_t { storage_alignment = std::alignment_of<storage_t>::value };

    variant() NOEXCEPT {}
    explicit variant(variant_id type) : vtable_(get_vtable(type)) {
        if (vtable_) { vtable_->construct_default(&data_); }
    }

    variant(const variant& v) : vtable_(v.vtable_) {
        if (vtable_) { v.vtable_->construct_copy(&data_, &v.data_); }
    }
    variant(variant&& v) NOEXCEPT : vtable_(v.vtable_) {
        if (vtable_) { vtable_->construct_move(&data_, &v.data_); }
    }
    UXS_EXPORT variant(variant_id type, const variant& v);

    ~variant() {
        if (vtable_) { vtable_->destroy(&data_); }
    }

    template<typename Ty, typename... Args, typename = std::void_t<typename variant_type_impl<Ty>::is_variant_type_impl>>
    explicit variant(in_place_type_t<Ty>, Args&&... args) : vtable_(get_vtable(variant_type_impl<Ty>::type_id)) {
        assert(vtable_);
        variant_type_impl<Ty>::construct(&data_, std::forward<Args>(args)...);
    }

    template<typename Ty, typename = std::void_t<typename variant_type_impl<std::decay_t<Ty>>::is_variant_type_impl>>
    variant(Ty&& val) : vtable_(get_vtable(variant_type_impl<std::decay_t<Ty>>::type_id)) {
        assert(vtable_);
        variant_type_impl<std::decay_t<Ty>>::construct(&data_, std::forward<Ty>(val));
    }

    UXS_EXPORT variant& operator=(const variant& v);
    UXS_EXPORT variant& operator=(variant&& v) NOEXCEPT;

    template<typename Ty, typename... Args, typename = std::void_t<typename variant_type_impl<Ty>::is_variant_type_impl>>
    Ty& emplace(Args&&... args);

    template<typename Ty, typename = std::void_t<typename variant_type_impl<std::decay_t<Ty>>::is_variant_type_impl>>
    variant& operator=(Ty&& val);

    bool has_value() const NOEXCEPT { return vtable_ != nullptr; }
    explicit operator bool() const NOEXCEPT { return vtable_ != nullptr; }
    variant_id type() const NOEXCEPT { return vtable_ ? vtable_->type : variant_id::invalid; }

    template<typename Ty, typename = std::void_t<typename variant_type_impl<Ty>::is_variant_type_impl>>
    bool is() const NOEXCEPT {
        return vtable_ && vtable_->type == variant_type_impl<Ty>::type_id;
    }

    void reset() NOEXCEPT {
        if (vtable_) { vtable_->destroy(&data_); }
        vtable_ = nullptr;
    }

    template<typename Ty, typename = std::void_t<typename variant_type_impl<Ty>::is_variant_type_impl>>
    uxs::optional<Ty> get() const;

    template<typename Ty, typename U, typename = std::void_t<typename variant_type_impl<Ty>::is_variant_type_impl>>
    Ty value_or(U&& default_value) const {
        auto result = get<Ty>();
        return result ? *result : Ty(std::forward<U>(default_value));
    }

    template<typename Ty, typename = std::void_t<typename variant_type_impl<Ty>::is_variant_type_impl>>
    Ty value() const {
        return value_or<Ty>(Ty());
    }

    template<typename Ty, typename = std::void_t<typename variant_type_impl<std::decay_t<Ty>>::is_variant_type_impl>>
    Ty as() const;

    template<typename Ty, typename = std::void_t<typename variant_type_impl<std::decay_t<Ty>>::is_variant_type_impl>>
    Ty as();

    template<typename Ty, typename = std::void_t<typename variant_type_impl<Ty>::is_variant_type_impl>>
    bool convert() {
        return convert(variant_type_impl<Ty>::type_id);
    }
    UXS_EXPORT bool convert(variant_id type);

    template<typename Ty, typename U = Ty, typename = std::void_t<typename variant_type_impl<U>::is_variant_type_impl>>
    bool is_equal_to(const Ty& val) const;
    UXS_EXPORT bool is_equal_to(const variant& v) const;

    variant(std::string_view s);
    variant& operator=(std::string_view s);
    bool is_equal_to(std::string_view s) const;

    variant(const char* cstr);
    variant& operator=(const char* cstr);
    bool is_equal_to(const char* cstr) const;

#ifdef USE_QT
    UXS_EXPORT void serialize(QDataStream& os) const;
    UXS_EXPORT void deserialize(QDataStream& is);
#endif  // USE_QT

 private:
    struct vtable_t {
        variant_id type;
        void* (*construct_default)(void*);
        void (*construct_copy)(void*, const void*);
        void (*construct_move)(void*, void*) NOEXCEPT;
        void (*destroy)(void*) NOEXCEPT;
        void (*assign_copy)(void*, const void*);
        void (*assign_move)(void*, void*) NOEXCEPT;
        const void* (*get_value_const_ptr)(const void*)NOEXCEPT;
        void* (*get_value_ptr)(void*)NOEXCEPT;
        bool (*is_equal)(const void*, const void*);
#ifdef USE_QT
        void (*serialize_qt)(QDataStream&, const void*);
        void (*deserialize_qt)(QDataStream&, void*);
#endif  // USE_QT
        bool (*convert_from)(variant_id, void*, const void*);
        bool (*convert_to)(variant_id, void*, const void*);
    };

    template<typename>
    friend struct variant_type_impl;

    template<typename, typename = void>
    struct getters_specializer;

    UXS_EXPORT static vtable_t* vtables_[];
    vtable_t* vtable_ = nullptr;
    storage_t data_;

    static vtable_t* get_vtable(variant_id type) {
        assert(static_cast<unsigned>(type) < max_type_id);
        return vtables_[static_cast<unsigned>(type)];
    }
};

template<typename Ty, typename... Args, typename>
Ty& variant::emplace(Args&&... args) {
    vtable_t* val_vtable = get_vtable(variant_type_impl<Ty>::type_id);
    assert(val_vtable);
    reset();
    Ty& val = variant_type_impl<Ty>::construct(&data_, std::forward<Args>(args)...);
    vtable_ = val_vtable;
    return val;
}

template<typename Ty, typename>
variant& variant::operator=(Ty&& val) {
    using DecayedTy = std::decay_t<Ty>;
    vtable_t* val_vtable = get_vtable(variant_type_impl<DecayedTy>::type_id);
    assert(val_vtable);
    if (vtable_ == val_vtable) {
        variant_type_impl<DecayedTy>::assign(&data_, std::forward<Ty>(val));
    } else {
        if (!vtable_) {
            variant_type_impl<DecayedTy>::construct(&data_, std::forward<Ty>(val));
        } else {
            Ty tmp(std::forward<Ty>(val));
            vtable_->destroy(&data_);
            variant_type_impl<DecayedTy>::construct(&data_, std::move(tmp));
        }
        vtable_ = val_vtable;
    }
    return *this;
}

template<typename Ty, typename>
uxs::optional<Ty> variant::get() const {
    if (!vtable_) { return uxs::nullopt(); }
    vtable_t* val_vtable = get_vtable(variant_type_impl<Ty>::type_id);
    assert(vtable_ && val_vtable);
    if (vtable_ == val_vtable) { return *static_cast<const Ty*>(vtable_->get_value_const_ptr(&data_)); }
    uxs::optional<Ty> result(uxs::in_place());
    if (vtable_->type > val_vtable->type && vtable_->convert_to) {
        if (vtable_->convert_to(val_vtable->type, &*result, vtable_->get_value_const_ptr(&data_))) { return result; }
    } else if (val_vtable->convert_from) {
        assert(val_vtable->type > vtable_->type);
        if (val_vtable->convert_from(vtable_->type, &*result, vtable_->get_value_const_ptr(&data_))) { return result; }
    }
    return uxs::nullopt();
}

template<typename Ty, typename>
struct variant::getters_specializer {
    template<typename Ty_ = Ty, typename = std::void_t<typename variant_type_impl<Ty>::is_variant_type_impl>>
    static Ty_ as(const variant& v) {
        auto result = v.get<Ty>();
        if (result) { return *result; }
        throw variant_error("bad value conversion");
    }
};

template<typename Ty>
struct variant::getters_specializer<Ty, std::enable_if_t<std::is_reference<Ty>::value>> {
    using DecayedTy = std::decay_t<Ty>;
    static Ty as(const variant& v) {
        vtable_t* val_vtable = get_vtable(variant_type_impl<DecayedTy>::type_id);
        assert(val_vtable);
        if (v.vtable_ != val_vtable) { throw variant_error("invalid value type"); }
        return *static_cast<const DecayedTy*>(v.vtable_->get_value_const_ptr(&v.data_));
    }
    template<typename Ty_ = Ty, typename = std::enable_if_t<!std::is_const<std::remove_reference_t<Ty_>>::value>>
    static Ty_ as(variant& v) {
        vtable_t* val_vtable = get_vtable(variant_type_impl<DecayedTy>::type_id);
        assert(val_vtable);
        if (v.vtable_ != val_vtable) { throw variant_error("invalid value type"); }
        return std::forward<Ty>(*static_cast<DecayedTy*>(v.vtable_->get_value_ptr(&v.data_)));
    }
};

template<typename Ty, typename>
Ty variant::as() const {
    return getters_specializer<Ty>::as(*this);
}

template<typename Ty, typename>
Ty variant::as() {
    return getters_specializer<Ty>::as(*this);
}

template<typename Ty, typename U, typename>
bool variant::is_equal_to(const Ty& val) const {
    if (!vtable_) { return false; }
    vtable_t* val_vtable = get_vtable(variant_type_impl<U>::type_id);
    assert(vtable_ && val_vtable);
    if (vtable_ == val_vtable) { return *static_cast<const U*>(vtable_->get_value_const_ptr(&data_)) == val; }
    U tmp;
    if (vtable_->type > val_vtable->type && vtable_->convert_to) {
        return vtable_->convert_to(val_vtable->type, &tmp, vtable_->get_value_const_ptr(&data_)) && tmp == val;
    } else if (val_vtable->convert_from) {
        assert(val_vtable->type > vtable_->type);
        return val_vtable->convert_from(vtable_->type, &tmp, vtable_->get_value_const_ptr(&data_)) && tmp == val;
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

template<typename Ty, variant_id TypeId, typename = void>
struct variant_type_base_impl {
    using is_variant_type_impl = int;
    static const variant_id type_id = TypeId;

    static const cow_ptr<Ty>& deref(const void* p) { return *static_cast<const cow_ptr<Ty>*>(p); }
    static cow_ptr<Ty>& deref(void* p) { return *static_cast<cow_ptr<Ty>*>(p); }

    template<typename... Args>
    static Ty& construct(void* p, Args&&... args) {
        new (p) cow_ptr<Ty>(make_cow<Ty>(std::forward<Args>(args)...));
        return *deref(p);
    }
    static void* construct_default(void* p) { return &*deref(new (p) cow_ptr<Ty>()); }
    static void construct_copy(void* p, const void* src) { new (p) cow_ptr<Ty>(deref(src)); }
    static void construct_move(void* p, void* src) NOEXCEPT { new (p) cow_ptr<Ty>(std::move(deref(src))); }
    static void destroy(void* p) NOEXCEPT { deref(p).~cow_ptr<Ty>(); }

    template<typename U>
    static Ty& assign(void* p, U&& val) {
        if (deref(p)) { return *deref(p) = std::forward<U>(val); }
        deref(p) = make_cow<Ty>(std::forward<U>(val));
        return *deref(p);
    }
    static void assign_copy(void* p, const void* src) { deref(p) = deref(src); }
    static void assign_move(void* p, void* src) NOEXCEPT { deref(p) = std::move(deref(src)); }
    static const void* get_value_const_ptr(const void* p) NOEXCEPT { return &*deref(p); }
    static void* get_value_ptr(void* p) NOEXCEPT { return &*deref(p); }
    static bool is_equal(const void* lhs, const void* rhs) { return *deref(lhs) == *deref(rhs); }
#ifdef USE_QT
    static void serialize_qt(QDataStream& os, const void* p) { os << *deref(p); }
    static void deserialize_qt(QDataStream& is, void* p) {
        if (!deref(p)) { deref(p) = make_cow<Ty>(); }
        is >> *deref(p);
    }
#endif  // USE_QT
};

template<typename Ty, variant_id TypeId>
struct variant_type_base_impl<
    Ty, TypeId,
    std::enable_if_t<(sizeof(Ty) <= variant::storage_size) && (std::alignment_of<Ty>::value <= variant::storage_alignment) &&
                     std::is_nothrow_move_constructible<Ty>::value && std::is_nothrow_move_assignable<Ty>::value>> {
    using is_variant_type_impl = int;
    static const variant_id type_id = TypeId;

    static const Ty& deref(const void* p) { return *static_cast<const Ty*>(p); }
    static Ty& deref(void* p) { return *static_cast<Ty*>(p); }

    template<typename... Args>
    static Ty& construct(void* p, Args&&... args) {
        new (p) Ty(std::forward<Args>(args)...);
        return deref(p);
    }
    static void* construct_default(void* p) { return new (p) Ty(); }
    static void construct_copy(void* p, const void* src) { new (p) Ty(deref(src)); }
    static void construct_move(void* p, void* src) NOEXCEPT { new (p) Ty(std::move(deref(src))); }
    static void destroy(void* p) NOEXCEPT { deref(p).~Ty(); }

    template<typename U>
    static Ty& assign(void* p, U&& val) {
        return deref(p) = std::forward<U>(val);
    }
    static void assign_copy(void* p, const void* src) { deref(p) = deref(src); }
    static void assign_move(void* p, void* src) NOEXCEPT { deref(p) = std::move(deref(src)); }
    static const void* get_value_const_ptr(const void* p) NOEXCEPT { return p; }
    static void* get_value_ptr(void* p) NOEXCEPT { return p; }
    static bool is_equal(const void* lhs, const void* rhs) { return deref(lhs) == deref(rhs); }
#ifdef USE_QT
    static void serialize_qt(QDataStream& os, const void* p) { os << deref(p); }
    static void deserialize_qt(QDataStream& is, void* p) { is >> deref(p); }
#endif  // USE_QT
};

UXS_DECLARE_VARIANT_TYPE(std::string, variant_id::string);

inline variant::variant(std::string_view s) : variant(std::string(s)) {}
inline variant& variant::operator=(std::string_view s) { return operator=(std::string(s)); }
inline bool variant::is_equal_to(std::string_view s) const { return is_equal_to<std::string_view, std::string>(s); }

inline variant::variant(const char* cstr) : variant(std::string(cstr)) {}
inline variant& variant::operator=(const char* cstr) { return operator=(std::string(cstr)); }
inline bool variant::is_equal_to(const char* cstr) const { return is_equal_to<const char*, std::string>(cstr); }

UXS_DECLARE_VARIANT_TYPE(bool, variant_id::boolean);
UXS_DECLARE_VARIANT_TYPE(int32_t, variant_id::integer);
UXS_DECLARE_VARIANT_TYPE(uint32_t, variant_id::unsigned_integer);
UXS_DECLARE_VARIANT_TYPE(int64_t, variant_id::long_integer);
UXS_DECLARE_VARIANT_TYPE(uint64_t, variant_id::unsigned_long_integer);
UXS_DECLARE_VARIANT_TYPE(float, variant_id::single_precision);
UXS_DECLARE_VARIANT_TYPE(double, variant_id::double_precision);

}  // namespace uxs
