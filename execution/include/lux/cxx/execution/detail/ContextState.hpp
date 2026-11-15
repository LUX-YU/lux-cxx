#pragma once

#include <atomic>
#include <cstddef>
#include <exception>
#include <limits>

namespace lux::cxx::execution::detail
{
    // The two facts are deliberately separate:
    //  * gate_: admission closed + publishers still using the queue/notifier;
    //  * pending_: admitted operations whose completion callback has not returned.
    //
    // Neither counter owns an operation. Neither proves that a borrowed scheduler
    // will not be used again: callers must settle/join their producers separately.
    class ContextState final
    {
    public:
        ContextState() noexcept = default;
        ContextState(const ContextState&) = delete;
        ContextState& operator=(const ContextState&) = delete;

        [[nodiscard]] bool try_enter_publisher() noexcept
        {
            auto observed = gate_.load(std::memory_order_relaxed);
            for (;;)
            {
                if ((observed & closed_bit) != 0) return false;
                if ((observed & count_mask) == count_mask) std::terminate();
                if (gate_.compare_exchange_weak(observed, observed + 1,
                        std::memory_order_acq_rel, std::memory_order_relaxed))
                    return true;
            }
        }

        // Acquires a publisher slot AND closes admission in one atomic operation.
        // The caller still has to leave_publisher(), even if this was already closed.
        // Its notifier is covered by that slot, exactly like an enqueue notifier.
        [[nodiscard]] bool enter_close() noexcept
        {
            auto observed = gate_.load(std::memory_order_relaxed);
            for (;;)
            {
                if ((observed & count_mask) == count_mask) std::terminate();
                const auto desired = (observed | closed_bit) + 1;
                if (gate_.compare_exchange_weak(observed, desired,
                        std::memory_order_acq_rel, std::memory_order_relaxed))
                    return (observed & closed_bit) == 0;
            }
        }

        // MUST be the publisher's final access to the context/notifier storage.
        // acq_rel RMWs chain the completion of earlier publishers into the final
        // release observed by idle(). No notification may touch the context after it.
        void leave_publisher() noexcept
        {
            gate_.fetch_sub(1, std::memory_order_acq_rel);
        }

        // Register before publishing the node; the consumer may immediately run it.
        void add_operation() noexcept
        {
            if (pending_.fetch_add(1, std::memory_order_relaxed) ==
                    (std::numeric_limits<std::size_t>::max)())
                std::terminate();
        }

        // Single consumer, AFTER the callbacks (which may destroy their operations).
        void complete_operations(std::size_t count) noexcept
        {
            pending_.fetch_sub(count, std::memory_order_release);
        }

        [[nodiscard]] bool closed() const noexcept
        {
            return (gate_.load(std::memory_order_acquire) & closed_bit) != 0;
        }

        [[nodiscard]] std::size_t pending() const noexcept
        {
            return pending_.load(std::memory_order_acquire);
        }

        [[nodiscard]] std::size_t publishers() const noexcept
        {
            return gate_.load(std::memory_order_acquire) & count_mask;
        }

        // An observation, NOT a general lifetime fence. After close() and after all
        // external callers have returned, true is stable until destruction.
        [[nodiscard]] bool idle() const noexcept
        {
            if (publishers() != 0) return false;
            return pending() == 0;
        }

    private:
        static constexpr auto closed_bit = std::size_t{1}
            << (std::numeric_limits<std::size_t>::digits - 1);
        static constexpr auto count_mask = closed_bit - 1;

        // Fixed internal alignment: no public ABI dependence on compiler-specific
        // hardware_destructive_interference_size defaults.
        alignas(64) std::atomic<std::size_t> gate_{0};
        alignas(64) std::atomic<std::size_t> pending_{0};
    };
}
