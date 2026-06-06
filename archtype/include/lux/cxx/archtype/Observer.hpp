#pragma once
#include <cstddef>
#include <memory>
#include <unordered_set>
#include <vector>

#include "Common.hpp"
#include "Registry.hpp"
#include "Signal.hpp"

namespace lux::cxx::archtype {

    /**
     * @class Observer
     * @brief Collects entities that triggered on_construct / on_update /
     *        on_destroy events. Useful for reactive systems: "process every
     *        entity that gained Velocity this frame," "sync to network every
     *        entity whose Transform changed," etc.
     *
     * Usage:
     * @code
     *   Observer obs;
     *   obs.observe_construct<Velocity>(reg)
     *      .observe_destroy<Health>(reg);
     *
     *   // ... game frame ...
     *
     *   obs.each([&](Entity e) {
     *       // e gained Velocity OR lost Health this frame
     *   });
     *   obs.clear();   // ready for next frame
     * @endcode
     *
     * Entities are deduplicated, so if the same entity triggers two observed
     * signals in the same frame it appears in the list once.
     *
     * Non-movable / non-copyable: the signal callbacks capture `this`. If
     * you need to store it in a container, wrap it in std::unique_ptr.
     *
     * Teardown order: an Observer holds raw Signal* into the Registry. It is
     * robust against the Registry being destroyed first — each connection carries
     * the Registry's shared liveness token and is skipped on teardown if the
     * Registry has already died (so no dangling Signal* is dereferenced).
     */
    class Observer {
    public:
        Observer() = default;
        ~Observer() { disconnect_all(); }

        Observer(const Observer&)            = delete;
        Observer& operator=(const Observer&) = delete;
        Observer(Observer&&)                 = delete;
        Observer& operator=(Observer&&)      = delete;

        template<class C> Observer& observe_construct(Registry& reg) { track(reg.on_construct<C>(), reg.aliveToken()); return *this; }
        template<class C> Observer& observe_update   (Registry& reg) { track(reg.on_update<C>(),    reg.aliveToken()); return *this; }
        template<class C> Observer& observe_destroy  (Registry& reg) { track(reg.on_destroy<C>(),   reg.aliveToken()); return *this; }

        template<class F>
        void each(F&& f) const {
            for (auto e : entities_) f(e);
        }

        void clear() noexcept {
            entities_.clear();
            in_set_.clear();
        }

        std::size_t size()  const noexcept { return entities_.size(); }
        bool        empty() const noexcept { return entities_.empty(); }
        auto        begin() const noexcept { return entities_.begin(); }
        auto        end()   const noexcept { return entities_.end(); }

        void disconnect_all() noexcept {
            for (auto& c : connections_) {
                // Skip if the owning Registry was destroyed first — its Signals
                // (and the SignalSet array) are gone, so c.sig dangles.
                if (c.registry_alive && *c.registry_alive)
                    c.sig->disconnect(c.id);
            }
            connections_.clear();
        }

    private:
        void track(Signal& sig, std::shared_ptr<bool> registry_alive) {
            const auto id = sig.connect([this](Registry&, Entity e) {
                if (in_set_.insert(e).second) entities_.push_back(e);
            });
            connections_.push_back({ &sig, id, std::move(registry_alive) });
        }

        struct Conn { Signal* sig; Signal::ConnectionId id; std::shared_ptr<bool> registry_alive; };
        std::vector<Entity>        entities_;
        std::unordered_set<Entity> in_set_;
        std::vector<Conn>          connections_;
    };

} // namespace lux::cxx::archtype
