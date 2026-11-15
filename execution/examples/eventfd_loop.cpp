// Linux reactor integration: real eventfd/epoll wakeups, no network protocol layer.
#include <lux/cxx/execution/ManualContext.hpp>
#include <exec/static_thread_pool.hpp>
#include "ExampleSupport.hpp"
#include <cerrno>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace ex = stdexec;
using namespace lux::cxx::execution;

class ReactorWake final
{
public:
    ReactorWake() noexcept
    {
        event_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        poll_ = epoll_create1(EPOLL_CLOEXEC);
        if (event_ < 0 || poll_ < 0) std::terminate();
        epoll_event event{};
        event.events = EPOLLIN;
        event.data.fd = event_;
        if (epoll_ctl(poll_, EPOLL_CTL_ADD, event_, &event) != 0) std::terminate();
    }
    ~ReactorWake() { ::close(poll_); ::close(event_); }
    ReactorWake(const ReactorWake&) = delete;
    ReactorWake& operator=(const ReactorWake&) = delete;
    static void notify(void* p) noexcept
    {
        auto& self = *static_cast<ReactorWake*>(p);
        for (;;)
        {
            if (eventfd_write(self.event_, 1) == 0) return;
            if (errno == EINTR) continue;
            if (errno == EAGAIN) return; // Already readable; a wake is pending.
            std::terminate();
        }
    }
    void consume() noexcept
    {
        eventfd_t count{};
        while (eventfd_read(event_, &count) == 0) {}
        if (errno != EAGAIN && errno != EINTR) std::terminate();
    }
    void wait() noexcept
    {
        epoll_event event{};
        while (epoll_wait(poll_, &event, 1, -1) < 0)
            if (errno != EINTR) std::terminate();
    }
private:
    int event_ = -1, poll_ = -1;
};

int main()
{
    ReactorWake reactor;
    ManualContext context(default_manual_context_config(), {&reactor, &ReactorWake::notify});
    unsigned completed = 0;
    {
        exec::static_thread_pool cpu{4};
        ex::counting_scope scope;
        for (unsigned i = 0; i < 1000; ++i)
        {
            auto task = ex::schedule(cpu.get_scheduler())
                | ex::continues_on(context.get_scheduler(ScheduleMode::delivery))
                | ex::then([&]() noexcept { ++completed; })
                | ex::upon_error([](auto&&) noexcept { std::terminate(); });
            ex::spawn(std::move(task), scope.get_token());
        }
        scope.close();
        // Only this consumer executes the final callback; run_ready() returns
        // after it and the scope's terminal path. No scope wait can need more
        // owner work once all 1000 final callbacks have returned.
        while (completed != 1000)
        {
            reactor.consume();
            const auto dispatched = context.run_ready(128);
            if (completed == 1000) break;
            if (dispatched != 0 || context.pending() != 0) continue;
            // eventfd retains a notification arriving between this check and wait.
            reactor.wait();
        }
        (void)ex::sync_wait(scope.join()); // All owner-dependent work has ended.
    }
    (void)context.close();
    if (!context.is_idle()) std::terminate();
    std::puts("epoll integration: 1000 owner completions");
}
