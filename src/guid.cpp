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

struct guid_random_generator {
    std::mt19937 generator;
    std::uniform_int_distribution<std::uint32_t> distribution;
    spin_mutex generator_lock;
    guid_random_generator() : distribution(0, std::numeric_limits<std::uint32_t>::max()) {
        std::random_device r;
        std::seed_seq seed{r(), r(), r(), r(), r()};
        generator.seed(seed);
    }
    guid operator()() {
        guid::data32_t data;

        {
            std::lock_guard<spin_mutex> lk(generator_lock);
            for (std::uint32_t& l : data) { l = distribution(generator); }
        }

        guid id{data};
        // set version: must be 0b0100xxxx
        id.data.w[1] = (id.data.w[1] & 0x4FFF) | 0x4000;
        // set variant: must be 0b10xxxxxx
        id.data.b[0] = (id.data.b[0] & 0xBF) | 0x80;
        return id;
    }
};

}  // namespace

/*static*/ guid guid::generate() {
    static guid_random_generator generator;
    return generator();
}
