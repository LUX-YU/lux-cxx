#include <lux/cxx/execution/ManualContext.hpp>
#include <exec/static_thread_pool.hpp>
#include "ExampleSupport.hpp"
#include <thread>

namespace ex = stdexec;
using namespace lux::cxx::execution;

int main()
{
    example::WakeEvent main_wake, io_wake;
    ManualContext main_context(default_manual_context_config(), {&main_wake, &example::WakeEvent::wake});
    ManualContext io_context(default_manual_context_config(), {&io_wake, &example::WakeEvent::wake});
    const auto main_thread = std::this_thread::get_id();
    unsigned replies = 0;
    {
        exec::static_thread_pool cpu{2};
        std::jthread io_thread([&](std::stop_token stop) {
            while (!stop.stop_requested())
            {
                const auto observed = io_wake.snapshot();
                // A real IO owner would also process its reactor events here.
                if (io_context.run_ready(128) != 0 || io_context.pending() != 0) continue;
                if (!stop.stop_requested()) io_wake.wait(observed);
            }
        });
        ex::counting_scope scope;
        for (unsigned i = 0; i < 100; ++i)
        {
            auto request = ex::schedule(io_context.get_scheduler())
                | ex::then([i]() noexcept { return i; })
                | ex::continues_on(cpu.get_scheduler())
                | ex::then([](unsigned x) noexcept { return x * 2; })
                | ex::continues_on(main_context.get_scheduler(ScheduleMode::delivery))
                | ex::then([&, i](unsigned x) noexcept {
                    if (std::this_thread::get_id() != main_thread || x != i * 2) std::terminate();
                    ++replies;
                })
                | ex::upon_error([](auto&&) noexcept { std::terminate(); });
            ex::spawn(std::move(request), scope.get_token());
        }
        scope.close();
        std::atomic<bool> joined{false};
        auto join = ex::connect(scope.join(), example::JoinReceiver{&joined, &main_wake});
        ex::start(join);
        example::pump_until_joined(main_context, main_wake, joined);
        // All roots ended. No later hop to io_context is possible in this example.
        io_thread.request_stop();
        io_wake.notify();
        io_thread.join();
    } // cpu pool joins any final publisher tails.
    (void)io_context.close();
    (void)main_context.close();
    if (replies != 100 || !io_context.is_idle() || !main_context.is_idle()) std::terminate();
    std::puts("100 requests crossed IO -> shared CPU pool -> main owner");
}
