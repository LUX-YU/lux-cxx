#pragma once
#include <algorithm>
#include <memory>
#include <vector>
#include <new>
#include <cstring>
#include <cassert>
#include "Common.hpp"
#include "TypeRegistry.hpp"

namespace lux::cxx::archtype {

    class Chunk; // forward declaration

    /**
     * @class Archetype
     * @brief Represents a specific combination of component types.
     *        All entities that share the same component set (signature)
     *        are stored in this Archetype. Internally, it manages
     *        multiple Chunk objects for actual data storage.
     */
    class Archetype {
        friend class Chunk;

    public:
        /**
         * @param sig The bitmask signature describing the set of components in this Archetype.
         * @param maxFree The maximum number of chunks we allow in the "free" list
         *        before we start removing empty chunks from the system.
         */
        Archetype(const Signature& sig, std::size_t maxFree = 32);

        /**
         * @brief Destroys the Archetype, releasing owned Chunks.
         */
        ~Archetype();

        /**
         * @brief Allocates space for an entity in one of the not-full Chunks.
         *        If no chunk is available or all are full, a new one is created.
         */
        std::pair<Chunk*, size_t> addEntity(Entity e);

        /**
         * @brief Retrieves or creates a Chunk that has space for at least one more Entity.
         *        This operation is O(1) by using a dedicated free list.
         */
        Chunk* getOrCreateChunkWithSpace();

        /**
         * @brief Removes an entity from the specified Chunk. This might move another
         *        entity from the last slot to fill the gap.
         *        If the Chunk becomes empty, we either keep it around if our free list
         *        is under capacity, or we remove and delete it.
         */
        Entity removeEntity(Chunk* chunk, size_t index);

        /**
         * @brief Returns the total number of entities stored in this Archetype.
         */
        size_t size() const { return entity_count_; }

        /**
         * @brief Access all Chunks managed by this Archetype.
         */
        const std::vector<std::unique_ptr<Chunk>>& getChunks() const { return chunks_; }

        /**
         * @brief Returns a reference to the component types in this Archetype.
         */
        const std::vector<ComponentTypeID>& getComponentTypes() const { return component_types_; }

        /**
         * @brief Returns the signature for this Archetype.
         */
        const Signature& signature() const { return signature_; }

        /**
         * @brief Retrieves a reference to component type T
         *        for an entity in the specified Chunk at a given index.
         */
        template<typename T>
        T& getComponent(Chunk* chunk, size_t index);

    private:
        /**
         * @brief Invoked by a Chunk when it transitions from not-full to full.
         *        We remove it from the free list in O(1).
         */
        void onChunkBecameFull(Chunk* c);

        /**
         * @brief Invoked by a Chunk when it transitions from full to not-full.
         *        We add it to the free list in O(1).
         */
        void onChunkHasSpace(Chunk* c);

        /**
         * @brief O(1) removal of a Chunk from the 'chunks_' vector via swap-and-pop.
         */
        void removeChunkFromAllList(Chunk* c);

        /**
         * @brief O(1) removal of a Chunk from the 'free_chunks_' vector via swap-and-pop.
         */
        void removeChunkFromFreeList(Chunk* c);

        /**
         * @brief O(1) insertion of a Chunk pointer into the free list.
         */
        void addChunkToFreeList(Chunk* c);

    private:
        Signature signature_;                       ///< Bitmask representing the combination of components.
        std::vector<ComponentTypeID> component_types_; ///< List of component type IDs in this Archetype.
        int comp_index_map_[kMaxComponents];        ///< Maps each component type ID to an index in 'component_types_'.

        // Store all Chunk objects as unique_ptr. This owns the memory.
        std::vector<std::unique_ptr<Chunk>> chunks_;

        // We keep pointers to Chunks that are not full (have space).
        // This allows O(1) retrieval of a chunk that can store a new entity.
        std::vector<Chunk*> free_chunks_;

        // The total count of entities in this Archetype.
        size_t entity_count_;

        // Maximum number of free chunks we allow before we remove an empty chunk from the system.
        size_t max_free_chunks_;
    };


    /**
     * @class Chunk
     * @brief Stores a subset of Entities in a contiguous memory block, along with their components.
     */
    class Chunk {
    public:
        explicit Chunk(Archetype* arch);
        ~Chunk();

        /**
         * @brief Allocates space in this Chunk for a new entity.
         *        Returns the index where the entity was placed.
         */
        size_t allocateEntity(Entity e);

        /**
         * @brief Removes the entity at 'index', possibly swapping in the last entity
         *        to fill the gap. Returns the Entity ID that was moved, or kInvalidEntity.
         */
        Entity removeEntityAt(size_t index);

        /**
         * @brief Whether this Chunk is not full (i.e. can store more entities).
         */
        bool hasSpace() const { return count_ < kChunkSize; }

        /**
         * @brief Returns how many entities are currently stored.
         */
        size_t count() const { return count_; }

        /**
         * @brief Retrieves the Entity ID at a particular index.
         */
        Entity getEntity(size_t index) const { return entity_ids_[index]; }

        /**
         * @brief Returns the raw pointer to a component within this Chunk.
         */
        void* getComponentData(ComponentTypeID compId, size_t index);

        /**
         * @brief The index of this Chunk in Archetype::chunks_.
         */
        int chunk_index_ = -1;

        /**
         * @brief The index of this Chunk in Archetype::free_chunks_, or -1 if not present there.
         */
        int free_list_index_ = -1;

    private:
        /**
         * @brief Allocates the memory layout for entities plus each component array.
         */
        void allocateData();

        Archetype* archetype_ = nullptr;
        size_t count_ = 0;   ///< Number of active entities in this Chunk

        char* data_buffer_ = nullptr;  ///< Raw memory buffer for entity IDs and component data
        size_t total_size_ = 0;
        size_t max_align_ = 0;

        Entity* entity_ids_ = nullptr;
        std::vector<void*> component_data_;
    };


    //====================================================
    //             Archetype Implementation
    //====================================================

    inline Archetype::Archetype(const Signature& sig, std::size_t maxFree)
        : signature_(sig)
        , entity_count_(0)
        , max_free_chunks_(maxFree)
    {
        for (ComponentTypeID id = 0; id < kMaxComponents; ++id) {
            if (sig.test(id)) {
                component_types_.push_back(id);
            }
        }
        std::fill(std::begin(comp_index_map_), std::end(comp_index_map_), -1);
        for (size_t i = 0; i < component_types_.size(); ++i) {
            comp_index_map_[component_types_[i]] = static_cast<int>(i);
        }
    }

    inline Archetype::~Archetype() {
        // We rely on unique_ptr to destroy all Chunks automatically.
        // No manual deletion needed here.
    }

    inline std::pair<Chunk*, size_t> Archetype::addEntity(Entity e) {
        Chunk* chunk = getOrCreateChunkWithSpace();
        size_t idx = chunk->allocateEntity(e);
        entity_count_++;
        return { chunk, idx };
    }

    inline Chunk* Archetype::getOrCreateChunkWithSpace() {
        // If we have any chunk in the free list, return the last one in O(1).
        if (!free_chunks_.empty()) {
            return free_chunks_.back();
        }
        // Otherwise, create a new Chunk and store it.
        auto new_chunk = std::make_unique<Chunk>(this);
        Chunk* ptr = new_chunk.get();
        ptr->chunk_index_ = static_cast<int>(chunks_.size());
        chunks_.push_back(std::move(new_chunk));

        // This new chunk definitely has space, so add it to the free list.
        addChunkToFreeList(ptr);
        return ptr;
    }

    inline Entity Archetype::removeEntity(Chunk* chunk, size_t index) {
        Entity moved = chunk->removeEntityAt(index);

        // If chunk is now empty, decide whether to keep it as a free chunk or remove it.
        if (chunk->count() == 0) {
            // If we have not exceeded max_free_chunks_, we simply add it (or keep it) in the free list
            // so that it can be reused. If it's already in the free list and is now empty, no problem.
            // If we already have enough free chunks, remove it from all lists.
            if (free_chunks_.size() >= max_free_chunks_) {
                removeChunkFromAllList(chunk); // This releases the unique_ptr ownership
                // The chunk is destroyed here. We do not keep empty chunk beyond the limit.
            }
            else {
                // If it is not already in the free list, add it. If it is in the free list, that's fine.
                addChunkToFreeList(chunk);
            }
        }

        entity_count_--;
        return moved;
    }

    template<typename T>
    inline T& Archetype::getComponent(Chunk* chunk, size_t index) {
        auto comp_id = TypeRegistry::getTypeID<T>();
        void* ptr = chunk->getComponentData(comp_id, index);
        return *reinterpret_cast<T*>(ptr);
    }

    inline void Archetype::onChunkBecameFull(Chunk* c) {
        // We remove it from the free list in O(1), because it no longer has space.
        removeChunkFromFreeList(c);
    }

    inline void Archetype::onChunkHasSpace(Chunk* c) {
        // We add it to the free list in O(1), since it has space again.
        addChunkToFreeList(c);
    }

    inline void Archetype::removeChunkFromAllList(Chunk* c) {
        // O(1) swap-and-pop in chunks_
        int idx = c->chunk_index_;
        int last = static_cast<int>(chunks_.size() - 1);
        if (idx != last) {
            std::swap(chunks_[idx], chunks_[last]);
            // Fix the chunk_index_ of the chunk that was swapped in
            chunks_[idx]->chunk_index_ = idx;
        }
        // Also remove from free_chunks_ if it's in there.
        removeChunkFromFreeList(c);

        // Popping back will delete the Chunk if it was the last unique_ptr.
        chunks_.pop_back();
    }

    inline void Archetype::removeChunkFromFreeList(Chunk* c) {
        // If c->free_list_index_ == -1, it means it's not in free_chunks_.
        int indexInFree = c->free_list_index_;
        if (indexInFree == -1) {
            return;
        }
        int last = static_cast<int>(free_chunks_.size() - 1);
        if (indexInFree != last) {
            std::swap(free_chunks_[indexInFree], free_chunks_[last]);
            free_chunks_[indexInFree]->free_list_index_ = indexInFree;
        }
        free_chunks_.pop_back();
        c->free_list_index_ = -1;
    }

    inline void Archetype::addChunkToFreeList(Chunk* c) {
        // If the chunk is already in the list, do nothing.
        if (c->free_list_index_ != -1) {
            return;
        }
        c->free_list_index_ = static_cast<int>(free_chunks_.size());
        free_chunks_.push_back(c);
    }


    //====================================================
    //               Chunk Implementation
    //====================================================

    inline Chunk::Chunk(Archetype* arch)
        : archetype_(arch)
    {
        allocateData();
    }

    inline Chunk::~Chunk() {
        // Destroy all active component instances
        for (size_t i = 0; i < count_; ++i) {
            for (auto compId : archetype_->component_types_) {
                const auto& ops = TypeRegistry::getOps(compId);
                char* ptr = static_cast<char*>(component_data_[archetype_->comp_index_map_[compId]])
                    + i * TypeRegistry::getTypeInfo(compId).size;
                if (ops.destroy) {
                    ops.destroy(ptr);
                }
            }
        }
        // Free the raw memory buffer
        ::operator delete[](data_buffer_, std::align_val_t(max_align_));
    }

    inline size_t Chunk::allocateEntity(Entity e) {
        // Place the entity in the next free slot
        size_t index = count_++;
        entity_ids_[index] = e;

        // If this chunk just became full, remove it from free list
        if (count_ == kChunkSize) {
            archetype_->onChunkBecameFull(this);
        }
        return index;
    }

    inline Entity Chunk::removeEntityAt(size_t index) {
        if (index >= count_) {
            return kInvalidEntity;
        }
        size_t last_index = count_ - 1;
        Entity moved_entity = kInvalidEntity;

        if (index != last_index) {
            // Move data from the last entity to fill the removed slot
            moved_entity = entity_ids_[last_index];
            for (auto compId : archetype_->component_types_) {
                size_t cindex = archetype_->comp_index_map_[compId];
                size_t sz = TypeRegistry::getTypeInfo(compId).size;

                void* dest_ptr = static_cast<char*>(component_data_[cindex]) + index * sz;
                void* src_ptr = static_cast<char*>(component_data_[cindex]) + last_index * sz;

                const auto& ops = TypeRegistry::getOps(compId);
                if (ops.destroy) {
                    ops.destroy(dest_ptr);
                }
                if (ops.move_construct) {
                    ops.move_construct(dest_ptr, src_ptr);
                }
                else {
                    std::memcpy(dest_ptr, src_ptr, sz);
                }
                if (ops.destroy) {
                    ops.destroy(src_ptr);
                }
            }
            entity_ids_[index] = entity_ids_[last_index];
        }
        else {
            // Just destroy the last entity's data
            for (auto compId : archetype_->component_types_) {
                size_t cindex = archetype_->comp_index_map_[compId];
                void* src_ptr = static_cast<char*>(component_data_[cindex])
                    + last_index * TypeRegistry::getTypeInfo(compId).size;
                const auto& ops = TypeRegistry::getOps(compId);
                if (ops.destroy) {
                    ops.destroy(src_ptr);
                }
            }
        }
        count_--;

        // If this chunk was full and now became not-full, add it to free list
        if (count_ == kChunkSize - 1) {
            archetype_->onChunkHasSpace(this);
        }
        return moved_entity;
    }

    inline void* Chunk::getComponentData(ComponentTypeID compId, size_t index) {
        int comp_index = archetype_->comp_index_map_[compId];
        char* base = static_cast<char*>(component_data_[comp_index]);
        return base + index * TypeRegistry::getTypeInfo(compId).size;
    }

    inline void Chunk::allocateData() {
        // We need space for:
        //   1) An array of Entity IDs
        //   2) Arrays for each component type (contiguous)
        size_t comp_count = archetype_->component_types_.size();
        size_t total_arrays = comp_count + 1;

        std::vector<size_t> offsets(total_arrays);
        std::vector<size_t> aligns(total_arrays);

        // First array is entity_ids_
        offsets[0] = 0;
        aligns[0] = alignof(Entity);
        size_t offset = kChunkSize * sizeof(Entity);
        size_t maxAlign = aligns[0];

        for (size_t i = 0; i < comp_count; ++i) {
            auto cid   = archetype_->component_types_[i];
            size_t alg = TypeRegistry::getTypeInfo(cid).alignment;
            size_t sz  = TypeRegistry::getTypeInfo(cid).size;

            // Align offset
            offset = (offset + alg - 1) & ~(alg - 1);
            offsets[i + 1] = offset;
            aligns[i + 1] = alg;
            if (alg > maxAlign) {
                maxAlign = alg;
            }
            offset += sz * kChunkSize;
        }
        total_size_ = offset;
        max_align_ = maxAlign;

        // Allocate raw buffer
        data_buffer_ = reinterpret_cast<char*>(
            ::operator new[](total_size_, std::align_val_t(max_align_))
            );

        // First array is for entity IDs
        entity_ids_ = reinterpret_cast<Entity*>(data_buffer_ + offsets[0]);

        // For each component, store its start pointer
        component_data_.resize(comp_count);
        for (size_t i = 0; i < comp_count; ++i) {
            component_data_[i] = data_buffer_ + offsets[i + 1];
        }
    }

} // namespace lux::ecs
