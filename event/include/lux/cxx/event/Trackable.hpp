#pragma once

#include <mutex>
#include "Connection.hpp"

namespace lux::cxx::event
{
    // ── Trackable ────────────────────────────────────────────────
    // Mix-in base class for objects that want automatic disconnection
    // when destroyed. Any ScopedConnection tracked by this object
    // will be disconnected in ~Trackable().
    //
    // Thread-safety: this object's own mutex makes track()/disconnect_all()/move
    // safe to call concurrently on the SAME Trackable. It does NOT make the
    // disconnect itself safe against a concurrent emit() of the connected signal.
    // @warning Auto-disconnect is only thread-safe with a thread-safe signal
    //          (ThreadSafeSignal<E>, whose unsubscribe waits for in-flight emits).
    //          Destroying / disconnecting a Trackable bound to a plain Signal<E>
    //          concurrently with that signal's emit() is undefined behaviour — plain
    //          Signal is single-threaded by design. Pair cross-thread Trackables
    //          with ThreadSafeSignal.
    //
    // Usage:
    //   class UIWidget : public lux::cxx::event::Trackable
    //   {
    //   public:
    //       void bind(Signal<ClickEvent>& sig)
    //       {
    //           track(connect(sig, [this](const ClickEvent& e) { onClick(e); }));
    //       }
    //       // ~UIWidget() automatically disconnects all tracked connections
    //   };

    class Trackable
    {
    public:
        Trackable() = default;

        virtual ~Trackable()
        {
            disconnect_all();
        }

        // Move: transfer tracked connections
        Trackable(Trackable &&o) noexcept
        {
            std::scoped_lock lock(o.mutex_);
            connections_ = std::move(o.connections_);
        }

        Trackable &operator=(Trackable &&o) noexcept
        {
            if (this != &o)
            {
                disconnect_all();
                std::scoped_lock lock(o.mutex_);
                connections_ = std::move(o.connections_);
            }
            return *this;
        }

        // Non-copyable (connections are not copyable)
        Trackable(const Trackable &) = delete;
        Trackable &operator=(const Trackable &) = delete;

    protected:
        // Track a connection. Will be auto-disconnected on destruction.
        void track(ScopedConnection conn)
        {
            std::lock_guard lock(mutex_);
            connections_.add(std::move(conn));
        }

        // Manually disconnect all tracked connections.
        void disconnect_all()
        {
            std::lock_guard lock(mutex_);
            connections_.disconnect_all();
        }

        // Number of tracked connections.
        [[nodiscard]] std::size_t tracked_count() const
        {
            std::lock_guard lock(mutex_);
            return connections_.size();
        }

    private:
        mutable std::mutex mutex_;
        ConnectionGroup connections_;
    };
} // namespace lux::cxx::event
