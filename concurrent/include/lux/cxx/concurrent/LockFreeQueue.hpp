#pragma once
/*
 * Copyright (c) 2025 Chenhui Yu
 * SPDX-License-Identifier: MIT
 */

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <new>
#include <thread>
#include <algorithm>

namespace lux::cxx
{
    /**
     * @tparam T  Element type stored in the queue.
     *
     * Single‑producer / single‑consumer lock‑free ring queue.
     * Capacity is always rounded up to the next power‑of‑two so the index can be
     * masked instead of modulo‑divided (faster).
     */
    template <typename T>
    class SpscLockFreeRingQueue
    {
        // ------------------------------------------------------------------
        // helpers
        // ------------------------------------------------------------------
        /**
         * @brief  Return the smallest power‑of‑two that is ≥ @p v.
         */
        static constexpr std::size_t ceil_pow2(std::size_t v) noexcept
        {
            if (v <= 1) return 1;
            --v;
            for (std::size_t i = 1; i < sizeof(std::size_t) * 8; i <<= 1) v |= v >> i;
            return ++v;
        }

        using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;

    public:
        // ------------------------------------------------------------------
        // ctor / dtor & rule‑of‑five
        // ------------------------------------------------------------------
        /**
         * @param capacity Requested capacity in element count.  Will be rounded up
         *                 to the next power‑of‑two (minimum 1).
         *
         * @note    The queue never reallocates; capacity is fixed after
         *          construction.
         */
        explicit SpscLockFreeRingQueue(std::size_t capacity = 64)
            : capacity_(ceil_pow2(capacity)), mask_(capacity_ - 1),
            buffer_(std::make_unique<Storage[]>(capacity_))
        {
            assert(capacity_ && (capacity_ & mask_) == 0 && "capacity must be power‑of‑two");
        }

        /**
         * @brief  Destroy the queue and all live objects still inside it.
         */
        ~SpscLockFreeRingQueue() { drain_destruct(); }

        SpscLockFreeRingQueue(const SpscLockFreeRingQueue&) = delete;
        SpscLockFreeRingQueue& operator=(const SpscLockFreeRingQueue&) = delete;
        SpscLockFreeRingQueue(SpscLockFreeRingQueue&&) = delete;
        SpscLockFreeRingQueue& operator=(SpscLockFreeRingQueue&&) = delete;

        // ------------------------------------------------------------------
        // single‑element operations
        // ------------------------------------------------------------------
        /**
         * @brief  Construct an element in‑place at the tail.
         * @tparam Args  Constructor argument types.
         * @param  args  Arguments forwarded to @p T's constructor.
         * @return       true  — element inserted;
         *               false — queue is full or already closed.
         * @warning      Must be called only from the *single producer* thread.
         */
        template <class... Args>
        bool emplace(Args&&... args) noexcept
        {
            if (closed_.load(std::memory_order_acquire)) return false;

            const auto tail = tail_.load(std::memory_order_relaxed);
            const auto next = (tail + 1) & mask_;
            if (next == head_.load(std::memory_order_acquire)) return false; // full

            new (&buffer_[tail]) T(std::forward<Args>(args)...);
            tail_.store(next, std::memory_order_release);
            return true;
        }

        /**
         * @brief  Push an element copy / move into the queue.
         * @param  value  Element to copy / move.
         * @return Same semantics as #emplace.
         */
        template <class U>
        bool push(U&& value) noexcept { return emplace(std::forward<U>(value)); }

        /**
         * @brief  Pop the front element into @p out.
         * @param  out   Reference receiving the element.
         * @return true  — an element was popped;
         *         false — queue is empty.
         * @warning      Must be called only from the *single consumer* thread.
         */
        bool pop(T& out) noexcept
        {
            const auto head = head_.load(std::memory_order_relaxed);
            if (head == tail_.load(std::memory_order_acquire)) return false; // empty

            T* ptr = std::launder(reinterpret_cast<T*>(&buffer_[head]));
            out = std::move(*ptr);
            ptr->~T(); // explicit destruction

            head_.store((head + 1) & mask_, std::memory_order_release);
            return true;
        }

        // ------------------------------------------------------------------
        // bulk operations (non‑blocking)
        // ------------------------------------------------------------------
        /**
         * @brief  Push up to @p count elements from an input iterator range.
         * @param  first  Iterator to the first element.
         * @param  count  Maximum number of elements to push.
         * @return Number of elements actually pushed (≤ count).
         */
        template <class InputIt>
        std::size_t bulk_push(InputIt first, std::size_t count)
        {
            std::size_t pushed = 0;
            while (pushed < count && emplace(*first)) {
                ++pushed;
                ++first;
            }
            return pushed;
        }

        /**
         * @brief  Pop up to @p maxCount elements into an output iterator.
         * @param  dest      Output iterator receiving the elements.
         * @param  maxCount  Maximum number of elements to pop.
         * @return Number of elements actually popped (≤ maxCount).
         */
        template <class OutputIt>
        std::size_t bulk_pop(OutputIt dest, std::size_t maxCount)
        {
            std::size_t popped = 0;
            while (popped < maxCount) {
                const auto head = head_.load(std::memory_order_relaxed);
                if (head == tail_.load(std::memory_order_acquire)) break;

                T* ptr = std::launder(reinterpret_cast<T*>(&buffer_[head]));
                *dest++ = std::move(*ptr);
                ptr->~T();
                head_.store((head + 1) & mask_, std::memory_order_release);
                ++popped;
            }
            return popped;
        }

        // ------------------------------------------------------------------
        // status helpers
        // ------------------------------------------------------------------
        /** Close the queue.  Further push/emplace attempts will fail. */
        void close() noexcept { closed_.store(true, std::memory_order_release); }

        /** @return Whether #close() has been called. */
        [[nodiscard]] bool closed() const noexcept
        {
            return closed_.load(std::memory_order_acquire);
        }

        /** @return true if the queue has no elements. */
        [[nodiscard]] bool empty() const noexcept
        {
            return head_.load(std::memory_order_acquire) ==
                tail_.load(std::memory_order_acquire);
        }

        /** @return Current element count (approximate, because producer/consumer race). */
        [[nodiscard]] std::size_t size() const noexcept
        {
            const auto h = head_.load(std::memory_order_acquire);
            const auto t = tail_.load(std::memory_order_acquire);
            return (t - h) & mask_;
        }

        /** @return Maximum number of elements the queue can hold. */
        [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

    private:
        /**
         * @brief  Destroy all remaining objects in the buffer (called from dtor).
         *         Should be executed only when producer & consumer have stopped.
         */
        void drain_destruct() noexcept
        {
            auto h = head_.load(std::memory_order_relaxed);
            auto t = tail_.load(std::memory_order_relaxed);
            while (h != t) {
                std::launder(reinterpret_cast<T*>(&buffer_[h]))->~T();
                h = (h + 1) & mask_;
            }
        }

        // ------------------------------------------------------------------
        // data members
        // ------------------------------------------------------------------
        alignas(64) std::atomic<std::size_t> head_{ 0 };
        alignas(64) std::atomic<std::size_t> tail_{ 0 };

        const std::size_t capacity_; ///< ring size (power‑of‑two)
        const std::size_t mask_;     ///< capacity_ - 1, used for index masking

        std::unique_ptr<Storage[]> buffer_; ///< raw storage for elements

        std::atomic<bool> closed_{ false };   ///< closure flag
    };

} // namespace lux::cxx
