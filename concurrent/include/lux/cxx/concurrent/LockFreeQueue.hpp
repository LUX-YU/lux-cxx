/*
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once
#include <vector>
#include <atomic>
#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>
#include <algorithm>
#include <chrono>
#include <thread>
#include <iostream>

namespace lux::cxx
{   
    template <typename T>
    class SpscLockFreeRingQueue
    {
    public:
        /**
         * @brief Constructs the queue with a fixed capacity.
         * @param capacity The maximum number of elements (must be > 0).
         */
        explicit SpscLockFreeRingQueue(std::size_t capacity = 64)
            : _buffer(capacity), _capacity(capacity), _head(0), _tail(0), _closed(false)
        {
            assert(capacity > 0 && "Capacity must be greater than 0");
        }

        /**
         * @brief Destructor (no special action needed, no locks).
         */
        ~SpscLockFreeRingQueue()
        {
            close();
        }

        // Non-copyable
        SpscLockFreeRingQueue(const SpscLockFreeRingQueue &) = delete;
        SpscLockFreeRingQueue &operator=(const SpscLockFreeRingQueue &) = delete;
        // Movable if needed
        SpscLockFreeRingQueue(SpscLockFreeRingQueue &&) = default;
        SpscLockFreeRingQueue &operator=(SpscLockFreeRingQueue &&) = default;

        /**
         * @brief Closes the queue. Further push attempts will fail immediately,
         *        but pop can still retrieve remaining elements until the queue is empty.
         */
        void close()
        {
            _closed.store(true, std::memory_order_release);
        }

        /**
         * @brief Checks if the queue is closed.
         * @return True if the queue is closed, otherwise false.
         */
        bool closed() const
        {
            return _closed.load(std::memory_order_acquire);
        }

        /**
         * @brief Pushes an element into the queue if not full and not closed.
         * @tparam U The type of the element to be pushed.
         * @param value The element to push.
         * @return True if pushed successfully, false if the queue is full or closed.
         */
        template <class U>
        bool push(U &&value)
        {
            if (_closed.load(std::memory_order_acquire))
            {
                return false;
            }

            // Read indices
            const auto head = _head.load(std::memory_order_acquire);
            const auto tail = _tail.load(std::memory_order_relaxed);

            // Next tail (circular increment)
            const auto nextTail = (tail + 1) % _capacity;

            // If nextTail == head, queue is full
            if (nextTail == head)
            {
                return false; // full
            }

            // Write the value into buffer
            _buffer[tail] = std::forward<U>(value);

            // Publish the new tail
            _tail.store(nextTail, std::memory_order_release);
            return true;
        }

        /**
         * @brief Pops an element from the queue if not empty.
         * @param out Reference to store the popped element.
         * @return True if popped successfully, false if the queue is empty (or closed and empty).
         */
        bool pop(T &out)
        {
            // Read indices
            const auto tail = _tail.load(std::memory_order_acquire);
            const auto head = _head.load(std::memory_order_relaxed);

            if (head == tail)
            {
                // Empty
                return false;
            }

            // Read the value
            out = std::move(_buffer[head]);

            // Advance head
            const auto nextHead = (head + 1) % _capacity;
            _head.store(nextHead, std::memory_order_release);
            return true;
        }

        /**
         * @brief Tries to push without blocking (same as push for SPSC).
         * @tparam U The type of the element to be pushed.
         * @param value The element to push.
         * @return True if pushed, false otherwise.
         */
        template <class U>
        bool try_push(U &&value)
        {
            return push(std::forward<U>(value));
        }

        /**
         * @brief Tries to pop without blocking (same as pop for SPSC).
         * @param out Reference to store the popped element.
         * @return True if popped, false if empty.
         */
        bool try_pop(T &out)
        {
            return pop(out);
        }

        /**
         * @brief Gets the current number of elements in the queue.
         * @return The queue size (approximate; valid in single-producer, single-consumer scenario).
         */
        size_t size() const
        {
            const auto head = _head.load(std::memory_order_acquire);
            const auto tail = _tail.load(std::memory_order_acquire);
            if (tail >= head)
            {
                return tail - head;
            }
            else
            {
                return _capacity - (head - tail);
            }
        }

        /**
         * @brief Checks if the queue is empty.
         * @return True if empty, otherwise false.
         */
        bool empty() const
        {
            return (_head.load(std::memory_order_acquire) ==
                    _tail.load(std::memory_order_acquire));
        }

        /**
         * @brief Gets the capacity of the ring buffer.
         * @return The maximum number of elements the queue can hold.
         */
        size_t capacity() const
        {
            return _capacity;
        }

        // ------------------------------------------------------------------
        // Bulk operations (non-blocking). These are optional but shown here
        // to match previous naming style as much as possible. They do not
        // block or wait; partial push/pop is possible.
        // ------------------------------------------------------------------

        /**
         * @brief Pushes multiple elements in bulk if space is available.
         *        No blocking or waiting.
         * @tparam InputIterator Iterator type for input.
         * @param first Iterator to the first element to push.
         * @param count Number of elements to push.
         * @return True if all elements were pushed, false if not enough space or queue is closed.
         */
        template <class InputIterator>
        bool push_bulk(InputIterator first, size_t count)
        {
            if (_closed.load(std::memory_order_acquire))
            {
                return false;
            }
            const auto head = _head.load(std::memory_order_acquire);
            const auto tail = _tail.load(std::memory_order_relaxed);

            // Calculate free space
            size_t freeSpace = (tail >= head)
                                   ? _capacity - (tail - head) - 1
                                   : (head - tail) - 1;

            if (count > freeSpace)
            {
                // Not enough space
                return false;
            }

            // There's enough space, push them
            size_t currentTail = tail;
            for (size_t i = 0; i < count; ++i)
            {
                _buffer[currentTail] = *first++;
                currentTail = (currentTail + 1) % _capacity;
            }
            // Publish new tail
            _tail.store(currentTail, std::memory_order_release);
            return true;
        }

        /**
         * @brief Pops multiple elements in bulk if available.
         *        No blocking or waiting.
         * @tparam OutputIterator Iterator type for output.
         * @param dest Destination iterator where popped elements will be placed.
         * @param maxCount Maximum number of elements to pop.
         * @return The actual number of elements popped.
         */
        template <class OutputIterator>
        size_t pop_bulk(OutputIterator dest, size_t maxCount)
        {
            const auto tail = _tail.load(std::memory_order_acquire);
            const auto head = _head.load(std::memory_order_relaxed);

            // If empty
            if (head == tail)
            {
                return 0;
            }

            // Number of elements in queue
            size_t available = (tail >= head)
                                   ? (tail - head)
                                   : (_capacity - (head - tail));

            size_t toPop = (maxCount < available) ? maxCount : available;

            size_t currentHead = head;
            for (size_t i = 0; i < toPop; ++i)
            {
                *dest++ = std::move(_buffer[currentHead]);
                currentHead = (currentHead + 1) % _capacity;
            }
            // Publish new head
            _head.store(currentHead, std::memory_order_release);
            return toPop;
        }

    private:
        std::vector<T> _buffer;      ///< Underlying buffer storage
        const std::size_t _capacity; ///< Maximum size of the queue

        std::atomic<size_t> _head; ///< Index of the front (consumer side)
        std::atomic<size_t> _tail; ///< Index of the back (producer side)
        std::atomic<bool> _closed; ///< Indicates whether the queue is closed
    };
}