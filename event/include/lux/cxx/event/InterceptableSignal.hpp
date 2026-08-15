#pragma once

#include <vector>
#include <algorithm>
#include <cstdint>
#include <lux/cxx/container/SlotMap.hpp>
#include <lux/cxx/container/SmallVector.hpp>
#include "EventTraits.hpp"
#include "EventResult.hpp"
#include "Subscription.hpp"
#include "Connection.hpp"

namespace lux::cxx::event
{
    // ── InterceptableSlotCallback<E> ─────────────────────────────
    // SBO type-erased callback that returns EventResult.
    // Mirrors SlotCallback<E> but with EventResult return type.

    template <typename E>
    class InterceptableSlotCallback
    {
    public:
        static constexpr std::size_t kSBOSize  = 48;
        static constexpr std::size_t kSBOAlign = alignof(std::max_align_t);

        struct Ops
        {
            EventResult (*invoke)(const void *storage, const E &event);
            void (*move_construct)(void *dst, void *src);
            void (*destroy)(void *storage);
        };

        // ── Default (empty) ──────────────────────────────────

        InterceptableSlotCallback() noexcept : ops_(nullptr) {}

        // ── From function pointer + context ──────────────────

        using RawFn = EventResult (*)(void *, const E &);

        InterceptableSlotCallback(void *ctx, RawFn fn) noexcept
            : ops_(&kRawFnOps)
        {
            auto &s = raw_slot();
            s.ctx = ctx;
            s.fn  = fn;
        }

        // ── From callable (lambda / functor) ─────────────────

        template <typename F>
            requires(!std::same_as<std::decay_t<F>, InterceptableSlotCallback>)
                 && std::invocable<F, const E &>
                 && std::convertible_to<std::invoke_result_t<F, const E &>, EventResult>
        InterceptableSlotCallback(F &&f)
        {
            using Fn = std::decay_t<F>;

            if constexpr (sizeof(Fn) <= kSBOSize && alignof(Fn) <= kSBOAlign
                          && std::is_nothrow_move_constructible_v<Fn>)
            {
                // Inline (SBO) path
                static constexpr Ops ops = {
                    // invoke
                    +[](const void *storage, const E &event) -> EventResult
                    {
                        return (*reinterpret_cast<const Fn *>(storage))(event);
                    },
                    // move_construct
                    +[](void *dst, void *src)
                    {
                        ::new (dst) Fn(std::move(*reinterpret_cast<Fn *>(src)));
                    },
                    // destroy
                    +[](void *storage)
                    {
                        reinterpret_cast<Fn *>(storage)->~Fn();
                    }
                };
                ops_ = &ops;
                ::new (storage_) Fn(std::forward<F>(f));
            }
            else
            {
                // Heap path
                static constexpr Ops ops = {
                    +[](const void *storage, const E &event) -> EventResult
                    {
                        auto *ptr = *reinterpret_cast<Fn *const *>(storage);
                        return (*ptr)(event);
                    },
                    +[](void *dst, void *src)
                    {
                        auto &src_ptr = *reinterpret_cast<Fn **>(src);
                        *reinterpret_cast<Fn **>(dst) = src_ptr;
                        src_ptr = nullptr;
                    },
                    +[](void *storage)
                    {
                        delete *reinterpret_cast<Fn **>(storage);
                    }
                };
                ops_ = &ops;
                *reinterpret_cast<Fn **>(storage_) = new Fn(std::forward<F>(f));
            }
        }

        // ── Move-only ────────────────────────────────────────

        InterceptableSlotCallback(InterceptableSlotCallback &&o) noexcept
            : ops_(o.ops_)
        {
            if (ops_)
            {
                ops_->move_construct(storage_, o.storage_);
                o.ops_ = nullptr;
            }
        }

        InterceptableSlotCallback &operator=(InterceptableSlotCallback &&o) noexcept
        {
            if (this != &o)
            {
                destroy();
                ops_ = o.ops_;
                if (ops_)
                {
                    ops_->move_construct(storage_, o.storage_);
                    o.ops_ = nullptr;
                }
            }
            return *this;
        }

        InterceptableSlotCallback(const InterceptableSlotCallback &) = delete;
        InterceptableSlotCallback &operator=(const InterceptableSlotCallback &) = delete;

        ~InterceptableSlotCallback() { destroy(); }

        // ── Invoke ───────────────────────────────────────────

        EventResult operator()(const E &event) const
        {
            return ops_->invoke(storage_, event);
        }

        explicit operator bool() const noexcept { return ops_ != nullptr; }

    private:
        const Ops *ops_ = nullptr;
        alignas(kSBOAlign) mutable char storage_[kSBOSize]{};

        void destroy()
        {
            if (ops_)
            {
                ops_->destroy(storage_);
                ops_ = nullptr;
            }
        }

        // Raw function pointer storage
        struct RawSlot
        {
            void *ctx;
            RawFn  fn;
        };
        static_assert(sizeof(RawSlot) <= kSBOSize);

        RawSlot       &raw_slot()       { return *reinterpret_cast<RawSlot *>(storage_); }
        const RawSlot &raw_slot() const { return *reinterpret_cast<const RawSlot *>(storage_); }

        static constexpr Ops kRawFnOps = {
            +[](const void *storage, const E &event) -> EventResult
            {
                auto &s = *reinterpret_cast<const RawSlot *>(storage);
                return s.fn(s.ctx, event);
            },
            +[](void *dst, void *src)
            {
                auto &d = *reinterpret_cast<RawSlot *>(dst);
                auto &s = *reinterpret_cast<RawSlot *>(src);
                d = s;
                s.fn = nullptr;
            },
            +[](void *) {}
        };
    };

    // ── InterceptableSignal<E> ───────────────────────────────────
    // Layer 1 signal with early-exit semantics.
    //
    // Subscribers are invoked in priority order (highest first).
    // If any subscriber returns EventResult::Handled, dispatch stops immediately.
    //
    // Usage:
    //   InterceptableSignal<InputEvent> on_input;
    //   auto sub = on_input.subscribe([](const InputEvent& e) -> EventResult {
    //       if (ui_wants_it(e)) return EventResult::Handled;
    //       return EventResult::Continue;
    //   });
    //   EventResult result = on_input.emit(input_event);

    template <Event E>
    class InterceptableSignal
    {
    public:
        using Callback = EventResult (*)(void *context, const E &event);

        InterceptableSignal() = default;
        ~InterceptableSignal() = default;

        InterceptableSignal(const InterceptableSignal &) = delete;
        InterceptableSignal &operator=(const InterceptableSignal &) = delete;
        InterceptableSignal(InterceptableSignal &&) noexcept = default;
        InterceptableSignal &operator=(InterceptableSignal &&) noexcept = default;

        // ── Subscribe (function pointer + context) ───────────

        [[nodiscard]] ScopedSubscription subscribe(void *context, Callback cb, int16_t priority = 0)
        {
            auto key = slots_.insert(Subscriber{InterceptableSlotCallback<E>{context, cb}, priority});

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
                    static_cast<InterceptableSignal *>(owner)->unsubscribe(h);
                },
                this, key};
        }

        // ── Subscribe (member function, compile-time bridge) ─

        template <auto MemFn, typename T>
            requires std::is_member_function_pointer_v<decltype(MemFn)>
                  && std::is_invocable_r_v<EventResult, decltype(MemFn), T *, const E &>
        [[nodiscard]] ScopedSubscription subscribe(T *obj, int16_t priority = 0)
        {
            return subscribe(
                static_cast<void *>(obj),
                +[](void *ctx, const E &event) -> EventResult
                {
                    return (static_cast<T *>(ctx)->*MemFn)(event);
                },
                priority);
        }

        // ── Subscribe (callable / lambda) ────────────────────

        [[nodiscard]] ScopedSubscription subscribe(InterceptableSlotCallback<E> cb, int16_t priority = 0)
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
                    static_cast<InterceptableSignal *>(owner)->unsubscribe(h);
                },
                this, key};
        }

        // ── Connect (returns ScopedConnection) ───────────────

        [[nodiscard]] ScopedConnection connect(InterceptableSlotCallback<E> cb, int16_t priority = 0)
        {
            return ScopedConnection{Connection{Connection::Token{}, subscribe(std::move(cb), priority)}};
        }

        template <auto MemFn, typename T>
            requires std::is_member_function_pointer_v<decltype(MemFn)>
                  && std::is_invocable_r_v<EventResult, decltype(MemFn), T *, const E &>
        [[nodiscard]] ScopedConnection connect(T *obj, int16_t priority = 0)
        {
            return ScopedConnection{Connection{Connection::Token{}, subscribe<MemFn>(obj, priority)}};
        }

        // ── Emit ─────────────────────────────────────────────
        // Invokes subscribers in priority order (highest first).
        // Returns Handled if any subscriber consumed the event.

        EventResult emit(const E &event)
        {
            // Rebuild only at the outermost emit; a nested emit must not mutate
            // sorted_keys_ while an outer emit iterates it (stale-size OOB). See
            // the matching note in Signal::emit.
            if (emit_depth_ == 0 && sorted_dirty_)
                rebuild_sorted();

            ++emit_depth_;

            EventResult result = EventResult::Continue;
            for (std::size_t i = 0, n = sorted_keys_.size(); i < n; ++i)
            {
                if (auto *sub = slots_.find(sorted_keys_[i]))
                {
                    if (sub->callback(event) == EventResult::Handled)
                    {
                        result = EventResult::Handled;
                        break;
                    }
                }
            }

            --emit_depth_;

            if (emit_depth_ == 0)
                apply_pending();

            return result;
        }

        // ── Queries ──────────────────────────────────────────

        [[nodiscard]] std::size_t subscriber_count() const noexcept { return slots_.size(); }
        [[nodiscard]] bool empty() const noexcept { return slots_.empty(); }

    private:
        struct Subscriber
        {
            InterceptableSlotCallback<E> callback;
            int16_t priority;
        };

        lux::cxx::SlotMap<Subscriber, SubscriptionTag> slots_;
        std::vector<SubscriptionHandle> sorted_keys_;
        bool sorted_dirty_ = false;
        uint32_t emit_depth_ = 0;
        lux::cxx::SmallVector<SubscriptionHandle, 4> pending_key_adds_;

        void unsubscribe(SubscriptionHandle h)
        {
            slots_.erase(h);

            if (emit_depth_ == 0)
            {
                auto it = std::find(sorted_keys_.begin(), sorted_keys_.end(), h);
                if (it != sorted_keys_.end())
                    sorted_keys_.erase(it);
            }
            else
            {
                sorted_dirty_ = true;
            }
        }

        void apply_pending()
        {
            bool changed = !pending_key_adds_.empty() || sorted_dirty_;

            for (auto key : pending_key_adds_)
            {
                if (slots_.isValid(key))
                    sorted_keys_.push_back(key);
            }
            pending_key_adds_.clear();

            if (changed)
                rebuild_sorted();
        }

        void rebuild_sorted()
        {
            std::erase_if(
                sorted_keys_,
                [this](SubscriptionHandle key)
                {
                    return !slots_.isValid(key);
                });

            std::stable_sort(sorted_keys_.begin(), sorted_keys_.end(),
                [this](SubscriptionHandle a, SubscriptionHandle b)
                {
                    return slots_[a].priority > slots_[b].priority;
                });

            sorted_dirty_ = false;
        }
    };
} // namespace lux::cxx::event
