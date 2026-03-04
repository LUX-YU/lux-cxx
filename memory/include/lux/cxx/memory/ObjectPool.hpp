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
 * - @c true : lock-free free list with per-thread cache to minimise CAS
 *   contention. Multiple threads may call @c acquire() and @c release()
 *   concurrently with near-zero contention in the common case.
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
        T* release() noexcept
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

    // ─── implementation detail: free-list node stored inside free slots ──────

    namespace detail
    {
        /**
         * @brief Node stored in-place inside a free slot.
         *
         * When a slot is free, its storage is reinterpreted as a @c FreeNode
         * that points to the next free slot. This gives zero extra memory
         * overhead for the free list.
         */
        struct FreeNode
        {
            FreeNode* next;
        };

        /**
         * @brief Tagged pointer for ABA-safe lock-free CAS operations.
         *
         * Packs a @c FreeNode* and a monotonic tag into a single value
         * that can be atomically compared-and-swapped.
         */
        struct TaggedPtr
        {
            FreeNode*   ptr = nullptr;
            std::size_t tag = 0;

            bool operator==(const TaggedPtr&) const noexcept = default;
            bool operator!=(const TaggedPtr&) const noexcept = default;
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
     * Each thread maintains a **thread-local cache** of free slots.
     * When the local cache is empty, the thread refills it by popping a
     * batch from the global lock-free free list (one CAS). When the local
     * cache exceeds a high-water mark, excess slots are flushed back to
     * the global list (one CAS). This amortises CAS operations over many
     * acquire/release calls, achieving near-single-threaded throughput
     * under contention.
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

    public:
        using value_type = T;
        using pointer    = T*;
        using size_type  = std::size_t;

        // ─── construction / destruction ─────────────────────────────────

        /** @brief Constructs an empty pool. Chunks are allocated on demand. */
        ObjectPool() = default;

        /**
         * @brief Constructs a pool and pre-allocates enough chunks to hold
         *        at least @p initial_capacity objects.
         */
        explicit ObjectPool(size_type initial_capacity)
        {
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
            for (auto& chunk : chunks_)
                ::operator delete(chunk, std::align_val_t{alignof(T)});
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
            while (capacity_ < n)
                grow();
        }

        /**
         * @brief Returns the total number of slots (free + in-use) across all chunks.
         */
        [[nodiscard]] size_type capacity() const noexcept { return capacity_; }

        /**
         * @brief Returns the number of currently allocated chunks.
         */
        [[nodiscard]] size_type chunk_count() const noexcept { return chunks_.size(); }

    private:
        // ═══ Slot storage ═══════════════════════════════════════════════

        /**
         * @brief Conceptual layout of a single slot.
         *
         * When *free*, the slot's bytes are reinterpreted as a
         * @c detail::FreeNode (intrusive next-pointer).
         * When *alive*, the slot holds a live @c T object.
         */
        static constexpr size_type SLOT_SIZE  = sizeof(T)  < sizeof(detail::FreeNode)
                                              ? sizeof(detail::FreeNode) : sizeof(T);
        static constexpr size_type SLOT_ALIGN = alignof(T) < alignof(detail::FreeNode)
                                              ? alignof(detail::FreeNode) : alignof(T);

        // ═══ Single-threaded free list ══════════════════════════════════

        /**
         * @brief Allocates one slot from the free list, growing if empty.
         *        Single-threaded specialisation.
         */
        void* allocate_slot_st()
        {
            if (!free_head_st_)
                grow();

            detail::FreeNode* node = free_head_st_;
            free_head_st_ = node->next;
            return static_cast<void*>(node);
        }

        /**
         * @brief Returns a slot to the free list (single-threaded path).
         */
        void deallocate_slot_st(void* ptr) noexcept
        {
            auto* node  = static_cast<detail::FreeNode*>(ptr);
            node->next  = free_head_st_;
            free_head_st_ = node;
        }

        // ═══ Lock-free free list with thread-local cache ════════════════

        /**
         * @brief Number of slots to transfer between thread-local cache
         *        and the global free list in a single CAS batch.
         */
        static constexpr size_type LOCAL_CACHE_REFILL  = ChunkSize;
        static constexpr size_type LOCAL_CACHE_HIGH_WATER = ChunkSize * 2;

        /**
         * @brief Per-thread cache: a plain (non-atomic) singly-linked list
         *        of free slots private to each thread. Access is contention-free.
         */
        struct ThreadLocalCache
        {
            detail::FreeNode* head  = nullptr;
            size_type         count = 0;
        };

        /**
         * @brief Returns the thread-local cache for this pool instance.
         *
         * Uses a static thread_local vector indexed by pool id to avoid
         * cross-pool interference. The cache is lazily initialised.
         */
        ThreadLocalCache& get_tl_cache()
        {
            // Each pool instance gets a unique id for indexing into the
            // thread-local array. This avoids the cost of a hash map.
            thread_local std::vector<ThreadLocalCache> tl_caches;

            auto id = pool_id_;
            if (id >= tl_caches.size())
                tl_caches.resize(id + 1);
            return tl_caches[id];
        }

        /**
         * @brief Pops up to @p count nodes from the global free list as a
         *        linked batch in a single CAS.
         * @return Head of the popped chain, or nullptr if the list is empty.
         */
        detail::FreeNode* global_pop_batch(size_type count)
        {
            while (true)
            {
                detail::TaggedPtr old_head = free_head_lf_.load(std::memory_order_acquire);
                if (!old_head.ptr)
                    return nullptr;

                // Walk up to `count` nodes to find the split point.
                detail::FreeNode* tail = old_head.ptr;
                size_type walked = 1;
                while (walked < count && tail->next)
                {
                    tail = tail->next;
                    ++walked;
                }

                detail::TaggedPtr new_head;
                new_head.ptr = tail->next;
                new_head.tag = old_head.tag + 1;

                if (free_head_lf_.compare_exchange_weak(
                        old_head, new_head,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire))
                {
                    tail->next = nullptr; // terminate the batch chain
                    return old_head.ptr;
                }
                // CAS failed — retry.
            }
        }

        /**
         * @brief Pushes a chain [first … last] onto the global free list
         *        in a single CAS.
         */
        void global_push_batch(detail::FreeNode* first, detail::FreeNode* last) noexcept
        {
            detail::TaggedPtr old_head = free_head_lf_.load(std::memory_order_acquire);
            detail::TaggedPtr new_head;
            do
            {
                last->next   = old_head.ptr;
                new_head.ptr = first;
                new_head.tag = old_head.tag + 1;
            }
            while (!free_head_lf_.compare_exchange_weak(
                        old_head, new_head,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire));
        }

        /**
         * @brief Allocates one slot (lock-free path with thread-local cache).
         */
        void* allocate_slot_lf()
        {
            auto& cache = get_tl_cache();

            // Fast path: pop from thread-local cache (no atomic).
            if (cache.head)
            {
                detail::FreeNode* node = cache.head;
                cache.head = node->next;
                --cache.count;
                return static_cast<void*>(node);
            }

            // Slow path: refill local cache from global list.
            detail::FreeNode* batch = global_pop_batch(LOCAL_CACHE_REFILL);
            if (!batch)
            {
                // Global list is also empty — grow.
                std::lock_guard<std::mutex> lock(grow_mutex_);
                // Double-check after lock.
                batch = global_pop_batch(LOCAL_CACHE_REFILL);
                if (!batch)
                {
                    grow();
                    batch = global_pop_batch(LOCAL_CACHE_REFILL);
                }
            }

            // Pop the first node for the caller, rest goes to local cache.
            detail::FreeNode* result = batch;
            cache.head = batch->next;
            // Count the remaining nodes.
            size_type cnt = 0;
            for (auto* n = cache.head; n; n = n->next) ++cnt;
            cache.count = cnt;

            return static_cast<void*>(result);
        }

        /**
         * @brief Returns a slot to the pool (lock-free path).
         *        Pushes to thread-local cache; flushes excess to global if
         *        the cache exceeds the high-water mark.
         */
        void deallocate_slot_lf(void* ptr) noexcept
        {
            auto& cache = get_tl_cache();

            auto* node = static_cast<detail::FreeNode*>(ptr);
            node->next = cache.head;
            cache.head = node;
            ++cache.count;

            // If the local cache is too large, flush half back to the global list.
            if (cache.count > LOCAL_CACHE_HIGH_WATER)
            {
                size_type to_flush = cache.count / 2;
                detail::FreeNode* first = cache.head;
                detail::FreeNode* prev  = first;
                for (size_type i = 1; i < to_flush; ++i)
                    prev = prev->next;

                cache.head   = prev->next;
                cache.count -= to_flush;

                // prev is the last node in the batch to flush.
                global_push_batch(first, prev);
            }
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
         * @brief Allocates a new chunk and threads all its slots onto the free list.
         *
         * In @c ThreadSafe mode this must be called under @c grow_mutex_.
         * In single-threaded mode it is called directly.
         */
        void grow()
        {
            // Allocate raw memory for ChunkSize slots.
            void* raw = ::operator new(SLOT_SIZE * ChunkSize, std::align_val_t{SLOT_ALIGN});
            chunks_.push_back(raw);
            capacity_ += ChunkSize;

            // Thread slots into the free list in reverse order so that the
            // first slot in the chunk is popped first (cache-friendly).
            auto* base = static_cast<std::byte*>(raw);

            if constexpr (ThreadSafe)
            {
                // Build a local chain, then CAS-splice it onto the head.
                auto* first = reinterpret_cast<detail::FreeNode*>(base);
                for (size_type i = 0; i < ChunkSize - 1; ++i)
                {
                    auto* curr = reinterpret_cast<detail::FreeNode*>(base + i * SLOT_SIZE);
                    auto* next = reinterpret_cast<detail::FreeNode*>(base + (i + 1) * SLOT_SIZE);
                    curr->next = next;
                }
                auto* last = reinterpret_cast<detail::FreeNode*>(base + (ChunkSize - 1) * SLOT_SIZE);

                // Splice: last->next = current head; head = first.
                detail::TaggedPtr old_head = free_head_lf_.load(std::memory_order_acquire);
                detail::TaggedPtr new_head;
                do
                {
                    last->next   = old_head.ptr;
                    new_head.ptr = first;
                    new_head.tag = old_head.tag + 1;
                }
                while (!free_head_lf_.compare_exchange_weak(
                            old_head, new_head,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire));
            }
            else
            {
                // Single-threaded: just prepend the chain.
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
        }

        // ═══ Data members ═══════════════════════════════════════════════

        std::vector<void*> chunks_;           ///< Pointers to allocated chunks.
        size_type          capacity_ = 0;     ///< Total slot count across all chunks.

        // ─── single-threaded free list ──────────────────────────────────
        detail::FreeNode* free_head_st_ = nullptr;

        // ─── lock-free free list + thread-local cache ───────────────────
        // alignas(64) avoids false sharing with adjacent members.
        alignas(64) std::atomic<detail::TaggedPtr> free_head_lf_{};

        std::mutex grow_mutex_;  ///< Serialises chunk allocation in lock-free mode.

        /// Unique id for this pool instance, used to index thread-local caches.
        static std::atomic<size_type>& next_pool_id()
        {
            static std::atomic<size_type> id{0};
            return id;
        }
        size_type pool_id_ = next_pool_id().fetch_add(1, std::memory_order_relaxed);
    };

} // namespace lux::cxx
