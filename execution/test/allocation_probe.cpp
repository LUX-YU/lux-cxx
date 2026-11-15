// Linux/linker-specific optional integration test. Counts BOTH C++ and queue mallocs.
#include <lux/cxx/execution/ManualContext.hpp>
#include "TestSupport.hpp"
#include <array>
#include <atomic>
#include <cstdlib>
#include <new>

static std::atomic<bool> counting{false};
static std::atomic<std::size_t> c_allocations{0}, cpp_allocations{0};
extern "C" void* __real_malloc(std::size_t);
extern "C" void* __real_calloc(std::size_t, std::size_t);
extern "C" void* __real_realloc(void*, std::size_t);
extern "C" void* __wrap_malloc(std::size_t n)
{
    if (counting.load(std::memory_order_relaxed)) c_allocations.fetch_add(1, std::memory_order_relaxed);
    return __real_malloc(n);
}
extern "C" void* __wrap_calloc(std::size_t n, std::size_t s)
{
    if (counting.load(std::memory_order_relaxed)) c_allocations.fetch_add(1, std::memory_order_relaxed);
    return __real_calloc(n, s);
}
extern "C" void* __wrap_realloc(void* p, std::size_t n)
{
    if (counting.load(std::memory_order_relaxed)) c_allocations.fetch_add(1, std::memory_order_relaxed);
    return __real_realloc(p, n);
}
void* operator new(std::size_t n)
{
    if (counting.load(std::memory_order_relaxed)) cpp_allocations.fetch_add(1, std::memory_order_relaxed);
    if (auto* p = std::malloc(n ? n : 1)) return p;
    std::terminate();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t n, std::align_val_t alignment)
{
    if (counting.load(std::memory_order_relaxed)) cpp_allocations.fetch_add(1, std::memory_order_relaxed);
    void* p = nullptr;
    if (posix_memalign(&p, static_cast<std::size_t>(alignment), n ? n : 1) != 0) std::terminate();
    return p;
}
void* operator new[](std::size_t n, std::align_val_t alignment) { return ::operator new(n, alignment); }
void operator delete(void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }

struct Receiver
{
    using receiver_concept = stdexec::receiver_t;
    unsigned* count;
    auto get_env() const noexcept { return stdexec::env{}; }
    void set_value() && noexcept { ++*count; }
    void set_error(lux::cxx::execution::ScheduleError) && noexcept { CHECK(false); }
    void set_stopped() && noexcept { CHECK(false); }
};
int main()
{
    using namespace lux::cxx::execution;
    ManualContext context({8192});
    ManualContext::Producer producer(context);
    using Sender = decltype(stdexec::schedule(producer.get_scheduler()));
    using Op = stdexec::connect_result_t<Sender, Receiver>;
    struct Slot { alignas(Op) std::byte bytes[sizeof(Op)]; };
    std::array<Slot, 128> slots;
    unsigned completed = 0;
    const auto batch = [&] {
        for (auto& slot : slots)
        {
            auto* op = ::new (static_cast<void*>(slot.bytes)) Op(stdexec::connect(
                stdexec::schedule(producer.get_scheduler()), Receiver{&completed}));
            stdexec::start(*op);
        }
        CHECK(context.run_ready(slots.size()) == slots.size());
        for (auto& slot : slots) std::launder(reinterpret_cast<Op*>(slot.bytes))->~Op();
    };
    for (unsigned i = 0; i < 64; ++i) batch(); // Warm producer/block bookkeeping.
    counting.store(true, std::memory_order_relaxed);
    for (unsigned i = 0; i < 10000; ++i) batch();
    counting.store(false, std::memory_order_relaxed);
    CHECK(c_allocations == 0); CHECK(cpp_allocations == 0);
    std::printf("warm connect/start/pump: C allocations=%zu C++ allocations=%zu; does NOT cover spawn/coroutine frames\n",
        c_allocations.load(), cpp_allocations.load());
}
