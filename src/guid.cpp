#include "uxs/guid.h"

#include <limits>
#include <random>

using namespace uxs;

//---------------------------------------------------------------------------------
// Guid implementation

namespace {
static std::random_device g_rd;
static std::seed_seq g_seed{g_rd(), g_rd(), g_rd(), g_rd(), g_rd()};
static std::mt19937 g_generator(g_seed);
}  // namespace

/*static*/ guid guid::generate() {
    guid id;
    std::uniform_int_distribution<uint32_t> distribution(0, std::numeric_limits<uint32_t>::max());
    for (uint32_t& l : id.data32_) { l = distribution(g_generator); }
    // set version: must be 0b0100xxxx
    id.data8_[7] = (id.data8_[7] & 0x4F) | 0x40;
    // set variant: must be 0b10xxxxxx
    id.data8_[8] = (id.data8_[8] & 0xBF) | 0x80;
    return id;
}
