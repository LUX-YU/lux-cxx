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
            // Appending during publish is safe: the index loop only visits slots
            // [0, n) captured before this push, and re-reads the (possibly
            // reallocated) buffer each iteration.
            callbacks_.push_back({ id, std::move(cb), true });
            return id;
        }

        bool disconnect(ConnectionId id) {
            for (auto it = callbacks_.begin(); it != callbacks_.end(); ++it) {
                if (it->id == id && it->alive) {
                    if (publishing_ > 0) {
                        // Erasing mid-publish would shift the vector under the
                        // index loop (the cached size becomes stale -> OOB read).
                        // Tombstone now; compact when the outermost publish unwinds.
                        it->alive = false;
                        has_dead_ = true;
                    } else {
                        callbacks_.erase(it);
                    }
                    return true;
                }
            }
            return false;
        }

        void disconnect_all() noexcept {
            if (publishing_ > 0) {
                for (auto& s : callbacks_) s.alive = false;
                has_dead_ = true;
            } else {
                callbacks_.clear();
            }
        }

        bool empty()  const noexcept { return callbacks_.empty(); }
        std::size_t size() const noexcept { return callbacks_.size(); }

        /// Invokes every connected callback. Hot path: returns immediately
        /// when there are no listeners. Safe to connect/disconnect (including
        /// self-disconnect) and to publish re-entrantly from within a callback.
        void publish(Registry& reg, Entity e) const {
            if (callbacks_.empty()) return;
            ++publishing_;
            for (std::size_t i = 0, n = callbacks_.size(); i < n; ++i) {
                const Slot& s = callbacks_[i];
                if (s.alive) s.cb(reg, e);
            }
            if (--publishing_ == 0 && has_dead_) {
                std::erase_if(callbacks_, [](const Slot& s) { return !s.alive; });
                has_dead_ = false;
            }
        }

    private:
        struct Slot {
            ConnectionId id;
            Callback     cb;
            bool         alive = true;
        };
        // Mutable: publish() is const (it does not change the logical listener set)
        // but defers compaction of tombstoned slots until the outermost publish ends.
        mutable std::vector<Slot> callbacks_;
        mutable int               publishing_ = 0;
        mutable bool              has_dead_   = false;
        ConnectionId              next_id_    = 1;
    };

    /// One signal per lifecycle hook per component type.
    struct SignalSet {
        Signal on_construct;
        Signal on_update;
        Signal on_destroy;
    };

} // namespace lux::cxx::archtype
