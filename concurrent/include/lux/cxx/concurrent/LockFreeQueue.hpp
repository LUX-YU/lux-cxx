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
#include <optional>
#include <thread>
#include <algorithm>
#include <lux/cxx/concurrent/QueueStatus.hpp>

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
        static constexpr std::size_t ceilPowerOfTwo(std::size_t value) noexcept
        {
            if (value <= 1) return 1;
            --value;
            for (std::size_t shift = 1; shift < sizeof(std::size_t) * 8; shift <<= 1)
            {
                value |= value >> shift;
            }
            return ++value;
        }

        struct alignas(alignof(T)) Storage { unsigned char data[sizeof(T)]; };

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
            : capacity_(ceilPowerOfTwo(capacity)), mask_(capacity_ - 1),
            buffer_(std::make_unique<Storage[]>(capacity_))
        {
            assert(capacity_ && (capacity_ & mask_) == 0 && "capacity must be power‑of‑two");
        }

        /**
         * @brief  Destroy the queue and all live objects still inside it.
         */
        ~SpscLockFreeRingQueue() { drainDestruct(); }

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
        [[nodiscard]] EQueuePushResult tryEmplace(Args&&... args)
            noexcept(std::is_nothrow_constructible_v<T, Args...>)
        {
            if (closed_.load(std::memory_order_acquire))
                return EQueuePushResult::CLOSED;

            const auto tail = tail_.load(std::memory_order_relaxed);
            if (tail - head_.load(std::memory_order_acquire) >= capacity_)
                return EQueuePushResult::FULL;

            new (&buffer_[tail & mask_]) T(std::forward<Args>(args)...);
            tail_.store(tail + 1, std::memory_order_release);
            return EQueuePushResult::ACCEPTED;
        }

        template <class U>
        [[nodiscard]] EQueuePushResult tryPush(U&& value)
            noexcept(std::is_nothrow_constructible_v<T, U&&>)
        {
            return tryEmplace(std::forward<U>(value));
        }

        /**
         * @brief  Pop the front element into @p out.
         * @param  out   Reference receiving the element.
         * @return true  — an element was popped;
         *         false — queue is empty.
         * @warning      Must be called only from the *single consumer* thread.
         */
        [[nodiscard]] EQueuePopResult tryPop(T& out)
        noexcept(std::is_nothrow_move_assignable_v<T> && std::is_nothrow_destructible_v<T>)
        {
            const auto head = head_.load(std::memory_order_relaxed);
            if (head == tail_.load(std::memory_order_acquire))
            {
                return closed_.load(std::memory_order_acquire)
                    ? EQueuePopResult::CLOSED_AND_DRAINED
                    : EQueuePopResult::EMPTY;
            }

            T* ptr = std::launder(reinterpret_cast<T*>(&buffer_[head & mask_]));
            out = std::move(*ptr);
            ptr->~T(); // explicit destruction

            head_.store(head + 1, std::memory_order_release);
            return EQueuePopResult::VALUE;
        }

        /// Pop by move construction. This overload supports move-only payloads
        /// that are neither default constructible nor move assignable while
        /// retaining the queue's allocation-free steady-state behavior.
        [[nodiscard]] QueuePopValue<T> tryPopValue() noexcept(
            std::is_nothrow_move_constructible_v<T> &&
            std::is_nothrow_destructible_v<T>)
        {
            const auto head = head_.load(std::memory_order_relaxed);
            if (head == tail_.load(std::memory_order_acquire))
            {
                return {
                    closed_.load(std::memory_order_acquire)
                        ? EQueuePopResult::CLOSED_AND_DRAINED
                        : EQueuePopResult::EMPTY,
                    std::nullopt
                };
            }

            T* ptr = std::launder(reinterpret_cast<T*>(&buffer_[head & mask_]));
            std::optional<T> value{std::in_place, std::move(*ptr)};
            ptr->~T();
            head_.store(head + 1, std::memory_order_release);
            return {EQueuePopResult::VALUE, std::move(value)};
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
            while (
                pushed < count &&
                tryEmplace(*first) == EQueuePushResult::ACCEPTED
            ) {
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

                T* ptr = std::launder(reinterpret_cast<T*>(&buffer_[head & mask_]));
                *dest++ = std::move(*ptr);
                ptr->~T();
                head_.store(head + 1, std::memory_order_release);
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
            const auto difference = t - h;
            return difference <= capacity_ ? difference : 0;
        }

        /** @return Maximum number of elements the queue can hold. */
        [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

        [[nodiscard]] EQueueState state() const noexcept
        {
            if (!closed()) return EQueueState::OPEN;
            return empty() ? EQueueState::DRAINED : EQueueState::CLOSED;
        }

    private:
        /**
         * @brief  Destroy all remaining objects in the buffer (called from dtor).
         *         Should be executed only when producer & consumer have stopped.
         */
        void drainDestruct() noexcept
        {
            auto h = head_.load(std::memory_order_relaxed);
            auto t = tail_.load(std::memory_order_relaxed);
            while (h != t) {
                std::launder(reinterpret_cast<T*>(&buffer_[h & mask_]))->~T();
                ++h;
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
