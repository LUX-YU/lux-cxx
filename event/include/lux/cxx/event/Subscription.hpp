#pragma once

#include <vector>
#include <lux/cxx/container/SlotMap.hpp>

namespace lux::cxx::event
{
    // ── Subscription Handle ──────────────────────────────────────
    // Direct reuse of SlotKey — carries index + generation for O(1) safe lookup.
    struct SubscriptionTag{};
    using SubscriptionHandle = lux::cxx::SlotKey<SubscriptionTag>;

    // ── Scoped Subscription ──────────────────────────────────────
    // RAII handle: automatically unsubscribes on destruction.
    // Move-only. Type-erased via (owner*, function-pointer, handle) triple.

    class ScopedSubscription
    {
    public:
        using UnsubFn = void (*)(void *owner, SubscriptionHandle);

        // Passkey token — restricts construction to code that can name this type.
        struct Token {};

        ScopedSubscription() = default;

        ScopedSubscription(Token, UnsubFn fn, void *owner, SubscriptionHandle h)
            : unsub_fn_(fn), owner_(owner), handle_(h) {}

        ~ScopedSubscription() { reset(); }

        // Move-only
        ScopedSubscription(ScopedSubscription &&o) noexcept
            : unsub_fn_(o.unsub_fn_), owner_(o.owner_), handle_(o.handle_)
        {
            o.unsub_fn_ = nullptr;
        }

        ScopedSubscription &operator=(ScopedSubscription &&o) noexcept
        {
            if (this != &o)
            {
                reset();
                unsub_fn_ = o.unsub_fn_;
                owner_ = o.owner_;
                handle_ = o.handle_;
                o.unsub_fn_ = nullptr;
            }
            return *this;
        }

        ScopedSubscription(const ScopedSubscription &) = delete;
        ScopedSubscription &operator=(const ScopedSubscription &) = delete;

        void reset()
        {
            if (unsub_fn_)
            {
                unsub_fn_(owner_, handle_);
                unsub_fn_ = nullptr;
            }
        }

        [[nodiscard]] bool active() const noexcept { return unsub_fn_ != nullptr; }
        explicit operator bool() const noexcept { return active(); }
        [[nodiscard]] SubscriptionHandle handle() const noexcept { return handle_; }

    private:
        UnsubFn unsub_fn_ = nullptr;
        void *owner_ = nullptr;
        SubscriptionHandle handle_{};
    };

    // ── Subscription Group ───────────────────────────────────────
    // Holds multiple ScopedSubscriptions. Clears all on destruction.

    class SubscriptionGroup
    {
    public:
        ScopedSubscription &add(ScopedSubscription &&sub)
        {
            return subs_.emplace_back(std::move(sub));
        }

        void clear() { subs_.clear(); }
        [[nodiscard]] std::size_t size() const { return subs_.size(); }
        [[nodiscard]] bool empty() const { return subs_.empty(); }

    private:
        std::vector<ScopedSubscription> subs_;
    };

} // namespace lux::cxx::event
