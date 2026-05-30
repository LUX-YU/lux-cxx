#pragma once
#include <cstddef>
#include <functional>
#include <vector>

#include "Common.hpp"

namespace lux::cxx::archtype {

    class Registry; // forward decl — circular includes between Signal and Registry

    /**
     * @class Signal
     * @brief A lightweight broadcaster for component lifecycle events.
     *
     * Each Registry holds three Signals per component type — on_construct,
     * on_update, on_destroy — and fires them from emplace/erase/destroy/create.
     *
     * publish() short-circuits when the listener list is empty, so a Registry
     * with no listeners adds only a `if (callbacks_.empty()) return;` per op
     * in the hot path.
     *
     * Listeners may safely read the entity's components but should avoid
     * mutating the registry directly inside the callback (that may invalidate
     * the in-progress op). Defer such mutations through CommandBuffer.
     */
    class Signal {
    public:
        using Callback     = std::function<void(Registry&, Entity)>;
        using ConnectionId = std::size_t;

        ConnectionId connect(Callback cb) {
            const ConnectionId id = next_id_++;
            callbacks_.push_back({ id, std::move(cb) });
            return id;
        }

        bool disconnect(ConnectionId id) {
            for (auto it = callbacks_.begin(); it != callbacks_.end(); ++it) {
                if (it->id == id) { callbacks_.erase(it); return true; }
            }
            return false;
        }

        void disconnect_all() noexcept { callbacks_.clear(); }

        bool empty()  const noexcept { return callbacks_.empty(); }
        std::size_t size() const noexcept { return callbacks_.size(); }

        /// Invokes every connected callback. Hot path: returns immediately
        /// when there are no listeners.
        void publish(Registry& reg, Entity e) const {
            if (callbacks_.empty()) return;
            // Iterate by index in case a callback connects/disconnects mid-publish.
            for (std::size_t i = 0, n = callbacks_.size(); i < n; ++i) {
                callbacks_[i].cb(reg, e);
            }
        }

    private:
        struct Slot {
            ConnectionId id;
            Callback     cb;
        };
        std::vector<Slot> callbacks_;
        ConnectionId      next_id_ = 1;
    };

    /// One signal per lifecycle hook per component type.
    struct SignalSet {
        Signal on_construct;
        Signal on_update;
        Signal on_destroy;
    };

} // namespace lux::cxx::archtype
