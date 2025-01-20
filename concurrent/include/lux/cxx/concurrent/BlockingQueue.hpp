#pragma once
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <chrono>
#include <cassert>
#include <utility>

namespace lux::cxx
{
    /**
     * @class BlockingQueue
     * @desc A thread-safe blocking queue implementation that supports
     *       various operations such as push, pop, and bulk operations,
     *       with optional timeout support.
     * @tparam T Type of elements stored in the queue.
     */
    template <typename T>
    class BlockingQueue
    {
    public:
        /**
         * @desc Constructs a BlockingQueue with a specified capacity.
         * @param capacity Maximum number of elements the queue can hold. Default is 64.
         */
        explicit BlockingQueue(std::size_t capacity = 64)
            : _capacity(capacity), _buffer(capacity), _head(0), _tail(0), _size(0), _exit(false)
        {
            assert(capacity > 0);
        }

        /**
         * @desc Destructor to close the queue and release resources.
         */
        ~BlockingQueue()
        {
            close();
        }

        BlockingQueue(const BlockingQueue&) = delete;
        BlockingQueue& operator=(const BlockingQueue&) = delete;
        BlockingQueue(BlockingQueue&&) = default;
        BlockingQueue& operator=(BlockingQueue&&) = default;

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
            _not_full.wait(lock, [this] {
                return _exit || (_size < _capacity);
            });

            if (_exit)
                return false;

            _buffer[_tail] = std::forward<U>(value);
            _tail = (_tail + 1) % _capacity;
            ++_size;

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
            if (!_not_full.wait_for(lock, timeout, [this] {
                    return _exit || (_size < _capacity);
                }))
            {
                return false;
            }

            if (_exit)
                return false;

            _buffer[_tail] = std::forward<U>(value);
            _tail = (_tail + 1) % _capacity;
            ++_size;

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
        bool emplace(Args&&... args)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _not_full.wait(lock, [this] {
                return _exit || (_size < _capacity);
            });

            if (_exit)
                return false;

            _buffer[_tail] = T(std::forward<Args>(args)...);
            _tail = (_tail + 1) % _capacity;
            ++_size;

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
        bool emplace(const std::chrono::duration<Rep, Period>& timeout, Args&&... args)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (!_not_full.wait_for(lock, timeout, [this] {
                    return _exit || (_size < _capacity);
                }))
            {
                return false;
            }

            if (_exit)
                return false;

            _buffer[_tail] = T(std::forward<Args>(args)...);
            _tail = (_tail + 1) % _capacity;
            ++_size;

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
            _not_empty.wait(lock, [this] {
                return _exit || (_size > 0);
            });

            if (_size == 0)
            {
                return false;
            }

            out = std::move(_buffer[_head]);
            _head = (_head + 1) % _capacity;
            --_size;

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
            if (!_not_empty.wait_for(lock, timeout, [this] {
                    return _exit || (_size > 0);
                }))
            {
                return false;
            }

            if (_size == 0)
            {
                return false;
            }

            out = std::move(_buffer[_head]);
            _head = (_head + 1) % _capacity;
            --_size;

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
            std::unique_lock<std::mutex> lock(_mutex);
            _not_full.wait(lock, [this, count] {
                return _exit || (_size + count <= _capacity);
            });

            if (_exit)
                return false;

            for (size_t i = 0; i < count; ++i)
            {
                _buffer[_tail] = *first++;
                _tail = (_tail + 1) % _capacity;
            }
            _size += count;

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
            std::unique_lock<std::mutex> lock(_mutex);
            if (!_not_full.wait_for(lock, timeout, [this, count] {
                    return _exit || (_size + count <= _capacity);
                }))
            {
                return false;
            }

            if (_exit)
                return false;

            for (size_t i = 0; i < count; ++i)
            {
                _buffer[_tail] = *first++;
                _tail = (_tail + 1) % _capacity;
            }
            _size += count;

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
            std::unique_lock<std::mutex> lock(_mutex);
            _not_empty.wait(lock, [this] {
                return _exit || (_size > 0);
            });

            if (_size == 0)
            {
                return 0;
            }

            size_t toPop = (maxCount < _size) ? maxCount : _size;
            for (size_t i = 0; i < toPop; ++i)
            {
                *dest++ = std::move(_buffer[_head]);
                _head = (_head + 1) % _capacity;
            }
            _size -= toPop;

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
            std::unique_lock<std::mutex> lock(_mutex);
            if (!_not_empty.wait_for(lock, timeout, [this] {
                    return _exit || (_size > 0);
                }))
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
                *dest++ = std::move(_buffer[_head]);
                _head = (_head + 1) % _capacity;
            }
            _size -= toPop;

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
            _buffer[_tail] = std::forward<U>(value);
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

            out = std::move(_buffer[_head]);
            _head = (_head + 1) % _capacity;
            --_size;

            _not_full.notify_one();
            return true;
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
        std::size_t _capacity; ///< Maximum capacity of the queue.
        std::vector<T> _buffer; ///< Internal buffer to store elements.
        std::size_t _head; ///< Index of the head of the queue.
        std::size_t _tail; ///< Index of the tail of the queue.
        std::size_t _size; ///< Current size of the queue.
        bool _exit; ///< Flag indicating whether the queue is closed.

        mutable std::mutex _mutex; ///< Mutex for thread-safety.
        std::condition_variable _not_full; ///< Condition variable to wait when the queue is full.
        std::condition_variable _not_empty; ///< Condition variable to wait when the queue is empty.
    };
}
