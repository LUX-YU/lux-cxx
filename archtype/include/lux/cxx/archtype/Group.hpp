#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "Common.hpp"
#include "Registry.hpp"

namespace lux::cxx::archtype {

    /**
     * @class Group
     * @brief Persistent, cache-friendly equivalent of View<Cs...>.
     *
     * Where View<Cs...> filters the archetype set at every construction, Group
     * caches:
     *   - The list of archetypes whose signature contains the include set
     *     (and excludes the exclude set, if any).
     *   - The per-archetype local index for each Cs.
     *
     * Iteration re-uses the cache. Registry bumps an `archetype_version_`
     * counter every time a new archetype is created; if Group sees the
     * version changed it refreshes its cache before iterating, so a Group
     * stored as a System member is always correct.
     *
     * Performance note (honest): in an archetype ECS, iteration speed is
     * already SoA-fast at the View level. Group's savings are limited to the
     * archetype filter cost and per-chunk local-index lookups — i.e., a few
     * percent on top of View, mostly visible in micro-benchmarks. The bigger
     * wins are conceptual (persistent handle, future hook for sort_by) and
     * for size() / empty() calls that would otherwise rebuild the filter set.
     */
    template<class... Cs>
    class Group {
    public:
        explicit Group(Registry& reg) noexcept : registry_(&reg) {
            (include_sig_.set(TypeRegistry::getTypeID<std::remove_cvref_t<Cs>>()), ...);
            refresh();
        }

        Group(const Group&)             = default;
        Group(Group&&) noexcept         = default;
        Group& operator=(const Group&)  = default;
        Group& operator=(Group&&) noexcept = default;

        /// Returns a Group equal to *this with the listed components excluded.
        template<class... Excl>
        Group exclude() const {
            Group g = *this;
            (g.exclude_sig_.set(TypeRegistry::getTypeID<std::remove_cvref_t<Excl>>()), ...);
            g.refresh();
            return g;
        }

        [[nodiscard]] std::size_t size() const {
            refresh_if_stale();
            std::size_t n = 0;
            for (auto& ca : cached_) n += ca.arch->size();
            return n;
        }

        [[nodiscard]] bool empty() const {
            refresh_if_stale();
            for (auto& ca : cached_) if (ca.arch->size() > 0) return false;
            return true;
        }

        /// Iterate entity-by-entity. Lambda accepts `(Entity, Cs&...)` or `(Cs&...)`.
        template<class F>
        void each(F&& f) {
            refresh_if_stale();
            each_dispatch(std::forward<F>(f), std::make_index_sequence<sizeof...(Cs)>{});
        }

        /// Iterate chunk-by-chunk. Lambda receives `(size_t n, Entity*, Cs*...)`.
        template<class F>
        void for_each_chunk(F&& f) {
            refresh_if_stale();
            chunk_dispatch(std::forward<F>(f), std::make_index_sequence<sizeof...(Cs)>{});
        }

        /// Re-scan archetype list. Normally called automatically. const because it
        /// only updates the mutable lazy cache (registry reads are const-safe).
        void refresh() const {
            cached_.clear();
            for (auto& a_up : registry_->archetypes_) {
                Archetype* a = a_up.get();
                if (!a->signature().contains(include_sig_)) continue;
                if (!a->signature().excludes(exclude_sig_)) continue;
                CachedArch ca;
                ca.arch = a;
                std::size_t k = 0;
                ((ca.locals[k++] = a->localIndex(TypeRegistry::getTypeID<std::remove_cvref_t<Cs>>())), ...);
                cached_.push_back(ca);
            }
            cached_version_ = registry_->archetypeVersion();
        }

        /// Sort entities (within each matching archetype) by comparing component C.
        /// After sort, subsequent iteration visits entities in the sorted order.
        ///
        /// Currently a snapshot: any subsequent emplace/erase that migrates an
        /// entity into / out of a matching archetype may disturb the order.
        /// Call sort_by again to re-establish.
        ///
        /// Complexity: O(N log N) compares + O(N * total_component_bytes) data
        /// moves per matching archetype. Allocates a side buffer the size of
        /// one archetype's component storage; freed when the sort finishes.
        template<class C, class Cmp>
        void sort_by(Cmp cmp) {
            refresh_if_stale();
            const ComponentTypeID sort_tid = TypeRegistry::getTypeID<C>();

            for (auto& ca : cached_) {
                Archetype* arch = ca.arch;
                const std::size_t N = arch->size();
                if (N < 2) continue;

                const std::int16_t sort_local = arch->localIndex(sort_tid);
                if (sort_local < 0) continue;
                const auto& sort_ci = arch->comps()[sort_local];

                // Gather all slots in linear traversal order.
                struct Slot { Chunk* chunk; std::uint32_t row; Entity ent; };
                std::vector<Slot> slots;
                slots.reserve(N);
                for (auto& cup : arch->getChunks()) {
                    Chunk* chunk = cup.get();
                    const std::uint32_t cnt = chunk->count();
                    for (std::uint32_t r = 0; r < cnt; ++r) {
                        slots.push_back({ chunk, r, chunk->getEntity(r) });
                    }
                }

                // Sort indices into slots[] by component C.
                std::vector<std::uint32_t> perm(N);
                for (std::uint32_t i = 0; i < N; ++i) perm[i] = i;
                std::sort(perm.begin(), perm.end(), [&](std::uint32_t a, std::uint32_t b) {
                    const C& av = *static_cast<const C*>(slots[a].chunk->comp_ptr(sort_ci, slots[a].row));
                    const C& bv = *static_cast<const C*>(slots[b].chunk->comp_ptr(sort_ci, slots[b].row));
                    return cmp(av, bv);
                });

                // Total bytes per entity across all (non-tag) components.
                std::size_t stride = 0;
                for (auto& ci : arch->comps()) stride += ci.size;
                if (stride == 0) continue; // only tag components — nothing to rearrange

                // Move components into a side buffer in sorted destination order.
                // After this step, the chunk slots are in moved-from / destroyed state.
                std::vector<char> side(N * stride);
                for (std::uint32_t i = 0; i < N; ++i) {
                    const Slot& src = slots[perm[i]];
                    char* dst_base = side.data() + i * stride;
                    std::size_t off = 0;
                    for (auto& ci : arch->comps()) {
                        if (ci.size == 0) continue;
                        char* src_p = static_cast<char*>(src.chunk->comp_ptr(ci, src.row));
                        if (ci.move_and_destroy == nullptr) {
                            std::memcpy(dst_base + off, src_p, ci.size);
                        } else {
                            ci.move_and_destroy(dst_base + off, src_p);
                        }
                        off += ci.size;
                    }
                }

                // Move components back from side buffer into chunks in linear
                // (i.e. sorted) order. After this the chunks are populated and
                // the side buffer is empty (destroyed).
                for (std::uint32_t i = 0; i < N; ++i) {
                    const Slot& dst = slots[i];
                    char* src_base = side.data() + i * stride;
                    std::size_t off = 0;
                    for (auto& ci : arch->comps()) {
                        if (ci.size == 0) continue;
                        char* dst_p = static_cast<char*>(dst.chunk->comp_ptr(ci, dst.row));
                        if (ci.move_and_destroy == nullptr) {
                            std::memcpy(dst_p, src_base + off, ci.size);
                        } else {
                            ci.move_and_destroy(dst_p, src_base + off);
                        }
                        off += ci.size;
                    }
                    // Update the entity-id slot to the sorted entity.
                    const Entity moved_ent = slots[perm[i]].ent;
                    dst.chunk->entities()[dst.row] = moved_ent;
                    // Update the entity slot so try_get<>(e) still resolves.
                    // arch_id is unchanged (we sort within a single archetype).
                    if (registry_->valid(moved_ent)) {
                        auto& s = registry_->slots_[moved_ent.id()];
                        s.arch_id = static_cast<std::uint16_t>(arch->arch_id_);
                        s.row     = (static_cast<std::uint32_t>(dst.chunk->chunk_index()) << kChunkShift)
                                  | dst.row;
                    }
                }
            }
        }

    private:
        void refresh_if_stale() const {
            if (cached_version_ != registry_->archetypeVersion()) refresh();
        }

        struct CachedArch {
            Archetype*   arch;
            std::int16_t locals[sizeof...(Cs)];
        };

        template<class F, std::size_t... Is>
        void each_dispatch(F&& f, std::index_sequence<Is...>) {
            for (auto& ca : cached_) {
                Archetype* arch = ca.arch;
                for (auto& cup : arch->getChunks()) {
                    Chunk* chunk = cup.get();
                    const std::uint32_t n = chunk->count();
                    if (n == 0) continue;
                    Entity* es = chunk->entities();
                    auto arrs = std::make_tuple(
                        static_cast<std::remove_reference_t<Cs>*>(chunk->comp_array_at_local(ca.locals[Is]))...);
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
            for (auto& ca : cached_) {
                for (auto& cup : ca.arch->getChunks()) {
                    Chunk* chunk = cup.get();
                    const std::uint32_t n = chunk->count();
                    if (n == 0) continue;
                    f(static_cast<std::size_t>(n),
                      chunk->entities(),
                      static_cast<std::remove_reference_t<Cs>*>(chunk->comp_array_at_local(ca.locals[Is]))...);
                }
            }
        }

        Registry*               registry_;
        Signature               include_sig_;
        Signature               exclude_sig_;
        // mutable: the cache is a lazy view, refreshed on access even from const
        // queries (size()/empty()); it is not part of the Group's logical value.
        mutable std::vector<CachedArch> cached_;
        mutable std::uint64_t           cached_version_ = static_cast<std::uint64_t>(-1);
    };

    // Out-of-line Registry accessor (declared in Registry, defined after Group).
    template<class... Cs>
    inline Group<Cs...> Registry::group() noexcept { return Group<Cs...>(*this); }

} // namespace lux::cxx::archtype
