#pragma once

#include <atomic>
#include <memory>
#include <vector>
#include "Subscription.hpp"

namespace lux::cxx::event
{

    // ── Connection ───────────────────────────────────────────────
    // Lightweight, copyable handle to a signal-slot connection.
    // Does NOT auto-disconnect on destruction. Use ScopedConnection for RAII.
    //
    // NOTE: copies are ALIASES, not independent connections. All copies share one
    // underlying subscription (a shared_ptr<Impl>); disconnect()ing any copy — or
    // the ScopedConnection that owns it — disconnects them all. To get N independent
    // connections, call connect() N times.

    class Connection
    {
    public:
        Connection() = default;

        // Passkey token — restricts construction to code that can name this type.
        struct Token {};

        struct Impl
        {
            std::atomic<bool> connected{true};
            ScopedSubscription sub;

            void disconnect()
            {
                if (connected.exchange(false, std::memory_order_acq_rel))
                {
                    sub.reset();
                }
            }
        };

        // Construct from a ScopedSubscription (via passkey)
        Connection(Token, ScopedSubscription sub)
            : impl_(std::make_shared<Impl>())
        {
            impl_->sub = std::move(sub);
        }

        void disconnect()
        {
            if (auto p = impl_)
                p->disconnect();
        }

        [[nodiscard]] bool connected() const noexcept
        {
            return impl_ && impl_->connected.load(std::memory_order_acquire);
        }

        explicit operator bool() const noexcept { return connected(); }

    private:
        std::shared_ptr<Impl> impl_;
    };

    // ── ScopedConnection ─────────────────────────────────────────
    // RAII wrapper: auto-disconnects on destruction. Move-only.

    class ScopedConnection
    {
    public:
        ScopedConnection() = default;

        explicit ScopedConnection(Connection conn)
            : conn_(std::move(conn)) {}

        ~ScopedConnection() { disconnect(); }

        // Move-only
        ScopedConnection(ScopedConnection &&o) noexcept = default;
        ScopedConnection &operator=(ScopedConnection &&o) noexcept
        {
            if (this != &o)
            {
                disconnect();
                conn_ = std::move(o.conn_);
            }
            return *this;
        }

        ScopedConnection(const ScopedConnection &) = delete;
        ScopedConnection &operator=(const ScopedConnection &) = delete;

        void disconnect()
        {
            conn_.disconnect();
        }

        [[nodiscard]] bool connected() const noexcept { return conn_.connected(); }
        explicit operator bool() const noexcept { return connected(); }
        [[nodiscard]] const Connection &connection() const noexcept { return conn_; }

    private:
        Connection conn_;
    };

    // ── ConnectionGroup ──────────────────────────────────────────
    // Holds multiple ScopedConnections. Disconnects all on destruction.

    class ConnectionGroup
    {
    public:
        ScopedConnection &add(ScopedConnection &&conn)
        {
            return conns_.emplace_back(std::move(conn));
        }

        void disconnect_all()
        {
            conns_.clear();
        }

        void clear() { disconnect_all(); }
        [[nodiscard]] std::size_t size() const { return conns_.size(); }
        [[nodiscard]] bool empty() const { return conns_.empty(); }

    private:
        std::vector<ScopedConnection> conns_;
    };

} // namespace lux::cxx::event
