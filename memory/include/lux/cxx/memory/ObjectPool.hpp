#pragma once
/**
 * @file ObjectPool.hpp
 * @brief High-performance object pool with optional lock-free thread safety.
 *
 * Provides O(1) acquire/release of objects from a chunk-based memory pool.
 * Pointers returned by @c acquire() remain stable until @c release() is called
 * — the pool never relocates live objects.
 *
 * Two thread-safety modes are available via the @c ThreadSafe template parameter:
 * - @c false (default): zero-overhead single-threaded path (no atomics).
 * - @c true : genuinely lock-free acquire/release. The free list is a Treiber
 *   stack whose head is a single 64-bit atomic holding a 32-bit slot index plus
 *   a 32-bit ABA tag, so the fast path is exactly one CAS and is always
 *   lock-free on the target. The only place a lock is taken is when the pool has
 *   to allocate a brand-new chunk (rare, and @c ::operator new is itself
 *   blocking), serialised by @c grow_mutex_.
 *
 * @copyright
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR
 * A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include <mutex>

namespace lux::cxx
{
    // Forward declaration
    template <typename T, std::size_t ChunkSize, bool ThreadSafe>
    class ObjectPool;

    // ═══════════════════════════════ pool_ptr ════════════════════════════════

    /**
     * @brief RAII handle that automatically returns an object to its pool on destruction.
     *
     * Semantics are similar to @c std::unique_ptr with a custom deleter, but the
     * deleter is embedded as a back-pointer to the owning pool rather than a
     * type-erased function — keeping @c sizeof(pool_ptr) == 2 pointers.
     *
     * @tparam T          Element type.
     * @tparam ChunkSize  Chunk size of the owning pool.
     * @tparam ThreadSafe Whether the owning pool is thread-safe.
     */
    template <typename T, std::size_t ChunkSize = 64, bool ThreadSafe = false>
    class pool_ptr
    {
    public:
        using pool_type = ObjectPool<T, ChunkSize, ThreadSafe>;

        /** @brief Constructs a null pool_ptr. */
        constexpr pool_ptr() noexcept : ptr_(nullptr), pool_(nullptr) {}
        constexpr pool_ptr(std::nullptr_t) noexcept : ptr_(nullptr), pool_(nullptr) {}

        /** @brief Constructs a pool_ptr that owns @p ptr and will return it to @p pool. */
        pool_ptr(T* ptr, pool_type* pool) noexcept : ptr_(ptr), pool_(pool) {}

        ~pool_ptr()
        {
            if (ptr_) pool_->release(ptr_);
        }

        // Move-only
        pool_ptr(const pool_ptr&) = delete;
        pool_ptr& operator=(const pool_ptr&) = delete;

        pool_ptr(pool_ptr&& other) noexcept
            : ptr_(other.ptr_), pool_(other.pool_)
        {
            other.ptr_  = nullptr;
            other.pool_ = nullptr;
        }

        pool_ptr& operator=(pool_ptr&& other) noexcept
        {
            if (this != &other)
            {
                if (ptr_) pool_->release(ptr_);
                ptr_        = other.ptr_;
                pool_       = other.pool_;
                other.ptr_  = nullptr;
                other.pool_ = nullptr;
            }
            return *this;
        }

        /** @brief Releases ownership and returns the raw pointer. The caller is
         *         responsible for eventually calling @c pool.release(). */
        [[nodiscard]] T* release() noexcept
        {
            T* p = ptr_;
            ptr_  = nullptr;
            pool_ = nullptr;
            return p;
        }

        /** @brief Releases the current object back to the pool (if any) and resets to null. */
        void reset() noexcept
        {
            if (ptr_)
            {
                pool_->release(ptr_);
                ptr_  = nullptr;
                pool_ = nullptr;
            }
        }

        // ─── observers ──────────────────────────────────────────────────

        [[nodiscard]] T*       get()       noexcept { return ptr_; }
        [[nodiscard]] const T* get() const noexcept { return ptr_; }

        T& operator*()  const noexcept { return *ptr_; }
        T* operator->() const noexcept { return  ptr_; }

        explicit operator bool() const noexcept { return ptr_ != nullptr; }

    private:
        T*         ptr_;
        pool_type* pool_;
    };

    // ═══════════════════════════════ ObjectPool ══════════════════════════════

    // ─── implementation detail: free-list nodes ─────────────────────────────

    namespace detail
    {
        /**
         * @brief Node stored in-place inside a free slot (single-threaded path).
         *
         * When a slot is free, its storage is reinterpreted as a @c FreeNode
         * that points to the next free slot. This gives zero extra memory
         * overhead for the free list. Only used by the non-thread-safe pool.
         */
        struct FreeNode
        {
            FreeNode* next;
        };

        /**
         * @brief Per-slot header used by the lock-free pool path.
         *
         * Unlike the single-threaded @c FreeNode (which overlays the object's
         * storage), this header lives in its own region of the slot, disjoint
         * from the @c T storage. That is what makes the lock-free path correct:
         *
         * - @c self is the slot's global index, written once at chunk creation
         *   and read in @c release() to recover the index from a @c T* in O(1).
         * - @c next is the free-list link. Because it never overlaps the live
         *   @c T object, reading another slot's @c next during a pop is neither
         *   a type-punning violation nor a data race with user code; making it
         *   @c std::atomic also keeps the (benign, ABA-tag-guarded) concurrent
         *   read/write during a CAS window well defined.
         */
        struct SlotHeaderLF
        {
            std::uint32_t              self;
            std::atomic<std::uint32_t> next;
        };

    } // namespace detail

    /**
     * @class ObjectPool
     * @brief Chunk-based object pool with O(1) acquire and release.
     *
     * Memory is organised in fixed-size chunks that are never moved or
     * reallocated, so pointers returned by @c acquire() are stable until
     * the corresponding @c release() call (or pool destruction).
     *
     * @par Single-threaded mode (`ThreadSafe = false`)
     * Uses a plain intrusive free list — no atomic operations at all.
     *
     * @par Lock-free mode (`ThreadSafe = true`)
     * The free list is a Treiber stack whose head is a single 64-bit atomic
     * packing { 32-bit slot index, 32-bit ABA tag }. @c acquire() / @c release()
     * are one CAS each on the fast path and are always lock-free. The free-list
     * link of each slot lives in a separate header (@c detail::SlotHeaderLF),
     * disjoint from the object storage, so no interior free-list node is ever
     * read while it may simultaneously hold a live @c T. The only lock taken is
     * @c grow_mutex_, and only to allocate a fresh chunk.
     *
     * @tparam T          Element type. Must satisfy @c std::is_destructible.
     * @tparam ChunkSize  Number of elements per chunk (default 64).
     * @tparam ThreadSafe If true, acquire/release are safe to call from
     *                     multiple threads concurrently.
     */
    template <
        typename T,
        std::size_t ChunkSize  = 64,
        bool        ThreadSafe = false
    >
    class ObjectPool
    {
        static_assert(ChunkSize > 0, "ObjectPool: ChunkSize must be > 0");
        static_assert(std::is_destructible_v<T>, "ObjectPool: T must be destructible");
        static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                      "ObjectPool: 64-bit atomic is not lock-free on this platform");

    public:
        using value_type = T;
        using pointer    = T*;
        using size_type  = std::size_t;

        /// Default upper bound on the number of chunks a lock-free pool may
        /// allocate. Total capacity is bounded by @c max_chunks * ChunkSize and
        /// must stay below 2^32 (the slot index is 32-bit). Override via the
        /// constructor's @p max_chunks argument.
        static constexpr size_type kDefaultMaxChunks = 1024;

        // ─── construction / destruction ─────────────────────────────────

        /**
         * @brief Constructs a pool, optionally pre-allocating capacity.
         *
         * @param initial_capacity If non-zero, pre-allocate enough chunks to hold
         *                          at least this many objects.
         * @param max_chunks        (lock-free mode only) hard cap on chunk count;
         *                          @c grow throws @c std::length_error past it.
         *                          Ignored in single-threaded mode.
         */
        explicit ObjectPool(size_type initial_capacity = 0,
                            size_type max_chunks = kDefaultMaxChunks)
        {
            if constexpr (ThreadSafe)
            {
                max_chunks_ = max_chunks ? max_chunks : kDefaultMaxChunks;
                // The slot index is 32-bit; clamp so index space never overflows.
                if (max_chunks_ > (NULL_IDX / ChunkSize))
                    max_chunks_ = NULL_IDX / ChunkSize;
                chunk_bases_ = std::make_unique<std::atomic<std::byte*>[]>(max_chunks_);
            }
            if (initial_capacity)
                reserve(initial_capacity);
        }

        /** @brief Destroys the pool and all chunks.
         *
         *  @warning Any live objects that have not been released are *not*
         *           destroyed — that is the caller's responsibility (or use
         *           @c pool_ptr for automatic lifetime management).
         *           The raw memory is freed regardless.
         */
        ~ObjectPool()
        {
            if constexpr (ThreadSafe)
            {
                const size_type n = chunk_count_.load(std::memory_order_relaxed);
                for (size_type i = 0; i < n; ++i)
                {
                    std::byte* base = chunk_bases_[i].load(std::memory_order_relaxed);
                    ::operator delete(base, std::align_val_t{LF_SLOT_ALIGN});
                }
            }
            else
            {
                for (auto* chunk : chunks_)
                    ::operator delete(chunk, std::align_val_t{SLOT_ALIGN});
            }
        }

        // Non-copyable, non-movable (users hold pointers into us).
        ObjectPool(const ObjectPool&)            = delete;
        ObjectPool& operator=(const ObjectPool&) = delete;
        ObjectPool(ObjectPool&&)                 = delete;
        ObjectPool& operator=(ObjectPool&&)      = delete;

        // ─── core operations ────────────────────────────────────────────

        /**
         * @brief Acquires a slot and constructs a @c T in-place.
         *
         * @tparam Args  Constructor argument types.
         * @param  args  Arguments forwarded to @c T's constructor.
         * @return Pointer to the newly constructed object.
         *
         * Complexity: O(1) amortised. May allocate a new chunk if the free list
         * is empty (chunk allocation is O(ChunkSize)).
         */
        template <typename... Args>
        [[nodiscard]] pointer acquire(Args&&... args)
        {
            void* slot = allocate_slot();
            return new (slot) T(std::forward<Args>(args)...);
        }

        /**
         * @brief Destroys the object at @p ptr and returns the slot to the pool.
         *
         * @param ptr Pointer previously returned by @c acquire().
         *            Passing @c nullptr is a no-op.
         *
         * @warning Passing a pointer not owned by this pool, or releasing the
         *          same pointer twice, is undefined behaviour.
         */
        void release(pointer ptr) noexcept
        {
            if (!ptr) return;
            ptr->~T();
            deallocate_slot(ptr);
        }

        /**
         * @brief Acquires a slot, constructs a @c T, and wraps it in a @c pool_ptr.
         *
         * @tparam Args  Constructor argument types.
         * @param  args  Arguments forwarded to @c T's constructor.
         * @return A @c pool_ptr that will automatically call @c release() on destruction.
         */
        template <typename... Args>
        [[nodiscard]] pool_ptr<T, ChunkSize, ThreadSafe> acquire_scoped(Args&&... args)
        {
            return pool_ptr<T, ChunkSize, ThreadSafe>(
                acquire(std::forward<Args>(args)...), this);
        }

        // ─── capacity ───────────────────────────────────────────────────

        /**
         * @brief Pre-allocates enough chunks so that at least @p n objects
         *        can be acquired without further allocation.
         */
        void reserve(size_type n)
        {
            if constexpr (ThreadSafe)
            {
                std::lock_guard<std::mutex> lock(grow_mutex_);
                while (chunk_count_.load(std::memory_order_relaxed) * ChunkSize < n)
                    grow_lf();
            }
            else
            {
                while (capacity_ < n)
                    grow();
            }
        }

        /**
         * @brief Returns the total number of slots (free + in-use) across all chunks.
         */
        [[nodiscard]] size_type capacity() const noexcept
        {
            if constexpr (ThreadSafe)
                return chunk_count_.load(std::memory_order_acquire) * ChunkSize;
            else
                return capacity_;
        }

        /**
         * @brief Returns the number of currently allocated chunks.
         */
        [[nodiscard]] size_type chunk_count() const noexcept
        {
            if constexpr (ThreadSafe)
                return chunk_count_.load(std::memory_order_acquire);
            else
                return chunks_.size();
        }

    private:
        // ═══ Single-threaded slot storage / free list ═══════════════════

        /**
         * @brief Slot size/alignment for the single-threaded path. When *free*,
         *        a slot's bytes are reinterpreted as @c detail::FreeNode; when
         *        *alive*, the slot holds a live @c T.
         */
        static constexpr size_type SLOT_SIZE  = sizeof(T)  < sizeof(detail::FreeNode)
                                              ? sizeof(detail::FreeNode) : sizeof(T);
        static constexpr size_type SLOT_ALIGN = alignof(T) < alignof(detail::FreeNode)
                                              ? alignof(detail::FreeNode) : alignof(T);

        void* allocate_slot_st()
        {
            if (!free_head_st_)
                grow();

            detail::FreeNode* node = free_head_st_;
            free_head_st_ = node->next;
            return static_cast<void*>(node);
        }

        void deallocate_slot_st(void* ptr) noexcept
        {
            auto* node  = static_cast<detail::FreeNode*>(ptr);
            node->next  = free_head_st_;
            free_head_st_ = node;
        }

        // ═══ Lock-free slot storage / free list ═════════════════════════

        static constexpr std::uint32_t NULL_IDX = 0xFFFFFFFFu; ///< empty/end sentinel

        /// Slot layout (lock-free): [ SlotHeaderLF ][ pad ][ T storage ].
        /// The header is disjoint from the T storage so the free-list link is
        /// never aliased by a live object.
        static constexpr size_type LF_HEADER_SIZE =
            sizeof(detail::SlotHeaderLF);
        static constexpr size_type LF_SLOT_ALIGN =
            alignof(detail::SlotHeaderLF) < alignof(T) ? alignof(T)
                                                       : alignof(detail::SlotHeaderLF);
        static constexpr size_type LF_STORAGE_OFFSET =
            ((LF_HEADER_SIZE + alignof(T) - 1) / alignof(T)) * alignof(T);
        static constexpr size_type LF_SLOT_STRIDE =
            ((LF_STORAGE_OFFSET + sizeof(T) + LF_SLOT_ALIGN - 1) / LF_SLOT_ALIGN) * LF_SLOT_ALIGN;

        static constexpr std::uint64_t pack(std::uint32_t idx, std::uint32_t tag) noexcept
        {
            return (static_cast<std::uint64_t>(tag) << 32) | idx;
        }
        static constexpr std::uint32_t lo(std::uint64_t v) noexcept
        {
            return static_cast<std::uint32_t>(v & 0xFFFFFFFFu);
        }
        static constexpr std::uint32_t hi(std::uint64_t v) noexcept
        {
            return static_cast<std::uint32_t>(v >> 32);
        }

        detail::SlotHeaderLF* header_at(std::uint32_t idx) const noexcept
        {
            const size_type cid = idx / ChunkSize;
            const size_type off = idx % ChunkSize;
            std::byte* base = chunk_bases_[cid].load(std::memory_order_acquire);
            return reinterpret_cast<detail::SlotHeaderLF*>(base + off * LF_SLOT_STRIDE);
        }

        void* storage_addr(std::uint32_t idx) const noexcept
        {
            return reinterpret_cast<std::byte*>(header_at(idx)) + LF_STORAGE_OFFSET;
        }

        void* allocate_slot_lf()
        {
            std::uint64_t old = free_head_.load(std::memory_order_acquire);
            for (;;)
            {
                std::uint32_t idx = lo(old);
                if (idx == NULL_IDX)
                {
                    // Free list empty: grow under the lock, then retry.
                    std::lock_guard<std::mutex> lock(grow_mutex_);
                    old = free_head_.load(std::memory_order_acquire);
                    if (lo(old) == NULL_IDX)
                    {
                        grow_lf();
                        old = free_head_.load(std::memory_order_acquire);
                    }
                    continue;
                }

                // header_at(idx).next is in a region disjoint from any live T,
                // so this read is race-free; a stale value is caught by the CAS.
                const std::uint32_t nxt = header_at(idx)->next.load(std::memory_order_acquire);
                const std::uint64_t neu = pack(nxt, hi(old) + 1);
                if (free_head_.compare_exchange_weak(
                        old, neu, std::memory_order_acq_rel, std::memory_order_acquire))
                {
                    return storage_addr(idx);
                }
                // CAS failed: `old` has been refreshed; retry.
            }
        }

        void deallocate_slot_lf(void* ptr) noexcept
        {
            auto* h = reinterpret_cast<detail::SlotHeaderLF*>(
                static_cast<std::byte*>(ptr) - LF_STORAGE_OFFSET);
            const std::uint32_t idx = h->self;

            std::uint64_t old = free_head_.load(std::memory_order_acquire);
            std::uint64_t neu;
            do
            {
                // The slot is ours until published by the CAS below, so storing
                // its next link first (relaxed) is safe; the release CAS
                // publishes it to a future acquirer.
                h->next.store(lo(old), std::memory_order_relaxed);
                neu = pack(idx, hi(old) + 1);
            }
            while (!free_head_.compare_exchange_weak(
                        old, neu, std::memory_order_acq_rel, std::memory_order_acquire));
        }

        // ═══ Dispatchers ════════════════════════════════════════════════

        void* allocate_slot()
        {
            if constexpr (ThreadSafe)
                return allocate_slot_lf();
            else
                return allocate_slot_st();
        }

        void deallocate_slot(void* ptr) noexcept
        {
            if constexpr (ThreadSafe)
                deallocate_slot_lf(ptr);
            else
                deallocate_slot_st(ptr);
        }

        // ═══ Chunk management ═══════════════════════════════════════════

        /**
         * @brief Allocates a new chunk and threads its slots onto the
         *        single-threaded free list.
         */
        void grow()
        {
            void* raw = ::operator new(SLOT_SIZE * ChunkSize, std::align_val_t{SLOT_ALIGN});
            chunks_.push_back(raw);
            capacity_ += ChunkSize;

            auto* base = static_cast<std::byte*>(raw);
            for (size_type i = 0; i < ChunkSize - 1; ++i)
            {
                auto* curr = reinterpret_cast<detail::FreeNode*>(base + i * SLOT_SIZE);
                auto* next = reinterpret_cast<detail::FreeNode*>(base + (i + 1) * SLOT_SIZE);
                curr->next = next;
            }
            auto* last = reinterpret_cast<detail::FreeNode*>(base + (ChunkSize - 1) * SLOT_SIZE);
            last->next    = free_head_st_;
            free_head_st_ = reinterpret_cast<detail::FreeNode*>(base);
        }

        /**
         * @brief Allocates a new chunk and pushes its slots onto the lock-free
         *        free list. Must be called under @c grow_mutex_.
         *
         * The whole chunk is spliced onto the stack with a single CAS; the
         * internal links touch only this chunk's own (not-yet-published) headers,
         * so no concurrent acquirer can observe a half-built chain.
         */
        void grow_lf()
        {
            const size_type cidx = chunk_count_.load(std::memory_order_relaxed);
            if (cidx >= max_chunks_)
                throw std::length_error("ObjectPool: exceeded max_chunks capacity");

            std::byte* base = static_cast<std::byte*>(
                ::operator new(LF_SLOT_STRIDE * ChunkSize, std::align_val_t{LF_SLOT_ALIGN}));

            const std::uint32_t base_index = static_cast<std::uint32_t>(cidx * ChunkSize);

            // Construct each header and chain it to its in-chunk successor.
            for (size_type i = 0; i < ChunkSize; ++i)
            {
                auto* h = new (base + i * LF_SLOT_STRIDE) detail::SlotHeaderLF;
                h->self = base_index + static_cast<std::uint32_t>(i);
                const std::uint32_t nxt = (i + 1 < ChunkSize)
                    ? (base_index + static_cast<std::uint32_t>(i + 1))
                    : NULL_IDX; // overwritten below for the chain tail
                h->next.store(nxt, std::memory_order_relaxed);
            }

            // Publish the chunk base so header_at()/storage_addr() can resolve
            // indices in this chunk before we make them reachable.
            chunk_bases_[cidx].store(base, std::memory_order_release);
            chunk_count_.store(cidx + 1, std::memory_order_release);

            const std::uint32_t first = base_index;
            const std::uint32_t last  = base_index + static_cast<std::uint32_t>(ChunkSize) - 1;
            detail::SlotHeaderLF* last_hdr = header_at(last);

            std::uint64_t old = free_head_.load(std::memory_order_acquire);
            std::uint64_t neu;
            do
            {
                last_hdr->next.store(lo(old), std::memory_order_relaxed);
                neu = pack(first, hi(old) + 1);
            }
            while (!free_head_.compare_exchange_weak(
                        old, neu, std::memory_order_acq_rel, std::memory_order_acquire));
        }

        // ═══ Data members ═══════════════════════════════════════════════

        // ─── single-threaded free list (ThreadSafe == false) ────────────
        std::vector<void*> chunks_;            ///< Pointers to allocated chunks.
        size_type          capacity_ = 0;      ///< Total slot count across all chunks.
        detail::FreeNode*  free_head_st_ = nullptr;

        // ─── lock-free free list (ThreadSafe == true) ───────────────────
        size_type                                  max_chunks_ = kDefaultMaxChunks;
        std::unique_ptr<std::atomic<std::byte*>[]> chunk_bases_;       ///< idx → chunk base.
        std::atomic<size_type>                     chunk_count_{0};    ///< # published chunks.
        // alignas(64) avoids false sharing with adjacent members.
        alignas(64) std::atomic<std::uint64_t>     free_head_{ NULL_IDX }; ///< { tag:32, idx:32 }
        std::mutex                                 grow_mutex_;        ///< serialises grow_lf().
    };

} // namespace lux::cxx
