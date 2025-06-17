#pragma once
#include <unordered_map>
#include <vector>
#include <memory>
#include <cassert>

#include "Common.hpp"
#include "EntityManager.hpp"
#include "Signature.hpp"
#include "Component.hpp"
#include "TypeRegistry.hpp"
#include "Archetype.hpp"

namespace lux::cxx::archtype {

    /**
     * @class Registry
     * @brief The central class for managing entities and their components.
     *        Internally uses Archetypes for efficient data storage 
     *        and manipulates entity signatures to track component additions/removals.
     */
    class Registry {
    public:
        Registry() = default;

        /**
         * @brief Creates a new entity with no components.
         * 
         * @return The entity ID.
         */
        Entity createEntity() {
            Entity e = entity_manager_.createEntity();
            if (e >= entity_locations_.size()) {
                entity_locations_.resize(e + 1);
            }
            entity_locations_[e] = { nullptr, nullptr, 0 };
            return e;
        }

        /**
         * @brief Destroys an entity, removing it from its Archetype if present.
         *        The entity ID is returned to the free list for reuse.
         * 
         * @param e The entity ID to destroy.
         */
        void destroyEntity(Entity e) {
            assert(e < entity_locations_.size());
            auto& loc = entity_locations_[e];
            if (loc.archetype) {
                Archetype* arch = loc.archetype;
                Chunk* chunk    = loc.chunk;
                size_t idx      = loc.index;
                // Remove from Archetype
                Entity moved = arch->removeEntity(chunk, idx);
                if (moved != kInvalidEntity) {
                    // Update the location of any entity that got swapped in
                    entity_locations_[moved].chunk = chunk;
                    entity_locations_[moved].index = idx;
                }
            }
            entity_locations_[e] = {nullptr, nullptr, 0};
            entity_manager_.destroyEntity(e);
        }

        /**
         * @brief Adds component T to the specified entity. If the entity is currently 
         *        in an Archetype that does not include T, the entity will be moved 
         *        to a new Archetype that does include T.
         * 
         * @tparam T The component type to add.
         * @tparam Args Parameter pack for T's constructor.
         * @param e The entity ID to which the component is added.
         * @param args Arguments to forward to T's constructor.
         * @return Reference to the newly constructed or updated component T.
         */
        template<typename T, typename... Args>
        T& addComponent(Entity e, Args&&... args) {
            assert(e < entity_locations_.size());
            auto& loc = entity_locations_[e];
            const auto comp_id = TypeRegistry::getTypeID<T>();

            // If entity already has this component, just update it in-place
            if (loc.archetype && loc.archetype->signature().test(comp_id)) {
                T* comp_ptr = reinterpret_cast<T*>( loc.chunk->getComponentData(comp_id, loc.index) );
                if constexpr (std::is_trivially_copy_assignable_v<T>) {
                    if constexpr (std::is_constructible_v<T, Args...>) {
                        *comp_ptr = T(std::forward<Args>(args)...);
                    } else {
                        *comp_ptr = T{std::forward<Args>(args)...};
                    }
                } else {
                    comp_ptr->~T();
                    new (comp_ptr) T(std::forward<Args>(args)...);
                }
                return *comp_ptr;
            }

            // Otherwise, we need to move the entity to an Archetype that contains T.
            Signature new_sig;
            Archetype* old_arch = loc.archetype;
            if (old_arch) {
                new_sig = old_arch->signature();
            }
            new_sig.set(comp_id);

            Archetype* new_arch = getOrCreateArchetype(new_sig);
            auto [new_chunk, new_idx] = new_arch->addEntity(e);

            // If there's an old archetype, move existing components to the new archetype
            if (old_arch) {
                Signature old_sig = old_arch->signature();
                Chunk* old_chunk  = loc.chunk;
                size_t old_idx    = loc.index;

                for (std::size_t cid=0; cid<kMaxComponents; ++cid) {
                    if (!old_sig.test(cid) || cid == comp_id) continue;

                    void* src = old_chunk->getComponentData(cid, old_idx);
                    void* dst = new_chunk->getComponentData(cid, new_idx);
                    auto& ops = TypeRegistry::getOps(cid);

                    if (ops.move_construct) {
                        ops.move_construct(dst, src);
                    } else {
                        std::memcpy(dst, src, TypeRegistry::getTypeInfo(cid).size);
                    }
                }
                // Remove entity from old Archetype
                Entity moved = old_arch->removeEntity(old_chunk, old_idx);
                if (moved != kInvalidEntity) {
                    entity_locations_[moved].chunk = old_chunk;
                    entity_locations_[moved].index = old_idx;
                }
            }

            // Construct the new component
            void* storage = new_chunk->getComponentData(comp_id, new_idx);
            if constexpr (std::is_constructible_v<T, Args...>) {
                new (storage) T(std::forward<Args>(args)...);
            } else {
                // fallback if T is not directly constructible with Args...
                T temp{std::forward<Args>(args)...};
                new (storage) T(temp);
            }

            // Update the entity's location
            entity_locations_[e] = { new_arch, new_chunk, new_idx };
            return *reinterpret_cast<T*>(storage);
        }

        /**
         * @brief Removes the specified component T from an entity, 
         *        moving the entity to a new Archetype that excludes T.
         * 
         * @tparam T The component type to remove.
         * @param e The entity ID from which to remove the component.
         */
        template<typename T>
        void removeComponent(Entity e) {
            assert(e < entity_locations_.size());
            auto& loc = entity_locations_[e];
            if (!loc.archetype) return;

            auto comp_id = TypeRegistry::getTypeID<T>();
            if (!loc.archetype->signature().test(comp_id)) {
                return; // nothing to remove
            }

            // Create a new signature that excludes T
            Signature old_sig = loc.archetype->signature();
            Signature new_sig = old_sig;
            new_sig.reset(comp_id);

            Archetype* new_arch = getOrCreateArchetype(new_sig);
            auto [new_chunk, new_idx] = new_arch->addEntity(e);

            // Move remaining components from old archetype to new
            Chunk* old_chunk = loc.chunk;
            size_t old_idx   = loc.index;

            for (ComponentTypeID cid=0; cid<kMaxComponents; ++cid) {
                if (!old_sig.test(cid)) continue;
                if (cid == comp_id) {
                    // Destroy the removed component
                    void* old_ptr = old_chunk->getComponentData(cid, old_idx);
                    auto& ops = TypeRegistry::getOps(cid);
                    if (ops.destroy) {
                        ops.destroy(old_ptr);
                    }
                    continue;
                }
                // Move other components
                void* src = old_chunk->getComponentData(cid, old_idx);
                void* dst = new_chunk->getComponentData(cid, new_idx);
                auto& ops = TypeRegistry::getOps(cid);

                if (ops.move_construct) {
                    ops.move_construct(dst, src);
                } else {
                    std::memcpy(dst, src, TypeRegistry::getTypeInfo(cid).size);
                }
            }

            // Remove the entity from the old archetype
            Entity moved = loc.archetype->removeEntity(old_chunk, old_idx);
            if (moved != kInvalidEntity) {
                entity_locations_[moved].chunk = old_chunk;
                entity_locations_[moved].index = old_idx;
            }
            entity_locations_[e] = { new_arch, new_chunk, new_idx };
        }

        /**
         * @brief Returns a pointer to the component T for the given entity, 
         *        or nullptr if the entity doesn't have that component.
         * 
         * @tparam T The component type to retrieve.
         * @param e The entity ID.
         * @return A pointer to T, or nullptr if not present.
         */
        template<typename T>
        T* getComponent(Entity e) {
            if (e >= entity_locations_.size()) return nullptr;
            auto& loc = entity_locations_[e];
            if (!loc.archetype) return nullptr;

            auto comp_id = TypeRegistry::getTypeID<T>();
            if (!loc.archetype->signature().test(comp_id)) {
                return nullptr;
            }
            return reinterpret_cast<T*>(loc.chunk->getComponentData(comp_id, loc.index));
        }

        /**
         * @brief Collects all entities that have at least the specified set of components (logical AND).
         * 
         * @tparam Components The list of required component types.
         * @return A vector containing all matching entity IDs.
         */
        template<typename... Components>
        std::vector<Entity> queryEntities() {
            Signature query_mask;
            (query_mask.set(TypeRegistry::getTypeID<Components>()), ...);

            std::vector<Entity> result;
            result.reserve(256); // assume some typical capacity

            for (auto& [sig, arch] : archetype_map_) {
                if (matchSignature(arch->signature(), query_mask)) {
                    // This Archetype matches all required components
                    for (auto& chunk : arch->getChunks()) {
                        size_t cnt = chunk->count();
                        for (size_t i = 0; i < cnt; ++i) {
                            result.push_back(chunk->getEntity(i));
                        }
                    }
                }
            }
            return result;
        }

    private:
        /**
         * @struct EntityLocation
         * @brief Stores the Archetype, Chunk, and index where a given entity is located.
         */
        struct EntityLocation {
            Archetype* archetype = nullptr;
            Chunk*     chunk     = nullptr;
            size_t     index     = 0;
        };

        /**
         * @brief Retrieves or creates an Archetype for the specified signature.
         * 
         * @param sig The bitmask signature to find or create.
         * @return A pointer to the Archetype.
         */
        Archetype* getOrCreateArchetype(const Signature& sig) {
            auto it = archetype_map_.find(sig);
            if (it != archetype_map_.end()) {
                return it->second;
            }
            auto arch_ptr = std::make_unique<Archetype>(sig);
            Archetype* raw = arch_ptr.get();
            archetypes_.push_back(std::move(arch_ptr));
            archetype_map_[sig] = raw;
            return raw;
        }

        EntityManager entity_manager_; ///< Manages entity ID allocation / recycling
        std::vector<EntityLocation> entity_locations_; ///< Maps each entity ID to its location (Archetype, Chunk, index)

        struct SignatureHash {
            size_t operator()(const Signature& sig) const noexcept {
                // For kMaxComponents <= 64, we can safely do to_ullong()
                return std::hash<unsigned long long>()(sig.to_ullong());
            }
        };

        /**
         * @brief Maps a signature to the Archetype pointer that handles 
         *        entities with that signature. 
         *        If no Archetype exists yet, one can be created.
         */
        std::unordered_map<Signature, Archetype*, SignatureHash> archetype_map_;

        std::vector<std::unique_ptr<Archetype>> archetypes_; ///< Owns the Archetype objects
    };

} // namespace lux::ecs
