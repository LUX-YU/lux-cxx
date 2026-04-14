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
    // Thread-safe: track() can be called from any thread.
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
