#pragma once

#include <vector>
#include <algorithm>
#include "Signal.hpp"

namespace lux::cxx::event
{
    // ── FilteredSignal<E> ────────────────────────────────────────
    // Wraps a Signal<E> with a pre-dispatch filter chain.
    // Filters are invoked in order before the event reaches subscribers.
    // If any filter returns false, the event is silently dropped.
    //
    // Usage:
    //   FilteredSignal<MotorCommand> on_motor_cmd;
    //   on_motor_cmd.add_filter([](const MotorCommand& c) {
    //       return c.torque <= MAX_TORQUE;  // safety limit
    //   });
    //   auto sub = on_motor_cmd.subscribe(...);
    //   on_motor_cmd.emit(cmd);  // dropped if filter rejects

    template <Event E>
    class FilteredSignal
    {
    public:
        using FilterFn = bool (*)(const E &);

        FilteredSignal() = default;
        ~FilteredSignal() = default;

        FilteredSignal(const FilteredSignal &) = delete;
        FilteredSignal &operator=(const FilteredSignal &) = delete;
        FilteredSignal(FilteredSignal &&) noexcept = default;
        FilteredSignal &operator=(FilteredSignal &&) noexcept = default;

        // ── Filter management ────────────────────────────────

        void add_filter(FilterFn fn)
        {
            filters_.push_back(fn);
        }

        bool remove_filter(FilterFn fn)
        {
            auto it = std::find(filters_.begin(), filters_.end(), fn);
            if (it != filters_.end())
            {
                filters_.erase(it);
                return true;
            }
            return false;
        }

        void clear_filters()
        {
            filters_.clear();
        }

        [[nodiscard]] std::size_t filter_count() const noexcept
        {
            return filters_.size();
        }

        // ── Emit (filters first, then dispatch) ──────────────
        // Returns true if the event passed all filters and was dispatched.

        bool emit(const E &event)
        {
            for (auto &filter : filters_)
            {
                if (!filter(event))
                    return false;
            }
            signal_.emit(event);
            return true;
        }

        // ── Subscribe / Connect — delegate to inner signal ───

        [[nodiscard]] ScopedSubscription subscribe(void *context,
                                                   typename Signal<E>::Callback cb,
                                                   int16_t priority = 0)
        {
            return signal_.subscribe(context, cb, priority);
        }

        [[nodiscard]] ScopedSubscription subscribe(SlotCallback<E> cb, int16_t priority = 0)
        {
            return signal_.subscribe(std::move(cb), priority);
        }

        template <auto MemFn, typename T>
            requires std::is_member_function_pointer_v<decltype(MemFn)>
                  && std::is_invocable_v<decltype(MemFn), T *, const E &>
        [[nodiscard]] ScopedSubscription subscribe(T *obj, int16_t priority = 0)
        {
            return signal_.template subscribe<MemFn>(obj, priority);
        }

        [[nodiscard]] ScopedConnection connect(SlotCallback<E> cb, int16_t priority = 0)
        {
            return signal_.connect(std::move(cb), priority);
        }

        template <auto MemFn, typename T>
            requires std::is_member_function_pointer_v<decltype(MemFn)>
                  && std::is_invocable_v<decltype(MemFn), T *, const E &>
        [[nodiscard]] ScopedConnection connect(T *obj, int16_t priority = 0)
        {
            return signal_.template connect<MemFn>(obj, priority);
        }

        // ── Queries ──────────────────────────────────────────

        [[nodiscard]] std::size_t subscriber_count() const noexcept { return signal_.subscriber_count(); }
        [[nodiscard]] bool empty() const noexcept { return signal_.empty(); }

        // Direct access to inner signal (for advanced usage)
        [[nodiscard]] Signal<E>       &signal()       noexcept { return signal_; }
        [[nodiscard]] const Signal<E> &signal() const noexcept { return signal_; }

    private:
        std::vector<FilterFn> filters_;
        Signal<E> signal_;
    };
} // namespace lux::cxx::event
