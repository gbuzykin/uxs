#include "uxs/guid.h"

#include <atomic>
#include <mutex>
#include <random>

using namespace uxs;

//---------------------------------------------------------------------------------
// Guid implementation

namespace {

class spin_mutex {
 public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) {}
    }
    void unlock() { flag_.clear(std::memory_order_release); }

 private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

struct random_generator {
    std::mt19937 generator;
    std::uniform_int_distribution<std::uint64_t> distribution;
    random_generator() : distribution(0, std::numeric_limits<std::uint64_t>::max()) {
        std::random_device r;
        std::seed_seq seed{r(), r(), r(), r(), r()};
        generator.seed(seed);
    }
    static guid::data64_t generate() {
        static bool is_initialized = false;
        static spin_mutex lock;
        alignas(std::alignment_of<random_generator>::value) static std::uint8_t v[sizeof(random_generator)];

        std::lock_guard<spin_mutex> lk(lock);

        auto& g = *reinterpret_cast<random_generator*>(&v);
        if (!is_initialized) {
            new (&g) random_generator;
            is_initialized = true;
        }

        return {g.distribution(g.generator), g.distribution(g.generator)};
    }
};

}  // namespace

guid guid::generate() {
    guid id(random_generator::generate());

    // set version: must be 0b0100xxxxxxxxxxxx
    id.layout.w[1] = (id.layout.w[1] & 0x0fff) | 0x4000;
    // set variant: must be 0b10xxxxxx
    id.layout.b[0] = (id.layout.b[0] & 0x3f) | 0x80;

    return id;
}
