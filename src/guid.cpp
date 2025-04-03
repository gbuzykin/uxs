#include "uxs/guid.h"

#include <random>

using namespace uxs;

//---------------------------------------------------------------------------------
// Guid implementation

struct guid_random_generator {
    std::mt19937 generator;
    std::uniform_int_distribution<std::uint32_t> distribution;
    guid_random_generator() : distribution(0, std::numeric_limits<std::uint32_t>::max()) {
        std::random_device r;
        std::seed_seq seed{r(), r(), r(), r(), r()};
        generator.seed(seed);
    }
    guid operator()() {
        guid id;
        for (std::uint32_t& l : id.data32) { l = distribution(generator); }
        // set version: must be 0b0100xxxx
        id.data8[7] = (id.data8[7] & 0x4F) | 0x40;
        // set variant: must be 0b10xxxxxx
        id.data8[8] = (id.data8[8] & 0xBF) | 0x80;
        return id;
    }
};

/*static*/ guid guid::generate() {
    static guid_random_generator generator;
    return generator();
}
