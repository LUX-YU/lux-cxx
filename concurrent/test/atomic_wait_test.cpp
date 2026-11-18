#include <lux/cxx/concurrent/AtomicWait.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <thread>

int main()
{
    using namespace std::chrono_literals;

    std::atomic<std::uint64_t> value{1};
    assert(lux::cxx::concurrent::waitAtomicU64Until(
        value,
        0,
        std::chrono::steady_clock::now()));

    const auto timeout_begin = std::chrono::steady_clock::now();
    assert(!lux::cxx::concurrent::waitAtomicU64Until(
        value,
        1,
        timeout_begin + 20ms));
    assert(std::chrono::steady_clock::now() >= timeout_begin + 10ms);

    std::thread producer([&value]
    {
        std::this_thread::sleep_for(20ms);
        value.store(2, std::memory_order_release);
        value.notify_all();
    });
    assert(lux::cxx::concurrent::waitAtomicU64Until(
        value,
        1,
        std::chrono::steady_clock::now() + 2s));
    producer.join();
    assert(value.load(std::memory_order_acquire) == 2);
}
