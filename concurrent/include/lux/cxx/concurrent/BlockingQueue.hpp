#pragma once
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

#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <chrono>
#include <cassert>
#include <utility>
#include <queue>
#include <memory>
#include <new>
#include <type_traits>
#include <cstring>

namespace lux::cxx
{
    /**
     * @class BlockingRingQueue
     * @desc A thread-safe blocking queue implementation that supports
     *       various operations such as push, pop, and bulk operations,
     *       with optional timeout support.
     * @tparam T Type of elements stored in the queue.
     */
    template <typename T>
    class BlockingRingQueue
    {
    private:
        using storage_t = std::aligned_storage_t<sizeof(T), alignof(T)>;

        // Helper to get pointer to element slot
        inline T* ptr_at(std::size_t index) noexcept
        {
            return std::launder(reinterpret_cast<T*>(&_buffer[index]));
        }
        inline const T* ptr_at(std::size_t index) const noexcept
        {
            return std::launder(reinterpret_cast<const T*>(&_buffer[index]));
        }

    public:
        /**
         * @desc Constructs a BlockingRingQueue with a specified capacity.
         * @param capacity Maximum number of elements the queue can hold. Default is 64.
         */
        explicit BlockingRingQueue(std::size_t capacity = 64)
            : _capacity(capacity), _buffer(capacity ? new storage_t[capacity] : nullptr),
            _head(0), _tail(0), _size(0), _exit(false)
        {
            assert(capacity > 0);
        }

        /**
         * @desc Destructor to close the queue and release resources.
         */
        ~BlockingRingQueue()
        {
            close();
            // Destroy any remaining constructed objects
            if (_buffer)
            {
                while (_size > 0)
                {
                    std::destroy_at(ptr_at(_head));
                    _head = (_head + 1) % _capacity;
                    --_size;
                }
                delete[] _buffer;
            }
        }

        BlockingRingQueue(const BlockingRingQueue&) = delete;
        BlockingRingQueue& operator=(const BlockingRingQueue&) = delete;
        BlockingRingQueue(BlockingRingQueue&&) = delete;
        BlockingRingQueue& operator=(BlockingRingQueue&&) = delete;

        /**
         * @desc Closes the queue. Further push operations are disallowed,
         *       but pop operations can still retrieve remaining elements.
         */
        void close()
        {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _exit = true;
            }
            _not_full.notify_all();
            _not_empty.notify_all();
        }

        /**
         * @desc Checks if the queue is closed.
         * @return True if the queue is closed; otherwise, false.
         */
        bool closed() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _exit;
        }

        /**
         * @desc Pushes a new element into the queue. Blocks if the queue is full.
         * @tparam U Type of the element to be pushed.
         * @param value The element to be pushed.
         * @return True if the element is successfully pushed; false if the queue is closed.
         */
        template <class U>
        bool push(U&& value)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _not_full.wait(lock, [this]
                { return _exit || (_size < _capacity); });

            if (_exit)
                return false;

            std::construct_at(ptr_at(_tail), std::forward<U>(value));
            _tail = (_tail + 1) % _capacity;
            ++_size;

            lock.unlock();
            _not_empty.notify_one();
            return true;
        }

        /**
         * @desc Pushes a new element into the queue with a timeout.
         * @tparam U Type of the element to be pushed.
         * @tparam Rep, Period Duration parameters for the timeout.
         * @param value The element to be pushed.
         * @param timeout Maximum time to wait for a slot to be available.
         * @return True if the element is successfully pushed; false on timeout or if the queue is closed.
         */
        template <class U, class Rep, class Period>
        bool push(U&& value, const std::chrono::duration<Rep, Period>& timeout)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (!_not_full.wait_for(lock, timeout, [this]
                { return _exit || (_size < _capacity); }))
            {
                return false;
            }

            if (_exit)
                return false;

            std::construct_at(ptr_at(_tail), std::forward<U>(value));
            _tail = (_tail + 1) % _capacity;
            ++_size;

            lock.unlock();
            _not_empty.notify_one();
            return true;
        }

        /**
         * @desc Constructs a new element in place at the back of the queue. Blocks if the queue is full.
         * @tparam Args Variadic template arguments for constructing the element.
         * @param args Arguments to construct the new element.
         * @return True if the element is successfully constructed; false if the queue is closed.
         */
        template <class... Args>
        bool emplace(Args &&...args)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _not_full.wait(lock, [this]
                { return _exit || (_size < _capacity); });

            if (_exit)
                return false;

            std::construct_at(ptr_at(_tail), T(std::forward<Args>(args)...));
            _tail = (_tail + 1) % _capacity;
            ++_size;

            lock.unlock();
            _not_empty.notify_one();
            return true;
        }

        /**
         * @desc Constructs a new element in place at the back of the queue with a timeout.
         * @tparam Rep, Period Duration parameters for the timeout.
         * @tparam Args Variadic template arguments for constructing the element.
         * @param timeout Maximum time to wait for a slot to be available.
         * @param args Arguments to construct the new element.
         * @return True if the element is successfully constructed; false on timeout or if the queue is closed.
         */
        template <class Rep, class Period, class... Args>
        bool emplace(const std::chrono::duration<Rep, Period>& timeout, Args &&...args)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (!_not_full.wait_for(lock, timeout, [this]
                { return _exit || (_size < _capacity); }))
            {
                return false;
            }

            if (_exit)
                return false;

            std::construct_at(ptr_at(_tail), T(std::forward<Args>(args)...));
            _tail = (_tail + 1) % _capacity;
            ++_size;

            lock.unlock();
            _not_empty.notify_one();
            return true;
        }

        /**
         * @desc Pops an element from the queue. Blocks if the queue is empty.
         * @param out Reference to the variable where the popped element will be stored.
         * @return True if an element is successfully popped; false if the queue is closed and empty.
         */
        bool pop(T& out)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _not_empty.wait(lock, [this]
                { return _exit || (_size > 0); });

            if (_size == 0)
            {
                return false;
            }

            T* elemPtr = ptr_at(_head);
            out = std::move(*elemPtr);
            std::destroy_at(elemPtr);
            _head = (_head + 1) % _capacity;
            --_size;

            lock.unlock();
            _not_full.notify_one();
            return true;
        }

        /**
         * @desc Pops an element from the queue with a timeout.
         * @tparam Rep, Period Duration parameters for the timeout.
         * @param out Reference to the variable where the popped element will be stored.
         * @param timeout Maximum time to wait for an element to be available.
         * @return True if an element is successfully popped; false on timeout or if the queue is closed and empty.
         */
        template <class Rep, class Period>
        bool pop(T& out, const std::chrono::duration<Rep, Period>& timeout)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (!_not_empty.wait_for(lock, timeout, [this]
                { return _exit || (_size > 0); }))
            {
                return false;
            }

            if (_size == 0)
            {
                return false;
            }

            T* elemPtr = ptr_at(_head);
            out = std::move(*elemPtr);
            std::destroy_at(elemPtr);
            _head = (_head + 1) % _capacity;
            --_size;

            lock.unlock();
            _not_full.notify_one();
            return true;
        }

        /**
         * @desc Pushes multiple elements into the queue. Blocks if the queue does not have enough space.
         * @tparam InputIterator Type of the input iterator.
         * @param first Iterator to the first element to push.
         * @param count Number of elements to push.
         * @return True if the elements are successfully pushed; false if the queue is closed.
         */
        template <class InputIterator>
        bool push_bulk(InputIterator first, size_t count)
        {
            if (count == 0)
                return true; // No-op branch for zero elements

            if (count > _capacity)
                return false; // Impossible to fit even if empty

            std::unique_lock<std::mutex> lock(_mutex);
            _not_full.wait(lock, [this, count]
                { return _exit || (_size + count <= _capacity); });

            if (_exit)
                return false;

            for (size_t i = 0; i < count; ++i)
            {
                std::construct_at(ptr_at(_tail), *first++);
                _tail = (_tail + 1) % _capacity;
            }
            _size += count;

            lock.unlock();
            _not_empty.notify_all();
            return true;
        }

        /**
         * @desc Pushes multiple elements into the queue with a timeout.
         * @tparam InputIterator Type of the input iterator.
         * @tparam Rep, Period Duration parameters for the timeout.
         * @param first Iterator to the first element to push.
         * @param count Number of elements to push.
         * @param timeout Maximum time to wait for space to be available.
         * @return True if the elements are successfully pushed; false on timeout or if the queue is closed.
         */
        template <class InputIterator, class Rep, class Period>
        bool push_bulk(InputIterator first, size_t count,
            const std::chrono::duration<Rep, Period>& timeout)
        {
            if (count == 0)
                return true;

            if (count > _capacity)
                return false;

            std::unique_lock<std::mutex> lock(_mutex);
            if (!_not_full.wait_for(lock, timeout, [this, count]
                { return _exit || (_size + count <= _capacity); }))
            {
                return false;
            }

            if (_exit)
                return false;

            for (size_t i = 0; i < count; ++i)
            {
                std::construct_at(ptr_at(_tail), *first++);
                _tail = (_tail + 1) % _capacity;
            }
            _size += count;

            lock.unlock();
            _not_empty.notify_all();
            return true;
        }

        /**
         * @desc Pops multiple elements from the queue. Blocks if the queue is empty.
         * @tparam OutputIterator Type of the output iterator.
         * @param dest Iterator to the destination where popped elements will be stored.
         * @param maxCount Maximum number of elements to pop.
         * @return Number of elements successfully popped.
         */
        template <class OutputIterator>
        size_t pop_bulk(OutputIterator dest, size_t maxCount)
        {
            if (maxCount == 0)
                return 0; // No-op

            std::unique_lock<std::mutex> lock(_mutex);
            _not_empty.wait(lock, [this]
                { return _exit || (_size > 0); });

            if (_size == 0)
            {
                return 0;
            }

            size_t toPop = (maxCount < _size) ? maxCount : _size;
            for (size_t i = 0; i < toPop; ++i)
            {
                T* elemPtr = ptr_at(_head);
                *dest++ = std::move(*elemPtr);
                std::destroy_at(elemPtr);
                _head = (_head + 1) % _capacity;
            }
            _size -= toPop;

            lock.unlock();
            _not_full.notify_all();
            return toPop;
        }

        /**
         * @desc Pops multiple elements from the queue with a timeout.
         * @tparam OutputIterator Type of the output iterator.
         * @tparam Rep, Period Duration parameters for the timeout.
         * @param dest Iterator to the destination where popped elements will be stored.
         * @param maxCount Maximum number of elements to pop.
         * @param timeout Maximum time to wait for elements to be available.
         * @return Number of elements successfully popped. Returns 0 on timeout or if the queue is closed and empty.
         */
        template <class OutputIterator, class Rep, class Period>
        size_t pop_bulk(OutputIterator dest, size_t maxCount,
            const std::chrono::duration<Rep, Period>& timeout)
        {
            if (maxCount == 0)
                return 0;

            std::unique_lock<std::mutex> lock(_mutex);
            if (!_not_empty.wait_for(lock, timeout, [this]
                { return _exit || (_size > 0); }))
            {
                return 0;
            }

            if (_size == 0)
            {
                return 0;
            }

            size_t toPop = (maxCount < _size) ? maxCount : _size;
            for (size_t i = 0; i < toPop; ++i)
            {
                T* elemPtr = ptr_at(_head);
                *dest++ = std::move(*elemPtr);
                std::destroy_at(elemPtr);
                _head = (_head + 1) % _capacity;
            }
            _size -= toPop;

            lock.unlock();
            _not_full.notify_all();
            return toPop;
        }

        /**
         * @desc Tries to push an element into the queue without blocking.
         * @tparam U Type of the element to be pushed.
         * @param value The element to be pushed.
         * @return True if the element is successfully pushed; false if the queue is full or closed.
         */
        template <class U>
        bool try_push(U&& value)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_exit || _size >= _capacity)
            {
                return false;
            }
            std::construct_at(ptr_at(_tail), std::forward<U>(value));
            _tail = (_tail + 1) % _capacity;
            ++_size;

            _not_empty.notify_one();
            return true;
        }

        /**
         * @desc Tries to pop an element from the queue without blocking.
         * @param out Reference to the variable where the popped element will be stored.
         * @return True if an element is successfully popped; false if the queue is empty.
         */
        bool try_pop(T& out)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_size == 0)
            {
                return false;
            }

            T* elemPtr = ptr_at(_head);
            out = std::move(*elemPtr);
            std::destroy_at(elemPtr);
            _head = (_head + 1) % _capacity;
            --_size;

            _not_full.notify_one();
            return true;
        }

        /**
         * @desc Tries to push multiple elements without blocking.
         * @tparam InputIterator Type of the input iterator.
         * @param first Iterator to the first element.
         * @param count Number of elements to push.
         * @return True if all elements are pushed; false if capacity insufficient or closed.
         */
        template <class InputIterator>
        bool try_push_bulk(InputIterator first, size_t count)
        {
            if (count == 0)
                return true;
            std::lock_guard<std::mutex> lock(_mutex);
            if (_exit || (_size + count > _capacity))
                return false;

            for (size_t i = 0; i < count; ++i)
            {
                std::construct_at(ptr_at(_tail), *first++);
                _tail = (_tail + 1) % _capacity;
            }
            _size += count;
            _not_empty.notify_all();
            return true;
        }

        /**
         * @desc Tries to pop multiple elements without blocking.
         * @tparam OutputIterator Output iterator type.
         * @param dest Destination iterator.
         * @param maxCount Maximum elements to pop.
         * @return Actual number of elements popped.
         */
        template <class OutputIterator>
        size_t try_pop_bulk(OutputIterator dest, size_t maxCount)
        {
            if (maxCount == 0)
                return 0;
            std::lock_guard<std::mutex> lock(_mutex);
            if (_size == 0)
                return 0;

            size_t toPop = (maxCount < _size) ? maxCount : _size;
            for (size_t i = 0; i < toPop; ++i)
            {
                T* elemPtr = ptr_at(_head);
                *dest++ = std::move(*elemPtr);
                std::destroy_at(elemPtr);
                _head = (_head + 1) % _capacity;
            }
            _size -= toPop;
            _not_full.notify_all();
            return toPop;
        }

        /**
         * @desc Gets the current number of elements in the queue.
         * @return The number of elements in the queue.
         */
        size_t size() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _size;
        }

        /**
         * @desc Checks if the queue is empty.
         * @return True if the queue is empty; otherwise, false.
         */
        bool empty() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return (_size == 0);
        }

        /**
         * @desc Gets the maximum capacity of the queue.
         * @return The capacity of the queue.
         */
        size_t capacity() const
        {
            return _capacity;
        }

    private:
        std::size_t _capacity;  ///< Maximum capacity of the queue.
        storage_t* _buffer;     ///< Raw buffer to store elements (uninitialized).
        std::size_t _head;      ///< Index of the head of the queue.
        std::size_t _tail;      ///< Index of the tail of the queue.
        std::size_t _size;      ///< Current size of the queue.
        bool _exit;             ///< Flag indicating whether the queue is closed.

        mutable std::mutex _mutex;          ///< Mutex for thread-safety.
        std::condition_variable _not_full;  ///< Condition variable to wait when the queue is full.
        std::condition_variable _not_empty; ///< Condition variable to wait when the queue is empty.
    };

    template <typename T>
    class BlockingQueue
    {
    public:
        /**
         * @brief Constructs a BlockingQueue with a specified capacity.
         *        If capacity == 0, the queue is unbounded (limited only by memory).
         *
         * @param capacity The maximum capacity of the queue.
         *                 If 0, the queue has no upper limit.
         */
        explicit BlockingQueue(std::size_t capacity = 0)
            : _capacity(capacity), _size(0), _exit(false)
        {
            // If _capacity == 0, we treat it as unbounded capacity.
        }

        /**
         * @brief Destructor that closes the queue and notifies all waiting threads.
         */
        ~BlockingQueue()
        {
            close();
        }

        BlockingQueue(const BlockingQueue&) = delete;
        BlockingQueue& operator=(const BlockingQueue&) = delete;
        BlockingQueue(BlockingQueue&&) = delete;
        BlockingQueue& operator=(BlockingQueue&&) = delete;

        /**
         * @brief Closes the queue.
         *        After calling close(), pushing new elements will fail immediately,
         *        but remaining elements can still be popped.
         */
        void close()
        {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _exit = true;
            }
            _not_full.notify_all();
            _not_empty.notify_all();
        }

        /**
         * @brief Checks whether the queue has been closed.
         * @return True if the queue is closed, false otherwise.
         */
        bool closed() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _exit;
        }

        /**
         * @brief Pushes an element into the queue. Blocks if the queue is at capacity (when _capacity > 0).
         *
         * @tparam U The type of the element being pushed (can be T or convertible to T).
         * @param value The element to push.
         * @return True if the element was pushed, false if the queue is closed.
         */
        template <class U>
        bool push(U&& value)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _not_full.wait(lock, [this]
                {
                    // Wait until the queue is not full or the queue is closed
                    return _exit || (_capacity == 0 || _size < _capacity); });

            if (_exit)
                return false;

            _queue.push(std::forward<U>(value));
            ++_size;

            lock.unlock();
            // Notify one thread that might be waiting to pop
            _not_empty.notify_one();
            return true;
        }

        /**
         * @brief Pushes an element into the queue with a timeout.
         *        If the queue is full and remains full until the timeout, returns false.
         *
         * @tparam U The type of the element being pushed.
         * @tparam Rep, Period Duration parameters for timeout.
         * @param value The element to push.
         * @param timeout The maximum time to wait for available capacity.
         * @return True if the element was pushed successfully, false otherwise (timeout or closed).
         */
        template <class U, class Rep, class Period>
        bool push(U&& value, const std::chrono::duration<Rep, Period>& timeout)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (!_not_full.wait_for(lock, timeout, [this]
                { return _exit || (_capacity == 0 || _size < _capacity); }))
            {
                // Timed out
                return false;
            }

            if (_exit)
                return false;

            _queue.push(std::forward<U>(value));
            ++_size;

            lock.unlock();
            _not_empty.notify_one();
            return true;
        }

        /**
         * @brief Constructs an element in-place at the back of the queue.
         *        Blocks if the queue is full (capacity > 0).
         *
         * @tparam Args Variadic template arguments for constructing T.
         * @param args Constructor arguments for T.
         * @return True if constructed and pushed, false if queue is closed.
         */
        template <class... Args>
        bool emplace(Args &&...args)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _not_full.wait(lock, [this]
                { return _exit || (_capacity == 0 || _size < _capacity); });

            if (_exit)
                return false;

            _queue.emplace(std::forward<Args>(args)...);
            ++_size;

            lock.unlock();
            _not_empty.notify_one();
            return true;
        }

        /**
         * @brief In-place construct with a timeout.
         *
         * @tparam Rep, Period Duration parameters.
         * @tparam Args Constructor arguments for T.
         * @param timeout The maximum time to wait for available capacity.
         * @param args Arguments for constructing T.
         * @return True if constructed and pushed, false otherwise (timeout or closed).
         */
        template <class Rep, class Period, class... Args>
        bool emplace(const std::chrono::duration<Rep, Period>& timeout, Args &&...args)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (!_not_full.wait_for(lock, timeout, [this]
                { return _exit || (_capacity == 0 || _size < _capacity); }))
            {
                return false;
            }

            if (_exit)
                return false;

            _queue.emplace(std::forward<Args>(args)...);
            ++_size;

            lock.unlock();
            _not_empty.notify_one();
            return true;
        }

        /**
         * @brief Pops an element from the queue. Blocks if the queue is empty.
         *
         * @param out Reference to store the popped element.
         * @return True if popped successfully, false if the queue is empty and closed.
         */
        bool pop(T& out)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _not_empty.wait(lock, [this]
                {
                    // Wait until the queue is not empty or closed
                    return _exit || (_size > 0); });

            if (_size == 0)
                return false; // closed and empty

            out = std::move(_queue.front());
            _queue.pop();
            --_size;

            lock.unlock();
            _not_full.notify_one();
            return true;
        }

        /**
         * @brief Pops an element with a timeout.
         *
         * @tparam Rep, Period Duration parameters.
         * @param out Reference to store the popped element.
         * @param timeout The maximum time to wait for an element.
         * @return True if popped successfully, false if timeout or closed-and-empty.
         */
        template <class Rep, class Period>
        bool pop(T& out, const std::chrono::duration<Rep, Period>& timeout)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (!_not_empty.wait_for(lock, timeout, [this]
                { return _exit || (_size > 0); }))
            {
                // Timed out
                return false;
            }

            if (_size == 0)
                return false;

            out = std::move(_queue.front());
            _queue.pop();
            --_size;

            lock.unlock();
            _not_full.notify_one();
            return true;
        }

        /**
         * @brief Pushes multiple elements in bulk.
         *        Blocks if there is not enough capacity (when capacity > 0).
         *
         * @tparam InputIterator Iterator type for the input range.
         * @param first The iterator to the first element to insert.
         * @param count Number of elements to push.
         * @return True if bulk push succeeded, false if the queue is closed.
         */
        template <class InputIterator>
        bool push_bulk(InputIterator first, size_t count)
        {
            if (count == 0)
                return true;
            if (_capacity != 0 && count > _capacity)
                return false;

            std::unique_lock<std::mutex> lock(_mutex);
            _not_full.wait(lock, [this, count]
                { return _exit || (_capacity == 0 || _size + count <= _capacity); });

            if (_exit)
                return false;

            for (size_t i = 0; i < count; ++i)
            {
                _queue.push(*first++);
                ++_size;
            }

            lock.unlock();
            _not_empty.notify_all();
            return true;
        }

        /**
         * @brief Bulk push with a timeout.
         *
         * @tparam InputIterator Iterator type for the input range.
         * @tparam Rep, Period Duration parameters.
         * @param first The iterator to the first element.
         * @param count Number of elements to push.
         * @param timeout The maximum time to wait for enough capacity.
         * @return True on success, false if timeout or queue is closed.
         */
        template <class InputIterator, class Rep, class Period>
        bool push_bulk(InputIterator first, size_t count,
            const std::chrono::duration<Rep, Period>& timeout)
        {
            if (count == 0)
                return true;
            if (_capacity != 0 && count > _capacity)
                return false;

            std::unique_lock<std::mutex> lock(_mutex);
            if (!_not_full.wait_for(lock, timeout, [this, count]
                { return _exit || (_capacity == 0 || _size + count <= _capacity); }))
            {
                return false; // Timed out
            }

            if (_exit)
                return false;

            for (size_t i = 0; i < count; ++i)
            {
                _queue.push(*first++);
                ++_size;
            }

            lock.unlock();
            _not_empty.notify_all();
            return true;
        }

        /**
         * @brief Pops multiple elements in bulk. Blocks if the queue is empty.
         *
         * @tparam OutputIterator Iterator type for output.
         * @param dest The output iterator where the popped elements will be placed.
         * @param maxCount The maximum number of elements to pop.
         * @return The actual number of elements popped.
         */
        template <class OutputIterator>
        size_t pop_bulk(OutputIterator dest, size_t maxCount)
        {
            if (maxCount == 0)
                return 0;
            std::unique_lock<std::mutex> lock(_mutex);
            _not_empty.wait(lock, [this]
                { return _exit || (_size > 0); });

            if (_size == 0)
                return 0;

            size_t toPop = (maxCount < _size) ? maxCount : _size;
            for (size_t i = 0; i < toPop; ++i)
            {
                *dest++ = std::move(_queue.front());
                _queue.pop();
                --_size;
            }

            lock.unlock();
            _not_full.notify_all();
            return toPop;
        }

        /**
         * @brief Bulk pop with a timeout.
         *
         * @tparam OutputIterator Output iterator type.
         * @tparam Rep, Period Duration parameters.
         * @param dest The output iterator to store the popped elements.
         * @param maxCount The maximum number of elements to pop.
         * @param timeout The maximum time to wait for elements.
         * @return The actual number of elements popped. May be 0 on timeout or closed-and-empty.
         */
        template <class OutputIterator, class Rep, class Period>
        size_t pop_bulk(OutputIterator dest, size_t maxCount,
            const std::chrono::duration<Rep, Period>& timeout)
        {
            if (maxCount == 0)
                return 0;
            std::unique_lock<std::mutex> lock(_mutex);
            if (!_not_empty.wait_for(lock, timeout, [this]
                { return _exit || (_size > 0); }))
            {
                return 0;
            }

            if (_size == 0)
                return 0;

            size_t toPop = (maxCount < _size) ? maxCount : _size;
            for (size_t i = 0; i < toPop; ++i)
            {
                *dest++ = std::move(_queue.front());
                _queue.pop();
                --_size;
            }

            lock.unlock();
            _not_full.notify_all();
            return toPop;
        }

        /**
         * @brief Non-blocking push.
         *        Immediately returns false if the queue is full or closed.
         *
         * @tparam U The type of the element being pushed.
         * @param value The element to push.
         * @return True if pushed successfully, false if full or closed.
         */
        template <class U>
        bool try_push(U&& value)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_exit || (_capacity > 0 && _size >= _capacity))
                return false;

            _queue.push(std::forward<U>(value));
            ++_size;
            _not_empty.notify_one();
            return true;
        }

        /**
         * @brief Non-blocking pop.
         *        Immediately returns false if the queue is empty.
         *
         * @param out Reference to store the popped element.
         * @return True if popped successfully, false if empty.
         */
        bool try_pop(T& out)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_size == 0)
                return false;

            out = std::move(_queue.front());
            _queue.pop();
            --_size;
            _not_full.notify_one();
            return true;
        }

        /**
         * @brief Non-blocking bulk push.
         *        Immediately returns false if capacity insufficient or closed.
         *
         * @tparam InputIterator Iterator type.
         * @param first Iterator to first element.
         * @param count Elements to push.
         * @return True if all elements pushed, false otherwise.
         */
        template <class InputIterator>
        bool try_push_bulk(InputIterator first, size_t count)
        {
            if (count == 0)
                return true;
            std::lock_guard<std::mutex> lock(_mutex);
            if (_exit || (_capacity > 0 && _size + count > _capacity))
                return false;

            for (size_t i = 0; i < count; ++i)
            {
                _queue.push(*first++);
                ++_size;
            }
            _not_empty.notify_all();
            return true;
        }

        /**
         * @brief Non-blocking bulk pop.
         *
         * @tparam OutputIterator Output iterator.
         * @param dest Destination iterator.
         * @param maxCount Maximum elements to pop.
         * @return Actual elements popped.
         */
        template <class OutputIterator>
        size_t try_pop_bulk(OutputIterator dest, size_t maxCount)
        {
            if (maxCount == 0)
                return 0;
            std::lock_guard<std::mutex> lock(_mutex);
            if (_size == 0)
                return 0;

            size_t toPop = (maxCount < _size) ? maxCount : _size;
            for (size_t i = 0; i < toPop; ++i)
            {
                *dest++ = std::move(_queue.front());
                _queue.pop();
                --_size;
            }
            _not_full.notify_all();
            return toPop;
        }

        /**
         * @brief Gets the current number of elements in the queue.
         *
         * @return The current size of the queue.
         */
        size_t size() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _size;
        }

        /**
         * @brief Checks if the queue is empty.
         *
         * @return True if empty, false otherwise.
         */
        bool empty() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return (_size == 0);
        }

        /**
         * @brief Returns the queue capacity setting.
         *        0 means unbounded.
         *
         * @return The configured capacity of the queue.
         */
        size_t capacity() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _capacity;
        }

    private:
        std::size_t _capacity; ///< The maximum capacity (0 = unbounded).
        std::size_t _size;     ///< Current number of elements in the queue.
        bool _exit;            ///< Indicates whether the queue is closed.

        std::queue<T> _queue; ///< Underlying std::queue for storing elements.

        mutable std::mutex _mutex;          ///< Mutex for thread-safe access.
        std::condition_variable _not_full;  ///< Condition variable to wait if the queue is full.
        std::condition_variable _not_empty; ///< Condition variable to wait if the queue is empty.
    };
} // namespace lux::cxx
