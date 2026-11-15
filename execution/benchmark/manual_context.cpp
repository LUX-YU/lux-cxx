#include <lux/cxx/execution/ManualContext.hpp>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <thread>
#include <vector>

namespace ex = stdexec;
using namespace lux::cxx::execution;

struct Receiver final
{
    using receiver_concept = ex::receiver_t;
    std::size_t* count;
    auto get_env() const noexcept { return ex::env{}; }
    void set_value() && noexcept { ++*count; }
    void set_stopped() && noexcept { std::terminate(); }
    void set_error(ScheduleError) && noexcept { std::terminate(); }
};
int main(int argc, char** argv)
{
    const unsigned producers = argc > 1 ? unsigned(std::strtoul(argv[1], nullptr, 10)) : 4;
    const unsigned per_producer = argc > 2 ? unsigned(std::strtoul(argv[2], nullptr, 10)) : 250000;
    const std::size_t budget = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 128;
    if (producers == 0 || producers > 128 || per_producer == 0 || budget == 0) return 2;
    const auto total = std::size_t{producers} * per_producer;
    using Sender = decltype(ex::schedule(std::declval<ManualContext::Scheduler>()));
    using Op = ex::connect_result_t<Sender, Receiver>;
    struct Slot { alignas(Op) std::byte bytes[sizeof(Op)]; };
    ManualContext context({total + producers * 64});
    auto slots = std::make_unique<Slot[]>(total);
    std::vector<std::unique_ptr<ManualContext::Producer>> tokens;
    for (unsigned i = 0; i < producers; ++i) tokens.emplace_back(new ManualContext::Producer(context));
    std::size_t completed = 0; // Consumer only, deliberately not an atomic statistic.
    std::barrier start(producers + 1);
    std::vector<std::jthread> workers;
    for (unsigned lane = 0; lane < producers; ++lane)
        workers.emplace_back([&, lane] {
            const auto scheduler = tokens[lane]->get_scheduler();
            start.arrive_and_wait();
            for (unsigned i = 0; i < per_producer; ++i)
            {
                auto* op = ::new (static_cast<void*>(slots[std::size_t{lane} * per_producer + i].bytes))
                    Op(ex::connect(ex::schedule(scheduler), Receiver{&completed}));
                ex::start(*op);
            }
        });
    const auto before = std::chrono::steady_clock::now();
    start.arrive_and_wait();
    while (completed != total) (void)context.run_ready(budget);
    workers.clear();
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - before).count();
    for (std::size_t i = 0; i < total; ++i)
        std::launder(reinterpret_cast<Op*>(slots[i].bytes))->~Op();
    (void)context.close();
    std::printf("producers=%u operations=%zu budget=%zu elapsed_s=%.6f operations_per_s=%.0f\n",
        producers, total, budget, elapsed, total / elapsed);
    std::puts("Measures connect/start/pump with preallocated operation storage and explicit tokens; excludes stdexec::spawn, IO and OS wakeups. No latency percentile claim.");
}
