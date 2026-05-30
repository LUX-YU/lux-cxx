#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

#include "Common.hpp"
#include "TypeRegistry.hpp"

namespace lux::cxx::archtype {

    class Archetype;
    class Chunk;

    /**
     * @struct CompInfo
     * @brief Per-archetype-cached component metadata. Hot loops touch only this
     *        struct — no static-array lookups via TypeRegistry — so the inner
     *        loop is base-pointer + integer math only.
     */
    struct CompInfo {
        ComponentTypeID type_id    = kInvalidComponentTypeID;
        std::uint32_t   offset     = 0;      ///< byte offset of this component array within a Chunk buffer
        std::uint32_t   size       = 0;      ///< sizeof(T)
        std::uint8_t    flags      = 0;      ///< copied from ComponentTypeInfo::flags
        void          (*move_and_destroy)(void*, void*) = nullptr; ///< nullptr => trivially relocatable
        void          (*destroy)(void*)                 = nullptr; ///< nullptr => trivially destructible
    };

    /**
     * @class Chunk
     * @brief A fixed-capacity SoA storage block. Holds entity ids plus one
     *        contiguous array per component, all packed into a single aligned
     *        heap allocation owned by the chunk.
     */
    class Chunk {
        friend class Archetype;
    public:
        explicit Chunk(Archetype* arch);
        ~Chunk();

        Chunk(const Chunk&)            = delete;
        Chunk& operator=(const Chunk&) = delete;

        bool          hasSpace() const noexcept { return count_ < kChunkSize; }
        std::uint32_t count()    const noexcept { return count_; }
        Archetype*    archetype() const noexcept { return archetype_; }

        Entity*       entities() noexcept { return reinterpret_cast<Entity*>(buffer_); }
        const Entity* entities() const noexcept { return reinterpret_cast<const Entity*>(buffer_); }
        Entity        getEntity(std::size_t i) const noexcept { return entities()[i]; }

        /// Pointer to the start of the component array for the i-th local component slot.
        void* comp_array_at_local(std::uint32_t local_idx) noexcept;

        /// Pointer to the (row)-th element of a given component (by archetype-local CompInfo).
        void* comp_ptr(const CompInfo& ci, std::size_t row) noexcept {
            return buffer_ + ci.offset + row * ci.size;
        }

        /// Pointer to the (row)-th element of the component whose ComponentTypeID is @p type_id.
        /// O(1) via the archetype's type_id -> local-index table.
        void* getComponentData(ComponentTypeID type_id, std::size_t row) noexcept;

        /// Allocate a new slot for @p e and return its row index.
        std::uint32_t allocateEntity(Entity e) noexcept;

        /// In-place destructor for the component data at @p row, leaving the slot uninitialized.
        void destroyComponentsAt(std::uint32_t row) noexcept;

        /// Compact the chunk after a slot was emptied (either via destroy or
        /// because data was moved into another archetype). Moves the last
        /// entity in to fill the slot if needed. Returns the moved entity or
        /// kInvalidEntity if no swap was performed.
        Entity compactAfterRemoval(std::uint32_t row) noexcept;

        std::int32_t chunk_index()  const noexcept { return chunk_index_; }
        std::int32_t free_list_index() const noexcept { return free_list_index_; }

    private:
        Archetype*   archetype_ = nullptr;
        char*        buffer_    = nullptr;
        std::uint32_t count_    = 0;
        std::int32_t chunk_index_     = -1;
        std::int32_t free_list_index_ = -1;
    };

    /**
     * @class Archetype
     * @brief All entities sharing the same component signature, stored in chunks.
     */
    class Archetype {
        friend class Chunk;
    public:
        Archetype(const Signature& sig, std::size_t maxFree = 32);
        ~Archetype();

        Archetype(const Archetype&)            = delete;
        Archetype& operator=(const Archetype&) = delete;

        /// Stable id assigned by Registry::getOrCreateArchetype = index into
        /// archetypes_. Stored in every EntityRecord so the chunk/row can be
        /// reached from just an EntityRecord via the registry's archetype list.
        std::uint32_t arch_id_ = kInvalidArchId;
        std::uint32_t archId() const noexcept { return arch_id_; }

        const Signature&             signature() const noexcept { return signature_; }
        const std::vector<CompInfo>& comps()     const noexcept { return comps_; }
        std::size_t                  size()      const noexcept { return entity_count_; }
        std::size_t                  chunkBufferSize() const noexcept { return chunk_buffer_size_; }
        std::size_t                  chunkAlign()      const noexcept { return chunk_align_; }

        /// O(1) lookup: ComponentTypeID -> index into comps_, or -1 if absent.
        std::int16_t localIndex(ComponentTypeID id) const noexcept {
            return comp_to_local_[id];
        }

        bool has(ComponentTypeID id) const noexcept { return comp_to_local_[id] >= 0; }

        const std::vector<std::unique_ptr<Chunk>>& getChunks() const noexcept { return chunks_; }
        std::vector<std::unique_ptr<Chunk>>&       getChunks()       noexcept { return chunks_; }

        /// Reserve a slot for @p e in some chunk (creating one if necessary).
        std::pair<Chunk*, std::uint32_t> addEntity(Entity e);

        /// Fully destroy the entity at (chunk, row): runs component destructors then compacts.
        Entity removeEntity(Chunk* chunk, std::uint32_t row) noexcept;

        /// Compact the chunk only — assumes the slot at row has already been
        /// emptied by a migration step. Returns the entity that was swapped in,
        /// if any.
        Entity removeEntityRaw(Chunk* chunk, std::uint32_t row) noexcept;

        // ---- archetype graph edges (O(1) cached transitions) ----
        Archetype* addEdge(ComponentTypeID id) const noexcept    { return add_edges_[id]; }
        Archetype* removeEdge(ComponentTypeID id) const noexcept { return remove_edges_[id]; }
        void setAddEdge(ComponentTypeID id, Archetype* a) noexcept    { add_edges_[id] = a; }
        void setRemoveEdge(ComponentTypeID id, Archetype* a) noexcept { remove_edges_[id] = a; }

        /// Release the heap buffer of every empty chunk in this archetype.
        /// The chunk objects stay in chunks_ as tombstones (buffer_ == nullptr)
        /// so chunk indices remain stable for existing EntityRecords. They
        /// will be reacquired by getOrCreateChunkWithSpace() if needed later.
        /// Returns the number of bytes freed.
        std::size_t compactEmptyChunks() noexcept;

    private:
        Chunk* getOrCreateChunkWithSpace();

        void onChunkBecameFull(Chunk* c) noexcept;
        void onChunkHasSpace(Chunk* c) noexcept;
        void onChunkEmpty(Chunk* c) noexcept;

        void addChunkToFreeList(Chunk* c) noexcept;
        void removeChunkFromFreeList(Chunk* c) noexcept;
        void destroyChunk(Chunk* c) noexcept;

        void computeLayout() noexcept;

    private:
        Signature              signature_;
        std::vector<CompInfo>  comps_;
        std::int16_t           comp_to_local_[kMaxComponents]; // -1 if absent

        std::vector<std::unique_ptr<Chunk>> chunks_;
        std::vector<Chunk*>                 free_chunks_;
        std::size_t            entity_count_     = 0;
        std::size_t            max_free_chunks_  = 32;

        std::size_t            chunk_buffer_size_ = 0;
        std::size_t            chunk_align_       = alignof(Entity);

        Archetype*             add_edges_[kMaxComponents]    = {};
        Archetype*             remove_edges_[kMaxComponents] = {};
    };

    //====================================================================
    //                       Archetype implementation
    //====================================================================

    inline Archetype::Archetype(const Signature& sig, std::size_t maxFree)
        : signature_(sig)
        , max_free_chunks_(maxFree)
    {
        std::fill(std::begin(comp_to_local_), std::end(comp_to_local_), std::int16_t{-1});

        // Build the CompInfo table by iterating set bits — typically 1..4 ids.
        for (std::size_t blk = 0; blk < kSignatureBlocks; ++blk) {
            std::uint64_t b = sig.bits[blk];
            while (b) {
                const std::uint32_t id =
                    static_cast<std::uint32_t>(blk * 64) +
                    static_cast<std::uint32_t>(std::countr_zero(b));
                b &= b - 1;

                const auto& info = TypeRegistry::getTypeInfo(static_cast<ComponentTypeID>(id));
                CompInfo ci;
                ci.type_id          = static_cast<ComponentTypeID>(id);
                ci.size             = info.size;
                ci.flags            = info.flags;
                ci.move_and_destroy = info.ops.move_and_destroy;
                ci.destroy          = info.ops.destroy;
                comp_to_local_[id]  = static_cast<std::int16_t>(comps_.size());
                comps_.push_back(ci);
            }
        }

        computeLayout();
    }

    inline Archetype::~Archetype() {
        // chunks_ owns the Chunks; their destructors run components' destructors and free buffers.
    }

    inline void Archetype::computeLayout() noexcept {
        std::size_t offset    = kChunkSize * sizeof(Entity);
        std::size_t max_align = alignof(Entity);

        for (auto& ci : comps_) {
            const std::size_t align = TypeRegistry::getTypeInfo(ci.type_id).alignment;
            offset      = (offset + align - 1) & ~(align - 1);
            ci.offset   = static_cast<std::uint32_t>(offset);
            offset     += static_cast<std::size_t>(ci.size) * kChunkSize;
            if (align > max_align) max_align = align;
        }
        chunk_buffer_size_ = offset;
        chunk_align_       = max_align;
    }

    inline std::pair<Chunk*, std::uint32_t> Archetype::addEntity(Entity e) {
        Chunk* chunk = getOrCreateChunkWithSpace();
        const std::uint32_t row = chunk->allocateEntity(e);
        ++entity_count_;
        return { chunk, row };
    }

    inline Entity Archetype::removeEntity(Chunk* chunk, std::uint32_t row) noexcept {
        chunk->destroyComponentsAt(row);
        return removeEntityRaw(chunk, row);
    }

    inline Entity Archetype::removeEntityRaw(Chunk* chunk, std::uint32_t row) noexcept {
        Entity moved = chunk->compactAfterRemoval(row);
        --entity_count_;
        if (chunk->count_ == 0) {
            onChunkEmpty(chunk);
        }
        return moved;
    }

    inline Chunk* Archetype::getOrCreateChunkWithSpace() {
        if (!free_chunks_.empty()) return free_chunks_.back();

        // Try to reacquire a tombstone (chunk with released buffer).
        // We don't track tombstones in a separate index — they are rare and
        // only walked here on the cold path of "free list is empty".
        for (auto& cup : chunks_) {
            Chunk* c = cup.get();
            if (c->buffer_ == nullptr) {
                c->buffer_ = static_cast<char*>(::operator new[](
                    chunk_buffer_size_,
                    std::align_val_t(chunk_align_)));
                addChunkToFreeList(c);
                return c;
            }
        }

        auto up = std::make_unique<Chunk>(this);
        Chunk* raw = up.get();
        raw->chunk_index_ = static_cast<std::int32_t>(chunks_.size());
        chunks_.push_back(std::move(up));
        addChunkToFreeList(raw);
        return raw;
    }

    inline std::size_t Archetype::compactEmptyChunks() noexcept {
        std::size_t freed = 0;
        for (auto& cup : chunks_) {
            Chunk* c = cup.get();
            if (c->count_ == 0 && c->buffer_ != nullptr) {
                ::operator delete[](c->buffer_, std::align_val_t(chunk_align_));
                c->buffer_ = nullptr;
                removeChunkFromFreeList(c);
                freed += chunk_buffer_size_;
            }
        }
        return freed;
    }

    inline void Archetype::onChunkBecameFull(Chunk* c) noexcept {
        removeChunkFromFreeList(c);
    }

    inline void Archetype::onChunkHasSpace(Chunk* c) noexcept {
        addChunkToFreeList(c);
    }

    inline void Archetype::onChunkEmpty(Chunk* /*c*/) noexcept {
        // No-op. Empty chunks stay in chunks_ for stable chunk indexing —
        // EntityRecord packs (arch_id, chunk_idx, idx_in_chunk) and relies
        // on chunk_idx never being reassigned. They sit on the free list
        // ready to host new entities. (max_free_chunks_ now only affects
        // peak memory; we never actually destroy chunks during normal
        // operation. registry.compact() can be added later for that.)
    }

    inline void Archetype::addChunkToFreeList(Chunk* c) noexcept {
        if (c->free_list_index_ != -1) return;
        c->free_list_index_ = static_cast<std::int32_t>(free_chunks_.size());
        free_chunks_.push_back(c);
    }

    inline void Archetype::removeChunkFromFreeList(Chunk* c) noexcept {
        const std::int32_t idx = c->free_list_index_;
        if (idx == -1) return;
        const std::int32_t last = static_cast<std::int32_t>(free_chunks_.size() - 1);
        if (idx != last) {
            free_chunks_[idx] = free_chunks_[last];
            free_chunks_[idx]->free_list_index_ = idx;
        }
        free_chunks_.pop_back();
        c->free_list_index_ = -1;
    }

    inline void Archetype::destroyChunk(Chunk* c) noexcept {
        removeChunkFromFreeList(c);
        const std::int32_t idx = c->chunk_index_;
        const std::int32_t last = static_cast<std::int32_t>(chunks_.size() - 1);
        if (idx != last) {
            chunks_[idx] = std::move(chunks_[last]);
            chunks_[idx]->chunk_index_ = idx;
        }
        chunks_.pop_back();
    }

    //====================================================================
    //                          Chunk implementation
    //====================================================================

    inline Chunk::Chunk(Archetype* arch)
        : archetype_(arch)
    {
        buffer_ = static_cast<char*>(::operator new[](
            arch->chunkBufferSize(),
            std::align_val_t(arch->chunkAlign())));
    }

    inline Chunk::~Chunk() {
        // Run destructors for any live component instances.
        for (std::uint32_t i = 0; i < count_; ++i) {
            for (auto& ci : archetype_->comps_) {
                if (ci.destroy) {
                    ci.destroy(buffer_ + ci.offset + i * ci.size);
                }
            }
        }
        if (buffer_) {
            ::operator delete[](buffer_, std::align_val_t(archetype_->chunkAlign()));
        }
    }

    inline void* Chunk::comp_array_at_local(std::uint32_t local_idx) noexcept {
        return buffer_ + archetype_->comps_[local_idx].offset;
    }

    inline void* Chunk::getComponentData(ComponentTypeID type_id, std::size_t row) noexcept {
        const std::int16_t local = archetype_->comp_to_local_[type_id];
        const auto& ci = archetype_->comps_[local];
        return buffer_ + ci.offset + row * ci.size;
    }

    inline std::uint32_t Chunk::allocateEntity(Entity e) noexcept {
        const std::uint32_t row = count_++;
        entities()[row] = e;
        if (count_ == kChunkSize) {
            archetype_->onChunkBecameFull(this);
        }
        return row;
    }

    inline void Chunk::destroyComponentsAt(std::uint32_t row) noexcept {
        for (auto& ci : archetype_->comps_) {
            if (ci.destroy) {
                ci.destroy(buffer_ + ci.offset + row * ci.size);
            }
        }
    }

    inline Entity Chunk::compactAfterRemoval(std::uint32_t row) noexcept {
        const std::uint32_t last = count_ - 1;
        Entity moved = kInvalidEntity;
        if (row != last) {
            moved = entities()[last];
            for (auto& ci : archetype_->comps_) {
                if (ci.size == 0) continue; // tag — nothing to move
                char* dst = buffer_ + ci.offset + row * ci.size;
                char* src = buffer_ + ci.offset + last * ci.size;
                if (ci.move_and_destroy == nullptr) {
                    // trivially relocatable: bit copy is enough
                    std::memcpy(dst, src, ci.size);
                } else {
                    ci.move_and_destroy(dst, src);
                }
            }
            entities()[row] = moved;
        }
        --count_;
        if (count_ == kChunkSize - 1) {
            archetype_->onChunkHasSpace(this);
        }
        return moved;
    }

} // namespace lux::cxx::archtype
