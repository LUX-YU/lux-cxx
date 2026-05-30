#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "Common.hpp"
#include "Registry.hpp"

namespace lux::cxx::archtype {

    /**
     * @class ComponentIndex
     * @brief Opt-in per-component direct-pointer cache for fast try_get<C>(e).
     *
     * Default `Registry::try_get<C>(e)` walks: slot -> archetype -> chunk ->
     * component (4–5 indirections, ~28 ns at 1M with random shuffled access).
     *
     * `ComponentIndex<C>` caches a direct `C*` per entity id, brought in sync
     * via:
     *   - on_construct<C> / on_destroy<C> (component appears / disappears)
     *   - on_migrate hook (entity moved between archetypes for *other* reasons)
     *
     * After construction, `idx.try_get(e)` is one cache load → one pointer
     * return, matching EnTT's sparse-set try_get speed (~14–18 ns at 1M).
     *
     * Memory cost: 16 bytes per slot in the entity-id space (~16 MB at 1M
     * entities). Only worth it for components on hot random-access paths
     * (player, camera, network sync sources, editor selection).
     *
     * Lifetime: ComponentIndex stays connected to the Registry it was built
     * from. Destroy it before the Registry, or before the component type is
     * unregistered.
     */
    template<class C>
    class ComponentIndex {
    public:
        explicit ComponentIndex(Registry& reg)
            : reg_(&reg)
            , type_id_(TypeRegistry::getTypeID<C>())
        {
            // Build from current state — walk every archetype that contains C.
            for (auto& a_up : reg_->archetypes_) {
                Archetype* a = a_up.get();
                if (!a->has(type_id_)) continue;
                const std::int16_t local = a->localIndex(type_id_);
                for (auto& cup : a->getChunks()) {
                    Chunk* chunk = cup.get();
                    const std::uint32_t cnt = chunk->count();
                    for (std::uint32_t r = 0; r < cnt; ++r) {
                        Entity e = chunk->getEntity(r);
                        store(e, static_cast<C*>(chunk->comp_ptr(a->comps()[local], r)));
                    }
                }
            }

            // Hook signals — on_construct / on_destroy of C, plus the
            // global migration signal for index sync on cross-archetype moves.
            on_construct_id_ = reg_->template on_construct<C>().connect(
                [this](Registry&, Entity e) { handle_construct(e); });
            on_destroy_id_ = reg_->template on_destroy<C>().connect(
                [this](Registry&, Entity e) { handle_destroy(e); });
            on_migrate_id_ = reg_->connectMigrationListener(
                [this](Entity e, std::uint16_t src, std::uint16_t dst, std::uint32_t row) {
                    handle_migrate(e, src, dst, row);
                });
        }

        ~ComponentIndex() {
            reg_->template on_construct<C>().disconnect(on_construct_id_);
            reg_->template on_destroy<C>().disconnect(on_destroy_id_);
            reg_->disconnectMigrationListener(on_migrate_id_);
        }

        ComponentIndex(const ComponentIndex&)            = delete;
        ComponentIndex& operator=(const ComponentIndex&) = delete;
        ComponentIndex(ComponentIndex&&)                 = delete;
        ComponentIndex& operator=(ComponentIndex&&)      = delete;

        /// Fast path: one cache load + one pointer return.
        C* try_get(Entity e) const noexcept {
            if (e.id() >= cache_.size()) return nullptr;
            const Slot s = cache_[e.id()];
            if (s.gen != e.gen()) return nullptr;
            return s.ptr;
        }

        C& get(Entity e) const noexcept {
            C* p = try_get(e);
            assert(p && "ComponentIndex::get: entity does not have C");
            return *p;
        }

        bool contains(Entity e) const noexcept { return try_get(e) != nullptr; }

    private:
        struct Slot {
            C*            ptr = nullptr;
            std::uint16_t gen = 0;
        };

        void ensure(std::uint32_t id) {
            if (id >= cache_.size()) cache_.resize(id + 1);
        }

        void store(Entity e, C* p) {
            ensure(e.id());
            cache_[e.id()] = { p, static_cast<std::uint16_t>(e.gen()) };
        }

        // C just got added to e in dst archetype. Look up its slot.
        void handle_construct(Entity e) {
            auto& slot = reg_->slots_[e.id()];
            Archetype* arch = reg_->archetypes_[slot.arch_id].get();
            const std::int16_t local = arch->localIndex(type_id_);
            if (local < 0) return;
            Chunk* chunk = arch->getChunks()[slot.row >> kChunkShift].get();
            C* p = static_cast<C*>(chunk->comp_ptr(arch->comps()[local],
                                                   slot.row & static_cast<std::uint32_t>(kChunkMask)));
            store(e, p);
        }

        void handle_destroy(Entity e) {
            if (e.id() < cache_.size()) {
                cache_[e.id()] = {};   // ptr=nullptr, gen=0 — both invalidate try_get
            }
        }

        // Entity migrated between archetypes. If both archetypes have C, its
        // address changed (different chunk / different row). Refresh the cache.
        // If src had C but dst doesn't, treat as destroy.
        // If dst has C but src didn't, on_construct already fired and stored.
        void handle_migrate(Entity e, std::uint16_t src, std::uint16_t dst, std::uint32_t new_row) {
            const bool dst_has = (dst != kNoArchSlot) && reg_->archetypes_[dst]->has(type_id_);
            const bool src_has = (src != kNoArchSlot) && reg_->archetypes_[src]->has(type_id_);
            if (dst_has) {
                Archetype* arch = reg_->archetypes_[dst].get();
                const std::int16_t local = arch->localIndex(type_id_);
                Chunk* chunk = arch->getChunks()[new_row >> kChunkShift].get();
                C* p = static_cast<C*>(chunk->comp_ptr(arch->comps()[local],
                                                       new_row & static_cast<std::uint32_t>(kChunkMask)));
                store(e, p);
            } else if (src_has) {
                handle_destroy(e);
            }
        }

        Registry*           reg_;
        ComponentTypeID     type_id_;
        std::vector<Slot>   cache_;
        Signal::ConnectionId on_construct_id_ = 0;
        Signal::ConnectionId on_destroy_id_   = 0;
        std::size_t         on_migrate_id_    = 0;
    };

} // namespace lux::cxx::archtype
