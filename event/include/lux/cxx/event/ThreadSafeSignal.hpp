#pragma once

#include <shared_mutex>
#include <memory>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <thread>
#include "EventTraits.hpp"
#include "Subscription.hpp"
#include "SlotCallback.hpp"
#include "Connection.hpp"

namespace lux::cxx::event
{
    // ── ThreadSafeSignal<E> ──────────────────────────────────────
    // Thread-safe signal using Copy-on-Write snapshot dispatch.
    //
    // - emit(): acquires a shared_ptr snapshot under a brief shared lock,
    //   then dispatches entirely lock-free. Multiple threads can emit concurrently.
    // - subscribe()/unsubscribe(): exclusive lock + COW of the snapshot.
    //
    // Safe against deadlocks: callbacks are never invoked while holding any lock.
    template <Event E>
    class ThreadSafeSignal
    {
    public:
        using Callback = void (*)(void *context, const E &event);

        ThreadSafeSignal()
            : snapshot_(std::make_shared<Snapshot>()) {}

        ~ThreadSafeSignal() = default;

        ThreadSafeSignal(const ThreadSafeSignal &) = delete;
        ThreadSafeSignal &operator=(const ThreadSafeSignal &) = delete;
        ThreadSafeSignal(ThreadSafeSignal &&) = delete;
        ThreadSafeSignal &operator=(ThreadSafeSignal &&) = delete;

        // ── Emit (snapshot + lock-free dispatch) ─────────────

        void emit(const E &event)
        {
            // Brief shared lock to grab the snapshot pointer
            std::shared_ptr<const Snapshot> snap;
            {
                std::shared_lock lock(mutex_);
                snap = snapshot_;
            }
            // Dispatch entirely outside any lock
            for (auto &entry : snap->entries)
            {
                entry->callback(event);
            }
        }

        // ── Subscribe (exclusive lock + COW) ─────────────────

        [[nodiscard]] ScopedSubscription subscribe(void *context, Callback cb, int16_t priority = 0)
        {
            return subscribe(SlotCallback<E>{context, cb}, priority);
        }

        [[nodiscard]] ScopedSubscription subscribe(SlotCallback<E> cb, int16_t priority = 0)
        {
            std::unique_lock lock(mutex_);
            auto id = next_id_++;
            auto entry = std::make_shared<Entry>(Entry{std::move(cb), priority, id});

            auto new_snap = cow_clone();
            new_snap->entries.push_back(std::move(entry));
            resort(new_snap->entries);
            snapshot_ = std::move(new_snap);

            return make_subscription(id);
        }

        template <auto MemFn, typename T>
            requires std::is_member_function_pointer_v<decltype(MemFn)>
                  && std::is_invocable_v<decltype(MemFn), T *, const E &>
        [[nodiscard]] ScopedSubscription subscribe(T *obj, int16_t priority = 0)
        {
            return subscribe(
                SlotCallback<E>{
                    static_cast<void *>(obj),
                    +[](void *ctx, const E &event)
                    {
                        (static_cast<T *>(ctx)->*MemFn)(event);
                    }
                },
                priority
            );
        }

        // ── Connect (returns ScopedConnection) ───────────────

        [[nodiscard]] ScopedConnection connect(SlotCallback<E> cb, int16_t priority = 0)
        {
            return ScopedConnection{Connection{Connection::Token{}, subscribe(std::move(cb), priority)}};
        }

        template <auto MemFn, typename T>
            requires std::is_member_function_pointer_v<decltype(MemFn)>
                  && std::is_invocable_v<decltype(MemFn), T *, const E &>
        [[nodiscard]] ScopedConnection connect(T *obj, int16_t priority = 0)
        {
            return ScopedConnection{Connection{Connection::Token{}, subscribe<MemFn>(obj, priority)}};
        }

        // ── Queries ──────────────────────────────────────────

        [[nodiscard]] std::size_t subscriber_count() const noexcept
        {
            std::shared_lock lock(mutex_);
            return snapshot_->entries.size();
        }

        [[nodiscard]] bool empty() const noexcept
        {
            std::shared_lock lock(mutex_);
            return snapshot_->entries.empty();
        }

    private:
        // SlotCallback is move-only, so we wrap each entry in shared_ptr
        // to allow cheap snapshot copies (COW).
        struct Entry
        {
            SlotCallback<E> callback;
            int16_t priority;
            uint64_t id;
        };

        struct Snapshot
        {
            std::vector<std::shared_ptr<const Entry>> entries;
        };

        mutable std::shared_mutex mutex_;
        std::shared_ptr<const Snapshot> snapshot_;
        uint64_t next_id_ = 0;

        // COW: shallow-copy the entry pointer vector (entries themselves are immutable)
        std::shared_ptr<Snapshot> cow_clone() const
        {
            auto clone = std::make_shared<Snapshot>();
            clone->entries = snapshot_->entries; // copies shared_ptrs, not entries
            return clone;
        }

        static void resort(std::vector<std::shared_ptr<const Entry>> &entries)
        {
            std::stable_sort(entries.begin(), entries.end(),
                [](const std::shared_ptr<const Entry> &a, const std::shared_ptr<const Entry> &b)
                {
                    return a->priority > b->priority;
                }
            );
        }

        ScopedSubscription make_subscription(uint64_t id)
        {
            // Pack the full 64-bit id across the handle's two 32-bit fields
            // (index = low, gen = high). Stuffing it into `index` alone truncated
            // it, so after 2^32 subscriptions the id collided with a live one and
            // unsubscribe removed the wrong entry (or leaked).
            return ScopedSubscription{
                ScopedSubscription::Token{},
                +[](void *owner, SubscriptionHandle h)
                {
                    const uint64_t full =
                        (static_cast<uint64_t>(h.gen) << 32) |
                        static_cast<uint64_t>(h.index);
                    static_cast<ThreadSafeSignal *>(owner)->unsubscribe_by_id(full);
                },
                this,
                SubscriptionHandle{
                    static_cast<uint32_t>(id & 0xFFFFFFFFull),
                    static_cast<uint32_t>(id >> 32)
                }
            };
        }

        void unsubscribe_by_id(uint64_t id)
        {
            std::shared_ptr<const Snapshot> old;
            {
                std::unique_lock lock(mutex_);
                auto new_snap = std::make_shared<Snapshot>();
                new_snap->entries.reserve(snapshot_->entries.size());
                for (auto &e : snapshot_->entries)
                {
                    if (e->id != id)
                        new_snap->entries.push_back(e);
                }
                old = std::move(snapshot_);     // hold the old snapshot to observe quiescence
                snapshot_ = std::move(new_snap);
            }

            // Quiescence: an emit that grabbed the old snapshot before the swap may
            // still be invoking the just-removed callback. Wait until no in-flight
            // emit references the old snapshot (use_count drops to 1 = only `old`),
            // so that once unsubscribe returns it is safe to destroy the object the
            // callback captured. New emits already see the new snapshot, so the
            // count only decreases — no risk of waiting forever.
            while (old.use_count() > 1)
                std::this_thread::yield();
        }
    };
} // namespace lux::cxx::event
