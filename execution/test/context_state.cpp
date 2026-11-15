// Tests of the ACTUAL production state implementation. No stdexec/queue shim.
// These prove only this state component, not the complete scheduler integration.
#include <lux/cxx/execution/detail/ContextState.hpp>
#include "TestSupport.hpp"
#include <atomic>
#include <array>
#include <barrier>
#include <cstdint>
#include <thread>
#include <vector>

using lux::cxx::execution::detail::ContextState;

static void initial_and_close()
{
    ContextState state;
    CHECK(state.idle()); CHECK(!state.closed()); CHECK(state.pending() == 0);
    CHECK(state.enter_close()); CHECK(state.closed()); CHECK(!state.idle());
    state.leave_publisher(); CHECK(state.idle()); CHECK(!state.try_enter_publisher());
    CHECK(!state.enter_close()); CHECK(!state.idle());
    state.leave_publisher(); CHECK(state.idle());
}
static void unpublished_is_not_idle()
{
    ContextState state;
    CHECK(state.try_enter_publisher()); CHECK(!state.idle());
    state.add_operation(); CHECK(state.pending() == 1);
    state.leave_publisher(); CHECK(!state.idle());
    state.complete_operations(1); CHECK(state.idle());
}
static void callback_before_publisher_return()
{
    ContextState state;
    CHECK(state.try_enter_publisher()); state.add_operation();
    state.complete_operations(1); // Node ran while enqueue's notifier is still active.
    CHECK(state.pending() == 0); CHECK(state.publishers() == 1); CHECK(!state.idle());
    state.leave_publisher(); CHECK(state.idle());
}
static void admitted_work_survives_close()
{
    ContextState state;
    CHECK(state.try_enter_publisher());
    CHECK(state.enter_close()); state.leave_publisher();
    CHECK(!state.try_enter_publisher());
    state.add_operation(); // The earlier publisher was admitted before close.
    state.leave_publisher(); CHECK(!state.idle());
    state.complete_operations(1); CHECK(state.idle());
}
static void batch_accounting()
{
    ContextState state;
    for (unsigned i = 0; i < 200; ++i)
    {
        CHECK(state.try_enter_publisher()); state.add_operation(); state.leave_publisher();
    }
    state.complete_operations(64); CHECK(state.pending() == 136);
    state.complete_operations(64); CHECK(state.pending() == 72);
    state.complete_operations(72); CHECK(state.idle());
}
static void simultaneous_close()
{
    ContextState state;
    constexpr unsigned lanes = 8;
    std::barrier start(lanes);
    std::atomic<unsigned> changed{0};
    std::vector<std::jthread> workers;
    for (unsigned i = 0; i < lanes; ++i)
        workers.emplace_back([&] {
            start.arrive_and_wait();
            if (state.enter_close()) changed.fetch_add(1, std::memory_order_relaxed);
            state.leave_publisher();
        });
    workers.clear();
    CHECK(changed.load() == 1); CHECK(state.closed()); CHECK(state.idle());
}
static void close_races_admission()
{
    constexpr unsigned lanes = 6;
    constexpr unsigned per_lane = 20000;
    for (unsigned round = 0; round < 20; ++round)
    {
        ContextState state;
        std::barrier start(lanes + 1);
        std::atomic<std::size_t> accepted{0}, rejected{0};
        std::vector<std::jthread> workers;
        for (unsigned i = 0; i < lanes; ++i)
            workers.emplace_back([&] {
                start.arrive_and_wait();
                for (unsigned k = 0; k < per_lane; ++k)
                {
                    if (state.try_enter_publisher())
                    {
                        state.add_operation();
                        accepted.fetch_add(1, std::memory_order_relaxed);
                        state.leave_publisher();
                    }
                    else rejected.fetch_add(1, std::memory_order_relaxed);
                }
            });
        start.arrive_and_wait();
        if ((round % 2) != 0) std::this_thread::yield();
        CHECK(state.enter_close()); state.leave_publisher();
        workers.clear();
        CHECK(accepted.load() + rejected.load() == std::size_t{lanes} * per_lane);
        CHECK(state.pending() == accepted.load());
        state.complete_operations(accepted.load());
        CHECK(state.idle()); CHECK(state.closed());
    }
}
static void publisher_tail_visibility()
{
    ContextState state;
    std::atomic<bool> node_completed{false}, release_tail{false};
    int final_payload = 0;
    std::jthread producer([&] {
        CHECK(state.try_enter_publisher()); state.add_operation();
        node_completed.store(true, std::memory_order_release);
        while (!release_tail.load(std::memory_order_acquire)) std::this_thread::yield();
        final_payload = 123;
        state.leave_publisher();
    });
    test::eventually([&] { return node_completed.load(std::memory_order_acquire); });
    state.complete_operations(1);
    CHECK(state.enter_close()); state.leave_publisher();
    CHECK(!state.idle());
    release_tail.store(true, std::memory_order_release);
    test::eventually([&] { return state.idle(); });
    CHECK(final_payload == 123); // acquire of the publisher's final release.
    producer.join();
}
static void million_state_transitions()
{
    ContextState state;
    constexpr unsigned lanes = 8;
    constexpr unsigned per_lane = 125000;
    std::atomic<std::size_t> published{0};
    std::barrier start(lanes + 1);
    std::vector<std::jthread> workers;
    for (unsigned i = 0; i < lanes; ++i)
        workers.emplace_back([&] {
            start.arrive_and_wait();
            for (unsigned k = 0; k < per_lane; ++k)
            {
                CHECK(state.try_enter_publisher()); state.add_operation();
                published.fetch_add(1, std::memory_order_release);
                state.leave_publisher();
            }
        });
    start.arrive_and_wait();
    std::size_t consumed = 0;
    test::eventually([&] {
        const auto available = published.exchange(0, std::memory_order_acquire);
        if (available) { state.complete_operations(available); consumed += available; }
        return consumed == std::size_t{lanes} * per_lane;
    });
    workers.clear();
    CHECK(state.pending() == 0); CHECK(state.idle());
    CHECK(state.enter_close()); state.leave_publisher(); CHECK(state.idle());
}

int main()
{
    const test::Case cases[] = {
        {"initial_and_close", initial_and_close},
        {"unpublished_is_not_idle", unpublished_is_not_idle},
        {"callback_before_publisher_return", callback_before_publisher_return},
        {"admitted_work_survives_close", admitted_work_survives_close},
        {"batch_accounting", batch_accounting},
        {"simultaneous_close", simultaneous_close},
        {"close_races_admission", close_races_admission},
        {"publisher_tail_visibility", publisher_tail_visibility},
        {"million_state_transitions", million_state_transitions}
    };
    return test::run(cases);
}
