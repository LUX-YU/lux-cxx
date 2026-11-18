#pragma once

#include <lux/cxx/execution/detail/ContextState.hpp>
#include <stdexec/execution.hpp>

#if __has_include(<concurrentqueue.h>)
#  include <concurrentqueue.h>
#elif __has_include(<concurrentqueue/concurrentqueue.h>)
#  include <concurrentqueue/concurrentqueue.h>
#elif __has_include(<moodycamel/concurrentqueue.h>)
#  include <moodycamel/concurrentqueue.h>
#else
#  error "lux::cxx::execution requires cameron314/concurrentqueue (no mutex fallback)."
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <type_traits>
#include <utility>

namespace lux::cxx::execution
{
    // Values only. This is an allocation hint, NOT a capacity/admission limit.
    struct ManualContextConfig final
    {
        std::size_t initial_queue_capacity;
    };

    [[nodiscard]] constexpr ManualContextConfig default_manual_context_config() noexcept
    {
        return {1024};
    }

    // Runtime assembly, deliberately not part of the serializable configuration.
    // Called after publication, on the producing thread. Must be noexcept,
    // thread-safe, fast, and must NOT pump/destroy this context or wait for it.
    // The callback target must outlive all publishing/close() calls.
    struct Wakeup final
    {
        void* context = nullptr;
        void (*notify)(void*) noexcept = nullptr;
    };

    enum class ScheduleError : std::uint8_t
    {
        context_closed
    };

    // delivery suppresses only this schedule operation's stop check. It does not
    // resurrect an already stopped upstream sender or override closed admission.
    enum class ScheduleMode : std::uint8_t
    {
        cancellable,
        delivery
    };

    // A manually driven, single-consumer execution resource. No thread, IO reactor,
    // task ownership, thread pool, future, or scope is created by this type.
    class ManualContext final
    {
        struct Node
        {
            void (*complete)(Node*) noexcept;
        };

        using Queue = moodycamel::ConcurrentQueue<Node*>;
        class ScheduleSender;
        template <class Receiver> class ScheduleOperation;

    public:
        class Scheduler;
        class Producer;

        explicit ManualContext(
            ManualContextConfig config = default_manual_context_config(),
            Wakeup wakeup = {}) noexcept
            : queue_(config.initial_queue_capacity), consumer_(queue_), wakeup_(wakeup)
        {
        }

        ManualContext(const ManualContext&) = delete;
        ManualContext& operator=(const ManualContext&) = delete;
        ManualContext(ManualContext&&) = delete;
        ManualContext& operator=(ManualContext&&) = delete;

        // Does not run user code, wait, cancel, or silently discard nodes.
        // Scope settlement and return/join of all producers AND the consumer are
        // the owner's responsibility. idle() alone does not authorize destruction.
        ~ManualContext() noexcept
        {
            if (driving_ || !state_.idle()) std::terminate();
        }

        [[nodiscard]] Scheduler get_scheduler(
            ScheduleMode mode = ScheduleMode::cancellable) noexcept;

        // Single consumer only; non-reentrant. Up to budget nodes, not a time limit.
        // A completed node can be destroyed synchronously by its receiver.
        [[nodiscard]] std::size_t run_ready(std::size_t budget) noexcept
        {
            if (driving_) std::terminate();
            driving_ = true;
            std::array<Node*, dequeue_batch> batch;
            std::size_t total = 0;
            while (total < budget)
            {
                const auto count = queue_.try_dequeue_bulk(consumer_, batch.begin(),
                    (std::min)(dequeue_batch, budget - total));
                if (count == 0) break;
                for (std::size_t i = 0; i < count; ++i)
                {
                    auto* node = batch[i];
                    node->complete(node);
                    // No further access to node: receiver completion may delete it.
                }
                state_.complete_operations(count);
                total += count;
            }
            driving_ = false;
            return total;
        }

        // Thread-safe, one-way admission seal; does not cancel admitted nodes.
        // Keep the context open while accepted pipelines still need to schedule
        // back here. Seal only after application admission/scopes are settled.
        // A later start() completes with ScheduleError on the starting thread.
        // Returns true exactly for the call that seals admission.
        [[nodiscard]] bool close() noexcept
        {
            const bool changed = state_.enter_close();
            if (changed && wakeup_.notify) wakeup_.notify(wakeup_.context);
            state_.leave_publisher(); // Last access to context in this invocation.
            return changed;
        }

        [[nodiscard]] bool is_closed() const noexcept { return state_.closed(); }

        // Includes unpublished admitted operations and executing/batched callbacks.
        // Not a count of immediately dequeueable nodes, and not an IO-work count.
        [[nodiscard]] std::size_t pending() const noexcept { return state_.pending(); }

        // Includes the producer's notifier tail, even if its node was already run.
        // Advisory while admission is open. Does NOT join potential future callers.
        [[nodiscard]] bool is_idle() const noexcept { return state_.idle(); }

    private:
        friend class Producer;
        template <class Receiver> friend class ScheduleOperation;
        static constexpr std::size_t dequeue_batch = 64;

        // true means admission succeeded. After successful enqueue no node access
        // is permitted here, even on the publisher thread.
        [[nodiscard]] bool enqueue(Node* node, const moodycamel::ProducerToken* producer) noexcept
        {
            if (!state_.try_enter_publisher()) return false;
            state_.add_operation();
            const bool published = producer ? queue_.enqueue(*producer, node) : queue_.enqueue(node);
            if (!published) std::terminate(); // Allocation failure; never silently drop accepted work.
            if (wakeup_.notify) wakeup_.notify(wakeup_.context);
            state_.leave_publisher(); // Last access; includes queue and notifier lifetime.
            return true;
        }

        Queue queue_;
        moodycamel::ConsumerToken consumer_;
        Wakeup wakeup_;
        detail::ContextState state_;
        bool driving_ = false; // Single-consumer contract, not a thread identity field.
    };

    // Optional explicit producer. Construction may allocate. This object must
    // remain at a stable address and outlive all handles that reference it.
    // Starts through one Producer must not overlap; different Producers can run
    // concurrently. The default scheduler has no such per-producer restriction.
    class ManualContext::Producer final
    {
    public:
        explicit Producer(ManualContext& context) noexcept
            : context_(context), token_(context.queue_)
        {
            if (!token_.valid()) std::terminate();
        }

        Producer(const Producer&) = delete;
        Producer& operator=(const Producer&) = delete;
        Producer(Producer&&) = delete;
        Producer& operator=(Producer&&) = delete;

        [[nodiscard]] Scheduler get_scheduler(
            ScheduleMode mode = ScheduleMode::cancellable) noexcept;

    private:
        ManualContext& context_;
        moodycamel::ProducerToken token_;
    };

    class ManualContext::Scheduler final
    {
    public:
        using scheduler_concept = stdexec::scheduler_t;

        Scheduler(const Scheduler&) noexcept = default;
        Scheduler& operator=(const Scheduler&) noexcept = default;
        [[nodiscard]] bool operator==(const Scheduler&) const noexcept = default;

        [[nodiscard]] ScheduleSender schedule() const noexcept;

        [[nodiscard]] stdexec::forward_progress_guarantee query(
            stdexec::get_forward_progress_guarantee_t) const noexcept
        {
            // The host must pump this context; it owns no independently running thread.
            return stdexec::forward_progress_guarantee::weakly_parallel;
        }

    private:
        friend class ManualContext;
        friend class Producer;
        friend class ScheduleSender;

        Scheduler(ManualContext& context, const moodycamel::ProducerToken* producer,
            ScheduleMode mode) noexcept
            : context_(&context), producer_(producer), mode_(mode)
        {
        }

        ManualContext* context_;
        const moodycamel::ProducerToken* producer_;
        ScheduleMode mode_;
    };

    template <class Receiver>
    class ManualContext::ScheduleOperation final : private Node
    {
    public:
        using operation_state_concept = stdexec::operation_state_t;

        ScheduleOperation(ManualContext& context, const moodycamel::ProducerToken* producer,
            ScheduleMode mode, Receiver receiver) noexcept(std::is_nothrow_move_constructible_v<Receiver>)
            : Node{&complete_on_context}, context_(context), producer_(producer),
              mode_(mode), receiver_(std::move(receiver))
        {
        }

        ScheduleOperation(const ScheduleOperation&) = delete;
        ScheduleOperation& operator=(const ScheduleOperation&) = delete;
        ScheduleOperation(ScheduleOperation&&) = delete;
        ScheduleOperation& operator=(ScheduleOperation&&) = delete;

        void start() & noexcept
        {
            if (!context_.enqueue(this, producer_))
            {
                stdexec::set_error(std::move(receiver_), ScheduleError::context_closed);
            }
            // No operation access after enqueue or set_error: either may have
            // caused completion and destruction of this operation.
        }

    private:
        static void complete_on_context(Node* node) noexcept
        {
            auto& self = *static_cast<ScheduleOperation*>(node);
            if (self.mode_ == ScheduleMode::cancellable &&
                stdexec::get_stop_token(stdexec::get_env(self.receiver_)).stop_requested())
            {
                stdexec::set_stopped(std::move(self.receiver_));
            }
            else
            {
                stdexec::set_value(std::move(self.receiver_));
            }
            // No operation access after completion.
        }

        ManualContext& context_;
        const moodycamel::ProducerToken* producer_;
        ScheduleMode mode_;
        Receiver receiver_;
    };

    class ManualContext::ScheduleSender final
    {
    public:
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(), stdexec::set_stopped_t(), stdexec::set_error_t(ScheduleError)
        >;

        struct Env final
        {
            Scheduler scheduler;
            [[nodiscard]] Scheduler
            query(stdexec::get_completion_scheduler_t<stdexec::set_value_t>) const noexcept
            {
                return scheduler;
            }

            [[nodiscard]] Scheduler
            query(stdexec::get_completion_scheduler_t<stdexec::set_stopped_t>) const noexcept
            {
                return scheduler;
            }
            // No set_error completion scheduler: a closed context rejects on the
            // starting thread, not necessarily on the context's consumer.
        };

        explicit ScheduleSender(Scheduler scheduler) noexcept : scheduler_(scheduler) {}
        [[nodiscard]] Env get_env() const noexcept { return {scheduler_}; }

        template <stdexec::receiver Receiver>
            requires stdexec::receiver_of<Receiver, completion_signatures>
        [[nodiscard]] auto connect(Receiver receiver) const
            noexcept(std::is_nothrow_move_constructible_v<Receiver>) -> ScheduleOperation<Receiver>
        {
            return {*scheduler_.context_, scheduler_.producer_, scheduler_.mode_, std::move(receiver)};
        }

    private:
        Scheduler scheduler_;
    };

    inline auto ManualContext::get_scheduler(ScheduleMode mode) noexcept -> Scheduler
    {
        return Scheduler{*this, nullptr, mode};
    }

    inline auto ManualContext::Producer::get_scheduler(ScheduleMode mode) noexcept -> Scheduler
    {
        return Scheduler{context_, &token_, mode};
    }

    inline auto ManualContext::Scheduler::schedule() const noexcept -> ScheduleSender
    {
        return ScheduleSender{*this};
    }

    static_assert(stdexec::scheduler<ManualContext::Scheduler>);
    static_assert(std::is_trivial_v<ManualContextConfig>);
    static_assert(std::is_standard_layout_v<ManualContextConfig>);
    static_assert(std::is_trivially_copyable_v<ManualContextConfig>);
}
