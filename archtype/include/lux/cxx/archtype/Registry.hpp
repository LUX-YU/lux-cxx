#pragma once
#include <cassert>
#include <cstdint>
#include <memory>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Common.hpp"
#include "Context.hpp"
#include "Signal.hpp"
#include "TypeRegistry.hpp"
#include "Archetype.hpp"

namespace lux::cxx::archtype {

    template<class... Cs> class View;
    template<class... Cs> class Group;

    /**
     * @class Registry
     * @brief Owns all entities, archetypes and chunks; the public ECS API.
     *
     * - Entity is a 32-bit (id, generation) handle. valid(e) catches stale handles.
     * - Components are stored in archetype-keyed chunks (SoA, contiguous).
     * - view<Cs...>() returns a lazy range that iterates SoA arrays directly —
     *   no per-entity getComponent indirection.
     * - The archetype graph caches "add T" / "remove T" transitions per archetype
     *   so repeated migrations are O(1).
     */
    class Registry {
        template<class...> friend class View;
        template<class...> friend class Group;
        template<class>    friend class ComponentIndex;
    public:
        Registry()  = default;
        // Mark the shared liveness token dead before any member (notably signals_)
        // is destroyed, so an Observer outliving this Registry can skip touching
        // the now-dead Signals instead of dereferencing dangling pointers.
        ~Registry() { *alive_ = false; }

        Registry(const Registry&)            = delete;
        Registry& operator=(const Registry&) = delete;

        /// Shared liveness token for objects that hold raw pointers into this
        /// Registry (e.g. Observer's Signal*). The pointed-to bool is true while
        /// the Registry is alive and set false in its destructor.
        [[nodiscard]] std::shared_ptr<bool> aliveToken() const { return alive_; }

        // ====================== Entity lifecycle ======================

        /// Create an entity with no components.
        Entity create() {
            const std::uint32_t id = allocateSlot();
            // slots_[id].gen was bumped on previous destroy (or 0 for new id).
            return Entity(id, slots_[id].gen);
        }

        /// Create an entity with the listed components, constructed from the
        /// passed values. The entity goes directly into the target archetype,
        /// avoiding all intermediate migrations.
        template<class... Cs>
        Entity create(Cs&&... vals) {
            static_assert(sizeof...(Cs) > 0, "create<>() with no components: use create()");

            Signature sig;
            (sig.set(TypeRegistry::getTypeID<std::remove_cvref_t<Cs>>()), ...);
            Archetype* arch = getOrCreateArchetype(sig);

            const std::uint32_t id = allocateSlot();
            Entity e(id, slots_[id].gen);

            auto [chunk, idx] = arch->addEntity(e);
            (constructInPlace<std::remove_cvref_t<Cs>>(chunk, idx, *arch,
                std::forward<Cs>(vals)), ...);

            auto& slot = slots_[id];
            slot.arch_id = archIdAs16(arch->arch_id_);
            slot.row     = packRow(chunk->chunk_index(), idx);

            // Fire on_construct for each component in the order listed.
            (signals_[TypeRegistry::getTypeID<std::remove_cvref_t<Cs>>()]
                .on_construct.publish(*this, e), ...);
            return e;
        }

        /// Pre-grow internal storage. Cheap hint; not required.
        void reserve(std::uint32_t n) {
            slots_.reserve(n);
        }

        // ====================== Migration tracking =======================
        //
        // Fired once per cross-archetype entity migration with (entity,
        // src_arch_id, dst_arch_id, new_row). ComponentIndex<C> uses this to
        // keep its direct-pointer cache in sync when an entity carrying C
        // migrates due to a *different* component being added/removed.
        //
        // Empty-list fast path: when nothing is connected, the hook is one
        // `if (callbacks_.empty()) return;` per migration.
        using MigrationCallback = std::function<
            void(Entity, std::uint16_t /*src*/, std::uint16_t /*dst*/, std::uint32_t /*new_row*/)>;

        std::size_t connectMigrationListener(MigrationCallback cb) {
            const std::size_t id = next_migration_id_++;
            migration_callbacks_.push_back({ id, std::move(cb) });
            return id;
        }
        void disconnectMigrationListener(std::size_t id) {
            for (auto it = migration_callbacks_.begin(); it != migration_callbacks_.end(); ++it) {
                if (it->id == id) { migration_callbacks_.erase(it); return; }
            }
        }
        void fireMigration(Entity e, std::uint16_t src, std::uint16_t dst, std::uint32_t row) noexcept {
            if (migration_callbacks_.empty()) return;
            for (auto& slot : migration_callbacks_) slot.cb(e, src, dst, row);
        }

        // A swap-on-remove relocated `moved` to `row` within the SAME archetype
        // `arch_id` (it changed chunk/row, hence its component addresses, but no
        // construct/destroy fired for it). Record the new location and fire a
        // src==dst migration so pointer caches (e.g. ComponentIndex) refresh —
        // otherwise they keep a dangling/stale C* for the moved entity.
        void noteRelocation(Entity moved, std::uint16_t arch_id, std::uint32_t row) noexcept {
            if (!moved.valid()) return;
            auto& mslot   = slots_[moved.id()];
            mslot.arch_id = arch_id;
            mslot.row     = row;
            fireMigration(moved, arch_id, arch_id, row);
        }

        // ====================== Low-level allocation =====================
        // These are advanced primitives used by CommandBuffer's batched commit.
        // Most users should NOT call these directly — use create<Cs...>().

        /// Result of allocateInSignature: where the new entity lives. The
        /// caller is responsible for constructing every component listed in
        /// `sig` into chunk->comp_ptr(...) AND for firing on_construct via
        /// fireOnConstruct(). The slot is already set up, so try_get is
        /// valid as soon as components are placed.
        struct PlacedEntity {
            Entity        e;
            Archetype*    arch;
            Chunk*        chunk;
            std::uint32_t idx;
        };

        /// Allocate one entity directly into the archetype matching `sig`,
        /// skipping all intermediate migrations. Returns where to write.
        PlacedEntity allocateInSignature(const Signature& sig) {
            Archetype* arch = getOrCreateArchetype(sig);
            const std::uint32_t id = allocateSlot();
            Entity e(id, slots_[id].gen);
            auto [chunk, idx] = arch->addEntity(e);
            auto& slot = slots_[id];
            slot.arch_id = archIdAs16(arch->arch_id_);
            slot.row     = packRow(chunk->chunk_index(), idx);
            fireMigration(e, kNoArchSlot, slot.arch_id, slot.row);
            return { e, arch, chunk, idx };
        }

        /// Publish on_construct for component `tid` on entity `e`. Used by
        /// CommandBuffer's batched commit after components have been placed.
        void fireOnConstruct(ComponentTypeID tid, Entity e) noexcept {
            signals_[tid].on_construct.publish(*this, e);
        }

        /// Release the heap buffers of every empty chunk across all archetypes.
        /// Call this at level boundaries / after large entity churn to reclaim
        /// peak memory. The chunk slots themselves are preserved (tombstoned)
        /// so existing EntityRecords with their stable chunk_idx remain valid;
        /// subsequent emplaces reacquire buffers transparently.
        ///
        /// Returns the total bytes released.
        std::size_t compact() noexcept {
            std::size_t total = 0;
            for (auto& a : archetypes_) {
                total += a->compactEmptyChunks();
            }
            return total;
        }

        bool valid(Entity e) const noexcept {
            return e.id() < slots_.size() && slots_[e.id()].gen == e.gen();
        }

        void destroy(Entity e) {
            if (!valid(e)) return;
            auto& slot = slots_[e.id()];
            if (slot.arch_id != kNoArchSlot) {
                Archetype* arch = archetypes_[slot.arch_id].get();
                Chunk* ch       = arch->getChunks()[slot.row >> kChunkShift].get();
                const std::uint32_t idx = idxInChunk(slot.row);
                // Fire on_destroy for each component BEFORE removal so the
                // listener can still read the about-to-be-destroyed state.
                for (auto& ci : arch->comps()) {
                    signals_[ci.type_id].on_destroy.publish(*this, e);
                }
                noteRelocation(arch->removeEntity(ch, idx), slot.arch_id,
                               packRow(ch->chunk_index(), idx));
            }
            // Bump generation, mark slot free, and link onto the intrusive
            // free list via the row field. The row field is unused while the
            // slot is on the free list — we'll overwrite it when this id is
            // recycled in allocateSlot().
            slot.gen     = static_cast<std::uint16_t>((slot.gen + 1) & ((1u << Entity::kGenBits) - 1));
            slot.arch_id = kNoArchSlot;
            slot.row     = free_head_;
            free_head_   = e.id();
        }

        // ====================== Component manipulation ======================

        /// Construct a component in place (existing entity).
        /// If the entity already has this component, the value is overwritten in place.
        template<class C, class... Args>
        C& emplace(Entity e, Args&&... args) {
            assert(valid(e));
            auto& slot = slots_[e.id()];
            const auto type_id = TypeRegistry::getTypeID<C>();

            if (slot.arch_id != kNoArchSlot) {
                Archetype* src       = archetypes_[slot.arch_id].get();
                Chunk*     src_chunk = src->getChunks()[slot.row >> kChunkShift].get();
                const std::uint32_t src_idx = idxInChunk(slot.row);

                if (src->has(type_id)) {
                    // Overwrite in place. Build the replacement first so a throwing
                    // constructor leaves the existing component intact rather than
                    // destroyed-but-still-counted (which would be destroyed again
                    // later — double destruction / UB).
                    const auto& ci = src->comps()[src->localIndex(type_id)];
                    C* p = static_cast<C*>(src_chunk->comp_ptr(ci, src_idx));
                    C tmp(std::forward<Args>(args)...);
                    if constexpr (!std::is_trivially_destructible_v<C>) p->~C();
                    ::new (p) C(std::move(tmp));
                    signals_[type_id].on_update.publish(*this, e);
                    return *p;
                }

                // Build the new component up-front: a throwing constructor must not
                // leave the entity half-migrated (dst row allocated + the other
                // components already moved out of src) with the exception escaping.
                C tmp(std::forward<Args>(args)...);

                Archetype* dst = src->addEdge(type_id);
                if (!dst) {
                    Signature ns = src->signature(); ns.set(type_id);
                    dst = getOrCreateArchetype(ns);
                    src->setAddEdge(type_id, dst);
                    dst->setRemoveEdge(type_id, src);
                }

                auto [new_chunk, new_idx] = dst->addEntity(e);
                moveExistingComponents(*src, src_chunk, src_idx, *dst, new_chunk, new_idx);

                // Move the pre-built component into place (move is typically noexcept).
                const auto& d_ci = dst->comps()[dst->localIndex(type_id)];
                C* np = static_cast<C*>(new_chunk->comp_ptr(d_ci, new_idx));
                ::new (np) C(std::move(tmp));

                // Remove source slot (components already moved out, so don't destroy them).
                noteRelocation(src->removeEntityRaw(src_chunk, src_idx), slot.arch_id,
                               packRow(src_chunk->chunk_index(), src_idx));

                const std::uint16_t old_arch_id = slot.arch_id;
                slot.arch_id = archIdAs16(dst->arch_id_);
                slot.row     = packRow(new_chunk->chunk_index(), new_idx);
                fireMigration(e, old_arch_id, slot.arch_id, slot.row);
                signals_[type_id].on_construct.publish(*this, e);
                return *np;
            }

            // No archetype yet — entity has 0 components. Build the component
            // before touching the destination archetype so a throwing constructor
            // leaves no phantom (counted-but-uninitialized) row behind.
            Signature ns; ns.set(type_id);
            C tmp(std::forward<Args>(args)...);
            Archetype* dst = getOrCreateArchetype(ns);
            auto [new_chunk, new_idx] = dst->addEntity(e);
            const auto& d_ci = dst->comps()[dst->localIndex(type_id)];
            C* p = static_cast<C*>(new_chunk->comp_ptr(d_ci, new_idx));
            ::new (p) C(std::move(tmp));

            slot.arch_id = archIdAs16(dst->arch_id_);
            slot.row     = packRow(new_chunk->chunk_index(), new_idx);
            fireMigration(e, kNoArchSlot, slot.arch_id, slot.row);
            signals_[type_id].on_construct.publish(*this, e);
            return *p;
        }

        /// Alias for emplace<C>(): construct-or-overwrite semantics. Same as
        /// EnTT's emplace_or_replace.
        template<class C, class... Args>
        C& emplace_or_replace(Entity e, Args&&... args) {
            return emplace<C>(e, std::forward<Args>(args)...);
        }

        /// Returns the existing component, or constructs a new one if absent.
        /// Fires on_construct only when a new component is added.
        template<class C, class... Args>
        C& get_or_emplace(Entity e, Args&&... args) {
            if (C* p = try_get<C>(e)) return *p;
            return emplace<C>(e, std::forward<Args>(args)...);
        }

        /// Mutate an existing component in place and fire on_update. The
        /// callback signature is `void(C&)`. Asserts the component exists.
        template<class C, class F>
        C& patch(Entity e, F&& f) {
            C& ref = get<C>(e);
            std::forward<F>(f)(ref);
            signals_[TypeRegistry::getTypeID<C>()].on_update.publish(*this, e);
            return ref;
        }

        /// Remove component C from entity (no-op if not present).
        template<class C>
        void erase(Entity e) {
            assert(valid(e));
            auto& slot = slots_[e.id()];
            if (slot.arch_id == kNoArchSlot) return;

            const auto type_id = TypeRegistry::getTypeID<C>();
            Archetype* src       = archetypes_[slot.arch_id].get();
            Chunk*     src_chunk = src->getChunks()[slot.row >> kChunkShift].get();
            const std::uint32_t src_idx = idxInChunk(slot.row);
            if (!src->has(type_id)) return;

            // Fire on_destroy BEFORE removal so the listener can still read
            // the component via get<C>(e).
            signals_[type_id].on_destroy.publish(*this, e);

            Archetype* dst = src->removeEdge(type_id);
            if (!dst) {
                Signature ns = src->signature(); ns.reset(type_id);
                if (ns.empty()) {
                    // Special case: removing the last component leaves the entity with no archetype.
                    // Destroy the component in place and compact.
                    const auto& s_ci = src->comps()[src->localIndex(type_id)];
                    if (s_ci.destroy) {
                        s_ci.destroy(src_chunk->comp_ptr(s_ci, src_idx));
                    }
                    noteRelocation(src->removeEntityRaw(src_chunk, src_idx), slot.arch_id,
                                   packRow(src_chunk->chunk_index(), src_idx));
                    // Entity is still alive — clear archetype location, keep gen.
                    slot.arch_id = kNoArchSlot;
                    slot.row     = 0;
                    return;
                }
                dst = getOrCreateArchetype(ns);
                src->setRemoveEdge(type_id, dst);
                dst->setAddEdge(type_id, src);
            }

            auto [new_chunk, new_idx] = dst->addEntity(e);

            // Move all components except the erased one.
            for (auto& s_ci : src->comps()) {
                if (s_ci.type_id == type_id) {
                    // Destroy the component being removed.
                    if (s_ci.destroy) {
                        s_ci.destroy(src_chunk->comp_ptr(s_ci, src_idx));
                    }
                    continue;
                }
                if (s_ci.size == 0) continue; // tag — nothing to move
                const auto& d_ci = dst->comps()[dst->localIndex(s_ci.type_id)];
                char* src_p = static_cast<char*>(src_chunk->comp_ptr(s_ci, src_idx));
                char* dst_p = static_cast<char*>(new_chunk->comp_ptr(d_ci, new_idx));
                if (s_ci.move_and_destroy == nullptr) {
                    std::memcpy(dst_p, src_p, s_ci.size);
                } else {
                    s_ci.move_and_destroy(dst_p, src_p);
                }
            }

            noteRelocation(src->removeEntityRaw(src_chunk, src_idx), slot.arch_id,
                           packRow(src_chunk->chunk_index(), src_idx));

            const std::uint16_t old_arch_id = slot.arch_id;
            slot.arch_id = archIdAs16(dst->arch_id_);
            slot.row     = packRow(new_chunk->chunk_index(), new_idx);
            fireMigration(e, old_arch_id, slot.arch_id, slot.row);
        }

        /// Atomically remove multiple components in a single migration.
        /// Components not present on the entity are silently ignored.
        /// on_destroy fires once for each component that was actually present,
        /// in the order they appear in the template parameter list.
        template<class C1, class C2, class... Rest>
        void erase(Entity e) {
            assert(valid(e));
            auto& slot = slots_[e.id()];
            if (slot.arch_id == kNoArchSlot) return;

            constexpr std::size_t N = 2 + sizeof...(Rest);
            const ComponentTypeID type_ids[N] = {
                TypeRegistry::getTypeID<C1>(),
                TypeRegistry::getTypeID<C2>(),
                TypeRegistry::getTypeID<Rest>()...
            };

            Archetype* src       = archetypes_[slot.arch_id].get();
            Chunk*     src_chunk = src->getChunks()[slot.row >> kChunkShift].get();
            const std::uint32_t src_idx = idxInChunk(slot.row);

            Signature target_sig = src->signature();
            bool any_present = false;
            for (auto tid : type_ids) {
                if (target_sig.test(tid)) { target_sig.reset(tid); any_present = true; }
            }
            if (!any_present) return;

            // Fire on_destroy for every component actually being removed.
            for (auto tid : type_ids) {
                if (src->has(tid)) {
                    signals_[tid].on_destroy.publish(*this, e);
                }
            }

            auto being_erased = [&](ComponentTypeID t) noexcept {
                for (auto tid : type_ids) if (tid == t) return true;
                return false;
            };

            // Edge case: removing everything leaves the entity with no archetype.
            if (target_sig.empty()) {
                for (auto& s_ci : src->comps()) {
                    if (s_ci.destroy) {
                        s_ci.destroy(src_chunk->comp_ptr(s_ci, src_idx));
                    }
                }
                noteRelocation(src->removeEntityRaw(src_chunk, src_idx), slot.arch_id,
                               packRow(src_chunk->chunk_index(), src_idx));
                // Entity still alive, no archetype, generation preserved.
                slot.arch_id = kNoArchSlot;
                slot.row     = 0;
                return;
            }

            Archetype* dst = getOrCreateArchetype(target_sig);
            auto [new_chunk, new_idx] = dst->addEntity(e);

            for (auto& s_ci : src->comps()) {
                if (being_erased(s_ci.type_id)) {
                    if (s_ci.destroy) {
                        s_ci.destroy(src_chunk->comp_ptr(s_ci, src_idx));
                    }
                    continue;
                }
                if (s_ci.size == 0) continue; // tag — nothing to move
                const auto& d_ci = dst->comps()[dst->localIndex(s_ci.type_id)];
                char* src_p = static_cast<char*>(src_chunk->comp_ptr(s_ci, src_idx));
                char* dst_p = static_cast<char*>(new_chunk->comp_ptr(d_ci, new_idx));
                if (s_ci.move_and_destroy == nullptr) {
                    std::memcpy(dst_p, src_p, s_ci.size);
                } else {
                    s_ci.move_and_destroy(dst_p, src_p);
                }
            }

            noteRelocation(src->removeEntityRaw(src_chunk, src_idx), slot.arch_id,
                           packRow(src_chunk->chunk_index(), src_idx));
            const std::uint16_t old_arch_id_multi = slot.arch_id;
            slot.arch_id = archIdAs16(dst->arch_id_);
            slot.row     = packRow(new_chunk->chunk_index(), new_idx);
            fireMigration(e, old_arch_id_multi, slot.arch_id, slot.row);
        }

        /// Returns pointer to component or nullptr if entity lacks it.
        /// Hot path: one 8-byte slot load (gen + arch_id + row in a single
        /// cache line), then archetype/chunk indirection via L1-hot tables.
        template<class C>
        C* try_get(Entity e) noexcept {
            if (e.id() >= slots_.size()) return nullptr;
            const EntitySlot s = slots_[e.id()];
            if (s.gen != e.gen())               return nullptr;
            if (s.arch_id == kNoArchSlot)       return nullptr;
            Archetype* arch = archetypes_[s.arch_id].get();
            const auto type_id = TypeRegistry::getTypeID<C>();
            const std::int16_t local = arch->localIndex(type_id);
            if (local < 0) return nullptr;
            Chunk* chunk = arch->getChunks()[s.row >> kChunkShift].get();
            return static_cast<C*>(chunk->comp_ptr(arch->comps()[local], idxInChunk(s.row)));
        }

        template<class C>
        C& get(Entity e) noexcept {
            C* p = try_get<C>(e);
            assert(p && "Registry::get<C>: entity does not have component C");
            return *p;
        }

        template<class C>
        bool has(Entity e) const noexcept {
            if (e.id() >= slots_.size()) return false;
            const EntitySlot s = slots_[e.id()];
            if (s.gen != e.gen())               return false;
            if (s.arch_id == kNoArchSlot)       return false;
            return archetypes_[s.arch_id]->has(TypeRegistry::getTypeID<C>());
        }

        // ====================== Views ======================

        /// Lazy view over all entities matching the given component set.
        template<class... Cs>
        View<Cs...> view() noexcept;

        /// Persistent cached view. Refresh is automatic when new archetypes
        /// appear. Use this for systems that iterate the same component set
        /// every frame. See Group.hpp.
        template<class... Cs>
        Group<Cs...> group() noexcept;

        // ====================== Signals ======================
        //
        // Fired by the Registry as part of create / emplace / erase / destroy.
        //
        // Semantics (EnTT-compatible):
        //   on_construct<C>  fires AFTER C is placed and the record is updated.
        //   on_update<C>     fires AFTER an in-place replacement (emplace<C> on
        //                    an entity that already had C).
        //   on_destroy<C>    fires BEFORE C is removed, so the listener can
        //                    still call get<C>(e).
        //
        // Listeners should not mutate the in-progress entity directly; use a
        // CommandBuffer for that.
        template<class C> Signal& on_construct() noexcept {
            return signals_[TypeRegistry::getTypeID<C>()].on_construct;
        }
        template<class C> Signal& on_update() noexcept {
            return signals_[TypeRegistry::getTypeID<C>()].on_update;
        }
        template<class C> Signal& on_destroy() noexcept {
            return signals_[TypeRegistry::getTypeID<C>()].on_destroy;
        }

        // ====================== Context / resources ======================
        //
        // Per-registry singleton storage for things that aren't entity
        // components (time, input, asset manager, RNG, ...). Indexed by
        // std::type_index, so independent from the kMaxComponents=64 limit.
        ContextStorage&       ctx()       noexcept { return ctx_; }
        const ContextStorage& ctx() const noexcept { return ctx_; }

        // ====================== Back-compat shims ======================

        Entity createEntity()          { return create(); }
        void   destroyEntity(Entity e) { destroy(e); }

        template<class C, class... Args>
        C& addComponent(Entity e, Args&&... args) { return emplace<C>(e, std::forward<Args>(args)...); }

        template<class C>
        void removeComponent(Entity e) { erase<C>(e); }

        template<class C>
        C* getComponent(Entity e) noexcept { return try_get<C>(e); }

        /// Back-compat: returns a materialized vector. Prefer view<Cs...>() for performance.
        template<class... Cs>
        std::vector<Entity> queryEntities() {
            std::vector<Entity> out;
            view<Cs...>().for_each_chunk(
                [&](std::size_t n, Entity* es, Cs*... /*arrs*/) {
                    const std::size_t old = out.size();
                    out.resize(old + n);
                    std::memcpy(out.data() + old, es, n * sizeof(Entity));
                });
            return out;
        }

    private:
        /// Compact 8-byte entity slot. Stores:
        ///   - gen     : current generation; entity is valid iff Entity.gen() matches.
        ///   - arch_id : index into archetypes_ (16-bit so the entire slot fits in 8 bytes;
        ///               capped at 65535 archetypes, which is plenty for any real game).
        ///               kNoArchSlot == 0xFFFF means "entity exists but has no components".
        ///   - row     : (chunk_idx << kChunkShift) | idx_in_chunk.
        ///
        /// All metadata for an entity lookup lives in one cache line, so try_get<C>(e)
        /// is one record load instead of "gen vector miss + records vector miss".
        struct EntitySlot {
            std::uint16_t gen     = 0;
            std::uint16_t arch_id = kNoArchSlot;
            std::uint32_t row     = 0;
        };

        void ensureSlot(std::uint32_t id) {
            if (id >= slots_.size()) slots_.resize(id + 1);
        }

        /// Allocate an entity id. Pops from the intrusive free list (slot's
        /// row field is the "next" pointer while free) or extends slots_.
        std::uint32_t allocateSlot() {
            if (free_head_ != kFreeListEnd) {
                const std::uint32_t id = free_head_;
                free_head_ = slots_[id].row;     // unlink: head ← next
                slots_[id].arch_id = kNoArchSlot;
                slots_[id].row     = 0;          // reset for use as chunk row
                return id;
            }
            const std::uint32_t id = static_cast<std::uint32_t>(slots_.size());
            slots_.emplace_back();
            return id;
        }

        // ---- Slot lookup helpers ----
        Archetype* archetypeAt(std::uint16_t arch_id) const noexcept {
            return arch_id == kNoArchSlot ? nullptr : archetypes_[arch_id].get();
        }
        Chunk* chunkAt(std::uint16_t arch_id, std::uint32_t row) const noexcept {
            Archetype* a = archetypeAt(arch_id);
            return a ? a->getChunks()[row >> kChunkShift].get() : nullptr;
        }
        static constexpr std::uint32_t idxInChunk(std::uint32_t row) noexcept {
            return row & static_cast<std::uint32_t>(kChunkMask);
        }
        static constexpr std::uint32_t packRow(std::int32_t chunk_idx, std::uint32_t idx) noexcept {
            return (static_cast<std::uint32_t>(chunk_idx) << kChunkShift) | idx;
        }
        static constexpr std::uint16_t archIdAs16(std::uint32_t id) noexcept {
            return static_cast<std::uint16_t>(id);
        }

        Archetype* getOrCreateArchetype(const Signature& sig) {
            auto it = archetype_map_.find(sig);
            if (it != archetype_map_.end()) return it->second;
            auto up = std::make_unique<Archetype>(sig);
            Archetype* raw = up.get();
            raw->arch_id_ = static_cast<std::uint32_t>(archetypes_.size());
            archetype_map_.emplace(sig, raw);
            archetypes_.push_back(std::move(up));
            ++archetype_version_;            // invalidate Group caches
            return raw;
        }

    public:
        /// Monotonic counter bumped each time a new Archetype is created.
        /// Used by Group<> to detect when its cached archetype list went stale.
        std::uint64_t archetypeVersion() const noexcept { return archetype_version_; }

    private:

        template<class T>
        void constructInPlace(Chunk* chunk, std::uint32_t row, const Archetype& arch, T&& val) {
            const auto type_id = TypeRegistry::getTypeID<std::remove_cvref_t<T>>();
            const auto& ci = arch.comps()[arch.localIndex(type_id)];
            void* p = chunk->comp_ptr(ci, row);
            ::new (p) std::remove_cvref_t<T>(std::forward<T>(val));
        }

        /// Move every component from (src, row_s) to (dst, row_d). Assumes dst
        /// archetype is a strict superset of src archetype's signature.
        static void moveExistingComponents(
            Archetype& src, Chunk* src_chunk, std::uint32_t row_s,
            Archetype& dst, Chunk* dst_chunk, std::uint32_t row_d) noexcept
        {
            for (auto& s_ci : src.comps()) {
                if (s_ci.size == 0) continue; // tag — nothing to move
                const std::int16_t d_local = dst.localIndex(s_ci.type_id);
                const auto& d_ci = dst.comps()[d_local];
                char* src_p = static_cast<char*>(src_chunk->comp_ptr(s_ci, row_s));
                char* dst_p = static_cast<char*>(dst_chunk->comp_ptr(d_ci, row_d));
                if (s_ci.move_and_destroy == nullptr) {
                    std::memcpy(dst_p, src_p, s_ci.size);
                } else {
                    s_ci.move_and_destroy(dst_p, src_p);
                }
            }
        }

    public:
        /// Lookup or build the cached archetype list matching (include, exclude).
        /// Reused by View<Cs...>::each / for_each_chunk / size to avoid the
        /// O(num_archetypes) filter cost on every call.
        const std::vector<Archetype*>& viewCacheLookup(
            const Signature& inc, const Signature& exc) const {
            // Linear scan — typical number of distinct view signatures in a game is small.
            for (auto& ep : view_cache_) {
                if (ep->inc == inc && ep->exc == exc) {
                    if (ep->version != archetype_version_) refreshViewCacheEntry(*ep);
                    return ep->archetypes;
                }
            }
            // Not found — build and insert. Evict oldest if at cap.
            if (view_cache_.size() >= kViewCacheMax) {
                view_cache_.erase(view_cache_.begin());
            }
            auto e = std::make_unique<ViewCacheEntry>();
            e->inc = inc;
            e->exc = exc;
            refreshViewCacheEntry(*e);
            view_cache_.push_back(std::move(e));
            return view_cache_.back()->archetypes;
        }

    private:
        struct ViewCacheEntry {
            Signature inc;
            Signature exc;
            std::uint64_t version = 0;
            std::vector<Archetype*> archetypes;
        };
        static constexpr std::size_t kViewCacheMax = 32;

        void refreshViewCacheEntry(ViewCacheEntry& e) const {
            e.archetypes.clear();
            for (auto& a_up : archetypes_) {
                Archetype* a = a_up.get();
                if (!a->signature().contains(e.inc)) continue;
                if (!a->signature().excludes(e.exc)) continue;
                e.archetypes.push_back(a);
            }
            e.version = archetype_version_;
        }

        struct MigrationSlot { std::size_t id; MigrationCallback cb; };

        std::vector<EntitySlot>                    slots_;
        std::uint32_t                              free_head_ = kFreeListEnd;  // intrusive list (next id in slots_[id].row)
        std::unordered_map<Signature, Archetype*, SignatureHash> archetype_map_;
        std::vector<std::unique_ptr<Archetype>>    archetypes_;
        SignalSet                                  signals_[kMaxComponents];
        ContextStorage                             ctx_;
        std::uint64_t                              archetype_version_ = 0;
        std::vector<MigrationSlot>                 migration_callbacks_;
        std::size_t                                next_migration_id_ = 1;
        // Liveness token shared with Observers (see aliveToken()). Declared last-ish
        // and reset in the destructor body, which runs before members are torn down.
        std::shared_ptr<bool>                      alive_ = std::make_shared<bool>(true);

        // unique_ptr so reference into entry survives view_cache_ reallocation.
        mutable std::vector<std::unique_ptr<ViewCacheEntry>> view_cache_;
    };


    //====================================================================
    //                              View
    //====================================================================

    /**
     * @class View
     * @brief Lightweight, lazy iterator over archetypes matching a component set.
     *
     * Construction is O(num_archetypes). Iteration via each() or for_each_chunk()
     * resolves component array pointers once per chunk and does a tight SoA inner
     * loop — no per-entity hashing or registry lookups.
     */
    template<class... Cs>
    class View {
    public:
        explicit View(Registry* reg) noexcept : registry_(reg) {
            (include_sig_.set(TypeRegistry::getTypeID<std::remove_cvref_t<Cs>>()), ...);
        }

        /// Return a copy of *this with the listed types added to the exclude set.
        template<class... Excl>
        View exclude() const {
            View v = *this;
            (v.exclude_sig_.set(TypeRegistry::getTypeID<std::remove_cvref_t<Excl>>()), ...);
            return v;
        }

        /// Total number of entities across all matching archetypes.
        std::size_t size() const noexcept {
            std::size_t n = 0;
            for (auto* a : registry_->viewCacheLookup(include_sig_, exclude_sig_)) {
                n += a->size();
            }
            return n;
        }

        bool empty() const noexcept {
            for (auto* a : registry_->viewCacheLookup(include_sig_, exclude_sig_)) {
                if (a->size() > 0) return false;
            }
            return true;
        }

        /// Invoke f(Entity, Cs&...) — or f(Cs&...) — for every matching entity.
        template<class F>
        void each(F&& f) {
            each_dispatch(std::forward<F>(f), std::make_index_sequence<sizeof...(Cs)>{});
        }

        /// Invoke f(size_t count, Entity* es, Cs*... arrays) once per non-empty chunk.
        /// Use this for SIMD-friendly inner loops over contiguous component arrays.
        template<class F>
        void for_each_chunk(F&& f) {
            chunk_dispatch(std::forward<F>(f), std::make_index_sequence<sizeof...(Cs)>{});
        }

    private:
        template<class F, std::size_t... Is>
        void each_dispatch(F&& f, std::index_sequence<Is...>) {
            // TypeRegistry is keyed by cv-stripped type; const-ness lives in
            // the template arg Cs so it can flow through to the user's lambda.
            const ComponentTypeID type_ids[sizeof...(Cs)] = {
                TypeRegistry::getTypeID<std::remove_cvref_t<Cs>>()...
            };
            auto& archetypes_ = registry_->viewCacheLookup(include_sig_, exclude_sig_);
            for (Archetype* arch : archetypes_) {
                const std::int16_t locals[sizeof...(Cs)] = { arch->localIndex(type_ids[Is])... };
                for (auto& cup : arch->getChunks()) {
                    Chunk* chunk = cup.get();
                    const std::uint32_t n = chunk->count();
                    if (n == 0) continue;
                    Entity* es = chunk->entities();
                    // Preserve const: view<const Position, Velocity> stores
                    // (const Position*, Velocity*) in the tuple.
                    auto arrs = std::make_tuple(
                        static_cast<std::remove_reference_t<Cs>*>(chunk->comp_array_at_local(locals[Is]))...);
                    if constexpr (std::is_invocable_v<F, Entity, Cs&...>) {
                        for (std::uint32_t r = 0; r < n; ++r) {
                            f(es[r], std::get<Is>(arrs)[r]...);
                        }
                    } else {
                        for (std::uint32_t r = 0; r < n; ++r) {
                            f(std::get<Is>(arrs)[r]...);
                        }
                    }
                }
            }
        }

        template<class F, std::size_t... Is>
        void chunk_dispatch(F&& f, std::index_sequence<Is...>) {
            const ComponentTypeID type_ids[sizeof...(Cs)] = {
                TypeRegistry::getTypeID<std::remove_cvref_t<Cs>>()...
            };
            auto& archetypes_ = registry_->viewCacheLookup(include_sig_, exclude_sig_);
            for (Archetype* arch : archetypes_) {
                const std::int16_t locals[sizeof...(Cs)] = { arch->localIndex(type_ids[Is])... };
                for (auto& cup : arch->getChunks()) {
                    Chunk* chunk = cup.get();
                    const std::uint32_t n = chunk->count();
                    if (n == 0) continue;
                    f(static_cast<std::size_t>(n),
                      chunk->entities(),
                      static_cast<std::remove_reference_t<Cs>*>(chunk->comp_array_at_local(locals[Is]))...);
                }
            }
        }

        Registry*               registry_ = nullptr;
        Signature               include_sig_;
        Signature               exclude_sig_;
    };

    template<class... Cs>
    inline View<Cs...> Registry::view() noexcept { return View<Cs...>(this); }

} // namespace lux::cxx::archtype
