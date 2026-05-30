#pragma once
#include <cstdint>
#include <functional>
#include <tuple>
#include <utility>
#include <vector>

#include "Common.hpp"
#include "Registry.hpp"

namespace lux::cxx::archtype {

    /**
     * @class CommandBuffer
     * @brief Deferred operations against a Registry, applied in bulk at commit().
     *
     * Two layers:
     *
     * (1) Per-deferred-entity batching. `cb.create()` allocates a
     *     DeferredEntity placeholder and an empty op set. Subsequent
     *     `cb.emplace<C>(de, ...)` accumulates component constructors into
     *     that op set instead of enqueueing a separate command. At commit,
     *     the entity is placed **directly** in its final archetype with one
     *     allocation — no intermediate migrations. This makes
     *
     *         auto de = cb.create();
     *         cb.emplace<Position>(de, 1, 2);
     *         if (is_enemy) cb.emplace<Health>(de, 100);
     *         cb.emplace<Velocity>(de, ...);
     *
     *     cost the same as a single `reg.create<Position, Health, Velocity>()`.
     *
     * (2) Flat command queue for ops on **real** entities (destroy / emplace /
     *     erase on entities that already exist before this CommandBuffer).
     *     These keep std::function semantics; they're the rare path.
     *
     * Use case examples:
     *   - Mutate the world during view iteration (queue destroy, commit after).
     *   - Build many entities with runtime-determined component sets each frame.
     */
    class CommandBuffer {
    public:
        struct DeferredEntity {
            std::uint32_t id = 0;
            constexpr bool operator==(const DeferredEntity&) const noexcept = default;
        };

        CommandBuffer() = default;
        CommandBuffer(const CommandBuffer&)            = delete;
        CommandBuffer& operator=(const CommandBuffer&) = delete;
        CommandBuffer(CommandBuffer&&) noexcept            = default;
        CommandBuffer& operator=(CommandBuffer&&) noexcept = default;

        // ===== Deferred entity ops (batched per entity) =====

        /// Enqueue creation of an entity. Returns a deferred handle that can
        /// receive further emplace() calls before commit. All those emplaces
        /// are batched into a single archetype allocation at commit time.
        DeferredEntity create() {
            const DeferredEntity de{ next_deferred_++ };
            deferred_ops_.emplace_back();
            return de;
        }

        /// Enqueue creation of an entity with the given components. Same as
        /// repeated emplace, but with explicit types known up front.
        template<class... Cs>
        DeferredEntity create(Cs... vals) {
            static_assert(sizeof...(Cs) > 0, "use create() for empty entity");
            const DeferredEntity de{ next_deferred_++ };
            deferred_ops_.emplace_back();
            (addCtor<Cs>(deferred_ops_.back(), std::move(vals)), ...);
            return de;
        }

        template<class C, class... Args>
        void emplace(DeferredEntity de, Args&&... args) {
            C value(std::forward<Args>(args)...);
            addCtor<C>(deferred_ops_[de.id], std::move(value));
        }

        void destroy(DeferredEntity de) {
            deferred_ops_[de.id].destroyed = true;
        }

        // ===== Real-entity ops (kept as std::function commands) =====

        void destroy(Entity e) {
            real_commands_.emplace_back([e](Registry& r, std::vector<Entity>&) {
                if (r.valid(e)) r.destroy(e);
            });
        }

        template<class C, class... Args>
        void emplace(Entity e, Args&&... args) {
            C value(std::forward<Args>(args)...);
            real_commands_.emplace_back(
                [e, v = std::move(value)](Registry& r, std::vector<Entity>&) mutable {
                    if (r.valid(e)) r.emplace<C>(e, std::move(v));
                });
        }

        template<class C>
        void erase(Entity e) {
            real_commands_.emplace_back([e](Registry& r, std::vector<Entity>&) {
                if (r.valid(e)) r.erase<C>(e);
            });
        }

        template<class C>
        void erase(DeferredEntity de) {
            // Less common — handled as a post-creation command that runs after
            // the batched create has placed the entity.
            real_commands_.emplace_back(
                [de](Registry& r, std::vector<Entity>& resolved) {
                    if (de.id < resolved.size() && r.valid(resolved[de.id])) {
                        r.erase<C>(resolved[de.id]);
                    }
                });
        }

        // ===== Apply =====

        /// Apply every queued deferred-create batch and every real-entity
        /// command, in order. Resets the buffer.
        void commit(Registry& reg) {
            std::vector<Entity> resolved(next_deferred_);

            for (std::uint32_t i = 0; i < next_deferred_; ++i) {
                DeferredOps& ops = deferred_ops_[i];
                if (ops.destroyed && ops.types.empty()) {
                    // Created and destroyed within the same buffer with no
                    // components — no-op; resolved[i] stays kInvalid.
                    continue;
                }

                Entity placed;
                if (ops.types.empty()) {
                    placed = reg.create();
                } else {
                    // Build the target signature and place directly.
                    Signature sig;
                    for (auto tid : ops.types) sig.set(tid);
                    auto p = reg.allocateInSignature(sig);
                    placed = p.e;
                    // Run each per-component ctor against the chunk slot.
                    for (std::size_t k = 0; k < ops.types.size(); ++k) {
                        const auto tid = ops.types[k];
                        const auto& ci = p.arch->comps()[p.arch->localIndex(tid)];
                        void* dst = p.chunk->comp_ptr(ci, p.idx);
                        ops.ctors[k](dst);
                    }
                    // Fire on_construct for each (since allocateInSignature did not).
                    for (auto tid : ops.types) {
                        reg.fireOnConstruct(tid, placed);
                    }
                }
                resolved[i] = placed;

                if (ops.destroyed) {
                    reg.destroy(placed);
                }
            }

            for (auto& cmd : real_commands_) cmd(reg, resolved);

            clear();
        }

        void clear() noexcept {
            deferred_ops_.clear();
            real_commands_.clear();
            next_deferred_ = 0;
        }

        std::size_t size() const noexcept {
            return deferred_ops_.size() + real_commands_.size();
        }
        bool empty() const noexcept { return size() == 0; }

    private:
        struct DeferredOps {
            std::vector<ComponentTypeID>                types;
            std::vector<std::function<void(void*)>>     ctors;   // placement-new each component into a chunk slot
            bool                                        destroyed = false;
        };

        template<class C, class V>
        static void addCtor(DeferredOps& ops, V&& v) {
            ops.types.push_back(TypeRegistry::getTypeID<C>());
            ops.ctors.emplace_back(
                [val = std::move(v)](void* dst) mutable {
                    ::new (dst) C(std::move(val));
                });
        }

        using RealCmd = std::function<void(Registry&, std::vector<Entity>&)>;
        std::vector<DeferredOps> deferred_ops_;
        std::vector<RealCmd>     real_commands_;
        std::uint32_t            next_deferred_ = 0;
    };

} // namespace lux::cxx::archtype
