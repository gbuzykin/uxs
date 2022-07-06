#include "uxs/pool_allocator_impl.h"

namespace uxs {
namespace detail {
pool<std::allocator<void>> g_global_pool;
template class pool<std::allocator<void>>;
}  // namespace detail
}  // namespace uxs
