#include <lux/cxx/execution/ManualContext.hpp>
#include <cstdio>
#include <thread>

namespace ex = stdexec;
using namespace lux::cxx::execution;

struct Completion final
{
    using receiver_concept = ex::receiver_t;
    const char* name;
    ex::inplace_stop_token stop;
    auto get_env() const noexcept { return ex::env{ex::prop{ex::get_stop_token, stop}}; }
    void set_value() && noexcept { std::printf("%s: value\n", name); }
    void set_stopped() && noexcept { std::printf("%s: stopped\n", name); }
    void set_error(ScheduleError) && noexcept { std::printf("%s: context closed\n", name); }
};
int main()
{
    ManualContext context;
    ex::inplace_stop_source stop;
    auto cancellable = ex::connect(ex::schedule(context.get_scheduler()),
        Completion{"ordinary", stop.get_token()});
    auto delivery = ex::connect(ex::schedule(context.get_scheduler(ScheduleMode::delivery)),
        Completion{"terminal delivery", stop.get_token()});
    ex::start(cancellable);
    ex::start(delivery);
    stop.request_stop();
    // Both complete here, never on the thread that requested stop.
    (void)context.run_ready(16);
    (void)context.close();
    auto rejected = ex::connect(ex::schedule(context.get_scheduler()), Completion{"late start", {}});
    ex::start(rejected); // Typed error on THIS starting thread.
}
