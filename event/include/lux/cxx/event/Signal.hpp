#pragma once

#include <vector>
#include <algorithm>
#include <cstdint>
#include <lux/cxx/container/SlotMap.hpp>
#include <lux/cxx/container/SmallVector.hpp>
#include "EventTraits.hpp"
#include "Subscription.hpp"
#include "SlotCallback.hpp"
#include "Connection.hpp"

namespace lux::cxx::event
{
    // ── Member-function thunk ────────────────────────────────────
    // Compile-time bridge: converts (void*, const E&) into a member-function call.
    namespace detail
    {

        template <auto MemFn, typename T, typename E>
        void member_thunk(void *ctx, const E &event)
        {
            (static_cast<T *>(ctx)->*MemFn)(event);
        }

    } // namespace detail

    // ── Signal<E> ────────────────────────────────────────────────
    // Layer 1: synchronous, single-thread, zero-allocation signal.
    //
    // Usage:
    //   Signal<HealthChanged> on_health_changed;
    //   auto sub = on_health_changed.subscribe<&MyClass::handler>(this);
    //   on_health_changed.emit({entity, 100.f, 80.f});
    //   // sub goes out of scope → automatic unsubscribe
    template <Event E>
    class Signal
    {
    public:
        using Callback = void (*)(void *context, const E &event);

        Signal() = default;
        ~Signal() = default;

        Signal(const Signal &) = delete;
        Signal &operator=(const Signal &) = delete;
        Signal(Signal &&) noexcept = default;
        Signal &operator=(Signal &&) noexcept = default;

        // ── Subscribe (function pointer + context) ───────────
        [[nodiscard]] ScopedSubscription subscribe(void *context, Callback cb, int16_t priority = 0)
        {
            auto key = slots_.insert(Subscriber{SlotCallback<E>{context, cb}, priority});

            if (emit_depth_ > 0)
            {
                pending_key_adds_.push_back(key);
            }
            else
            {
                sorted_keys_.push_back(key);
                sorted_dirty_ = true;
            }

            return ScopedSubscription{
                ScopedSubscription::Token{},
                +[](void *owner, SubscriptionHandle h)
                {
                    static_cast<Signal *>(owner)->unsubscribe(h);
                },
                this, key
            };
        }

        // ── Subscribe (member function, compile-time bridge) ─

        template <auto MemFn, typename T>
            requires std::is_member_function_pointer_v<decltype(MemFn)> && std::is_invocable_v<decltype(MemFn), T *, const E &>
        [[nodiscard]] ScopedSubscription subscribe(T *obj, int16_t priority = 0)
        {
            return subscribe(
                static_cast<void *>(obj),
                &detail::member_thunk<MemFn, T, E>,
                priority
            );
        }

        // ── Subscribe (SlotCallback — supports lambdas) ──────
        [[nodiscard]] ScopedSubscription subscribe(SlotCallback<E> cb, int16_t priority = 0)
        {
            auto key = slots_.insert(Subscriber{std::move(cb), priority});

            if (emit_depth_ > 0)
            {
                pending_key_adds_.push_back(key);
            }
            else
            {
                sorted_keys_.push_back(key);
                sorted_dirty_ = true;
            }

            return ScopedSubscription{
                ScopedSubscription::Token{},
                +[](void *owner, SubscriptionHandle h)
                {
                    static_cast<Signal *>(owner)->unsubscribe(h);
                },
                this, key
            };
        }

        // ── Connect (returns Connection for high-level API) ──
        [[nodiscard]] ScopedConnection connect(SlotCallback<E> cb, int16_t priority = 0)
        {
            return ScopedConnection{Connection{Connection::Token{}, subscribe(std::move(cb), priority)}};
        }

        template <auto MemFn, typename T>
            requires std::is_member_function_pointer_v<decltype(MemFn)> && std::is_invocable_v<decltype(MemFn), T *, const E &>
        [[nodiscard]] ScopedConnection connect(T *obj, int16_t priority = 0)
        {
            return ScopedConnection{Connection{Connection::Token{}, subscribe<MemFn>(obj, priority)}};
        }

        // ── Emit ─────────────────────────────────────────────
        // Synchronously invokes all subscribers in priority order.
        // Safe to call recursively (reentrant).

        void emit(const E &event)
        {
            if (sorted_dirty_)
                rebuild_sorted();

            ++emit_depth_;

            // Use index-based loop: sorted_keys_ is not modified during iteration
            // (adds go to pending_key_adds_, removes only erase from slots_).
            for (std::size_t i = 0, n = sorted_keys_.size(); i < n; ++i)
            {
                if (auto *sub = slots_.find(sorted_keys_[i]))
                {
                    sub->callback(event);
                }
                // find() returns nullptr for erased keys → skip silently
            }

            --emit_depth_;

            if (emit_depth_ == 0)
                apply_pending();
        }

        // ── Queries ──────────────────────────────────────────

        [[nodiscard]] std::size_t subscriber_count() const noexcept { return slots_.size(); }
        [[nodiscard]] bool empty() const noexcept { return slots_.empty(); }

    private:
        struct Subscriber
        {
            SlotCallback<E> callback;
            int16_t         priority;
        };

        // SlotMap: O(1) insert / erase / find, generational safety
        lux::cxx::SlotMap<Subscriber, SubscriptionTag> slots_;

        // Priority-sorted key list for deterministic iteration order
        std::vector<SubscriptionHandle> sorted_keys_;
        bool sorted_dirty_ = false;

        // Reentry guard
        uint32_t emit_depth_ = 0;

        // Keys added during emit (deferred until outermost emit completes)
        lux::cxx::SmallVector<SubscriptionHandle, 4> pending_key_adds_;

        // ── Private methods ──────────────────────────────────

        void unsubscribe(SubscriptionHandle h)
        {
            slots_.erase(h);

            if (emit_depth_ == 0)
            {
                // Safe to modify sorted_keys_ directly
                auto it = std::find(sorted_keys_.begin(), sorted_keys_.end(), h);
                if (it != sorted_keys_.end())
                    sorted_keys_.erase(it);
            }
            else
            {
                // During emit: slots_.find(h) will return nullptr for the rest
                // of the iteration. Clean up sorted_keys_ after emit completes.
                sorted_dirty_ = true;
            }
        }

        void apply_pending()
        {
            bool changed = !pending_key_adds_.empty() || sorted_dirty_;

            for (auto key : pending_key_adds_)
            {
                if (slots_.is_valid(key))
                    sorted_keys_.push_back(key);
            }
            pending_key_adds_.clear();

            if (changed)
                rebuild_sorted();
        }

        void rebuild_sorted()
        {
            // Remove stale keys (unsubscribed during emit)
            std::erase_if(
                sorted_keys_, 
                [this](SubscriptionHandle key)
                { 
                    return !slots_.is_valid(key); 
                }
            );

            // Stable sort by descending priority (higher priority first)
            std::stable_sort(sorted_keys_.begin(), sorted_keys_.end(),
                [this](SubscriptionHandle a, SubscriptionHandle b)
                {
                    return slots_[a].priority > slots_[b].priority;
                }
            );

            sorted_dirty_ = false;
        }
    };
} // namespace lux::cxx::event
