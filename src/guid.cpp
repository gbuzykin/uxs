#include "uxs/guid.h"

#include <random>

using namespace uxs;

//---------------------------------------------------------------------------------
// Guid implementation

namespace {
std::random_device g_rd;
std::seed_seq g_seed{g_rd(), g_rd(), g_rd(), g_rd(), g_rd()};
std::mt19937 g_generator(g_seed);
}  // namespace

/*static*/ guid guid::generate() {
    guid id;
    std::uniform_int_distribution<std::uint32_t> distribution(0, std::numeric_limits<std::uint32_t>::max());
    for (std::uint32_t& l : id.data32) { l = distribution(g_generator); }
    // set version: must be 0b0100xxxx
    id.data8[7] = (id.data8[7] & 0x4F) | 0x40;
    // set variant: must be 0b10xxxxxx
    id.data8[8] = (id.data8[8] & 0xBF) | 0x80;
    return id;
}
