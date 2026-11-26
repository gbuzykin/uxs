#pragma once

#include "utility.h"  // IWYU pragma: keep

namespace uxs {

template<typename ArgValueTuple, typename RetVal, typename... FormalArgs>
class raii_caller {
 public:
    template<typename U>
    raii_caller(RetVal (*fn)(FormalArgs...), U&& args) : fn_(fn), args_(std::forward<U>(args)) {}

    raii_caller(raii_caller&& other) noexcept(std::is_nothrow_move_constructible<ArgValueTuple>())
        : fn_(other.fn_), args_(std::move(other.args_)) {
        other.fn_ = nullptr;
    }

    raii_caller& operator=(raii_caller&& other) noexcept(std::is_nothrow_move_assignable<ArgValueTuple>()) {
        if (&other == this) { return *this; }
        fn_ = other.fn_;
        args_ = std::move(other.args_);
        other.fn_ = nullptr;
        return *this;
    }

    ~raii_caller() {
        if (fn_) { invoke(std::make_index_sequence<std::tuple_size<ArgValueTuple>::value>()); }
    }

    void release() noexcept { fn_ = nullptr; }

 private:
    RetVal (*fn_)(FormalArgs...);
    ArgValueTuple args_;

    template<std::size_t... Indices>
    void invoke(std::index_sequence<Indices...>) noexcept {
        fn_(std::get<Indices>(args_)...);
    }
};

namespace detail {
template<typename ArgValueTuple, typename RetVal, typename... FormalArgs>
raii_caller<ArgValueTuple, RetVal, FormalArgs...> make_raii_caller_impl(RetVal (*fn)(FormalArgs...),
                                                                        ArgValueTuple&& args) {
    return {fn, std::forward<ArgValueTuple>(args)};
}
}  // namespace detail

template<typename Func, typename... Args, typename = std::enable_if_t<std::is_pointer<Func>::value>>
auto make_raii_caller(Func fn, Args&&... args)
    -> decltype(detail::make_raii_caller_impl(fn, std::make_tuple(std::forward<Args>(args)...))) {
    return detail::make_raii_caller_impl(fn, std::make_tuple(std::forward<Args>(args)...));
}

}  // namespace uxs
