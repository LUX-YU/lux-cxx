#include <lux/cxx/execution/ManualContext.hpp>
#include <exec/static_thread_pool.hpp>
#include "ExampleSupport.hpp"
#include <memory>
#include <thread>

namespace ex = stdexec;
using namespace lux::cxx::execution;

int main()
{
    example::WakeEvent wake; // Must outlive context AND producer notifier tails.
    ManualContext context(default_manual_context_config(), {&wake, &example::WakeEvent::wake});
    const auto owner = std::this_thread::get_id();
    unsigned received = 0;
    {
        // In an application this pool can be shared across Engine/Editor/Render.
        exec::static_thread_pool cpu{4};
        ex::counting_scope scope;
        for (unsigned i = 0; i < 32; ++i)
        {
            auto pipeline = ex::schedule(cpu.get_scheduler())
                | ex::then([i]() noexcept { return std::make_unique<unsigned>(i * i); })
                | ex::continues_on(context.get_scheduler(ScheduleMode::delivery))
                | ex::then([&](std::unique_ptr<unsigned> value) noexcept {
                    if (std::this_thread::get_id() != owner) std::terminate();
                    std::printf("owner received %u\n", *value);
                    ++received;
                })
                | ex::upon_error([](auto&&) noexcept { std::terminate(); });
            ex::spawn(std::move(pipeline), scope.get_token());
        }
        // Stop NEW roots, not the context needed by already accepted pipelines.
        scope.close();
        std::atomic<bool> joined{false};
        auto join = ex::connect(scope.join(), example::JoinReceiver{&joined, &wake});
        ex::start(join);
        example::pump_until_joined(context, wake, joined);
    } // Scope settled; the example's pool joins producer tails before context teardown.
    (void)context.close();
    if (received != 32 || !context.is_idle()) std::terminate();
    return 0;
}
