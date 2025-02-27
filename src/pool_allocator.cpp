#include "uxs/impl/pool_allocator_impl.h"

namespace uxs {
namespace detail {
UXS_EXPORT pool<std::allocator<void>> g_global_pool;
template class pool<std::allocator<void>>;
}  // namespace detail
}  // namespace uxs
