#pragma once
// Example host integration, NOT another public execution abstraction.
#include <stdexec/execution.hpp>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <mutex>

namespace example
{
    class WakeEvent final
    {
    public:
        void notify() noexcept
        {
            {
                std::lock_guard lock(mutex_);
                ++epoch_;
            }
            changed_.notify_one();
        }
        [[nodiscard]] std::uint64_t snapshot() noexcept
        {
            std::lock_guard lock(mutex_);
            return epoch_;
        }
        void wait(std::uint64_t observed)
        {
            std::unique_lock lock(mutex_);
            changed_.wait(lock, [&] { return epoch_ != observed; });
        }
        static void wake(void* self) noexcept { static_cast<WakeEvent*>(self)->notify(); }
    private:
        std::mutex mutex_;
        std::condition_variable changed_;
        std::uint64_t epoch_ = 0;
    };

    struct JoinReceiver final
    {
        using receiver_concept = stdexec::receiver_t;
        std::atomic<bool>* completed;
        WakeEvent* wake;
        auto get_env() const noexcept
        {
            return stdexec::env{stdexec::prop{stdexec::get_scheduler, stdexec::inline_scheduler{}}};
        }
        void set_value() && noexcept
        {
            auto* signal = wake;
            auto* terminal = completed;
            terminal->store(true, std::memory_order_release);
            // The observer may now destroy this receiver; use only the local
            // signal pointer. The host keeps the signal alive through producer join.
            signal->notify();
        }
        void set_stopped() && noexcept { std::terminate(); }
        template<class Error> void set_error(Error&&) && noexcept { std::terminate(); }
    };

    template<class Context>
    void pump_until_joined(Context& context, WakeEvent& wake, std::atomic<bool>& joined)
    {
        while (!joined.load(std::memory_order_acquire))
        {
            const auto observed = wake.snapshot(); // BEFORE all readiness checks.
            if (context.run_ready(128) != 0 || context.pending() != 0) continue;
            if (!joined.load(std::memory_order_acquire)) wake.wait(observed);
        }
    }
}
