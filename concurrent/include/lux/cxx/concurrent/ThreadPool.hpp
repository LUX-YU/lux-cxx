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

#include <concepts>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <stop_token>
#include <thread>      // std::jthread
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "BlockingQueue.hpp"
#include <lux/cxx/compile_time/move_only_function.hpp>

#ifndef ENABLE_EXCEPTIONS
#   define ENABLE_EXCEPTIONS 1
#   include <exception>
#endif

namespace lux::cxx
{
    enum class EThreadPoolDrainMode : std::uint8_t
    {
        DRAIN,
        ABORT,
    };

    /**
     * @brief A handle for a submitted task that allows tracking and control
     * 
     * @tparam T The return type of the task
     * 
     * This structure couples a std::future (for getting the task result) with a
     * std::stop_source (for requesting cancellation of the task). This enables
     * more control over tasks than a traditional thread pool implementation.
     */
    template <typename T>
    struct task_handle
    {
        std::future<T>   fut;  ///< Future to retrieve the task's result
        std::stop_source src;  ///< Stop source to request cancellation

        /**
         * @brief Gets the result of the task (blocks until completion)
         * @return The result of the task
         * @throws Any exception thrown by the task
         */
        T    get() { return fut.get(); }

        /**
         * @brief Checks if the future contains a valid shared state
         * @return True if the future is valid
         */
        bool valid()        const { return fut.valid(); }

        /**
         * @brief Requests cancellation of the task
         * 
         * This signals to the task that it should stop execution, but it's up to
         * the task to check for the stop token's state and respond appropriately.
         */
        void request_stop() { src.request_stop(); }

        /**
         * @brief Checks if stop has been requested for this task
         * @return True if cancellation has been requested
         */
        bool stop_requested() const { return src.stop_requested(); }
    };

    /**
     * @class ThreadPool
     * @brief A flexible thread pool implementation with task cancellation support
     * 
     * This thread pool manages a collection of worker threads that process
     * tasks from a shared queue. It supports both regular task submission and
     * cancellable tasks via stop tokens. Tasks can return values and throw
     * exceptions safely.
     *
     * @warning Bounded queue + self-submission can DEADLOCK. The task queue is
     *          bounded (queue_cap, default 64) and submit()/submit_copy() BLOCK
     *          when it is full. If a task running ON this pool submits more work
     *          to the SAME pool and the queue is full, the worker blocks waiting
     *          for space that only a worker could free — a classic dependent-task
     *          deadlock. If you submit recursively/from within tasks, construct the
     *          pool with queue_cap == 0 (unbounded) or use a separate pool for the
     *          nested work.
     */
    class ThreadPool
    {
        using RawTask = move_only_function<void()>;  ///< Type-erased task function

        /// Selects the preallocated ring for bounded pools and preserves the
        /// historical deque-backed queue only for explicitly unbounded pools.
        /// Dispatch is one predictable branch and does not use virtual calls.
        class TaskQueue final
        {
        public:
            explicit TaskQueue(std::size_t capacity)
            {
                if (capacity == 0)
                    unbounded_ = std::make_unique<BlockingQueue<RawTask>>(0);
                else
                    bounded_ = std::make_unique<BlockingRingQueue<RawTask>>(
                        capacity
                    );
            }

            [[nodiscard]] bool push(RawTask task)
            {
                return bounded_
                    ? bounded_->push(std::move(task))
                    : unbounded_->push(std::move(task));
            }

            [[nodiscard]] EQueuePushResult tryPush(RawTask task) noexcept
            {
                if (bounded_) return bounded_->tryPush(std::move(task));
                try
                {
                    return unbounded_->tryPush(std::move(task));
                }
                catch (...)
                {
                    // The legacy unbounded queue may allocate.  Detached
                    // submission remains noexcept and reports backpressure if
                    // that allocation cannot be satisfied.
                    return EQueuePushResult::FULL;
                }
            }

            [[nodiscard]] bool pop(RawTask& task)
            {
                return bounded_
                    ? bounded_->pop(task)
                    : unbounded_->pop(task);
            }

            [[nodiscard]] bool try_pop(RawTask& task) noexcept
            {
                return bounded_
                    ? bounded_->try_pop(task)
                    : unbounded_->try_pop(task);
            }

            void close() noexcept
            {
                if (bounded_)
                    bounded_->close();
                else
                    unbounded_->close();
            }

        private:
            std::unique_ptr<BlockingRingQueue<RawTask>> bounded_;
            std::unique_ptr<BlockingQueue<RawTask>> unbounded_;
        };

    public:
        /**
         * @brief Constructs a ThreadPool with the specified number of threads
         * 
         * @param thread_count Number of worker threads to create (defaults to hardware concurrency)
         * @param queue_cap Maximum capacity of the task queue (defaults to 64)
         * 
         * The ThreadPool will immediately create and start the worker threads,
         * which will begin processing tasks as they are submitted.
         */
        explicit ThreadPool(
            std::size_t thread_count = std::jthread::hardware_concurrency(),
            std::size_t queue_cap = 64)
            : _tasks(queue_cap)
        {
            for (std::size_t i = 0; i < thread_count; ++i)
            {
                _workers.emplace_back([this](std::stop_token st) {
                    worker_loop(st);
                    }
                );
            }
        }

        // Disable copying and moving
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        /**
         * @brief Destructor that closes the thread pool and joins all worker threads
         * 
         * This will automatically close the task queue and stop all worker threads,
         * waiting for them to complete their current tasks.
         */
        ~ThreadPool() { close(); }

        /**
         * @brief Submits a cancellable task to the thread pool
         * 
         * @tparam Func Type of the callable
         * @tparam Args Types of the arguments
         * @param func Function to execute that accepts a stop_token as first argument
         * @param args Additional arguments to pass to the function
         * @return task_handle containing both the future result and a stop source
         * @throws std::runtime_error if the thread pool is closed
         * 
         * This overload is for functions that explicitly accept a stop_token as their
         * first parameter, enabling cooperative cancellation.
         *
         * @warning The arguments @p args are captured **by reference** (via
         *          std::forward_as_tuple) and are dereferenced later, when a worker
         *          thread runs the task. The referenced objects MUST stay alive until
         *          the task finishes executing. Do NOT pass temporaries / rvalues or
         *          any local that may go out of scope before the task runs — doing so
         *          is a use-after-free. When in doubt, use submit_copy(), which
         *          decay-copies the arguments into the task.
         */
        template <typename Func, typename... Args>
        requires std::invocable<Func, std::stop_token, Args...>
        auto submit(Func&& func, Args&&... args)
        {
            using Ret = std::invoke_result_t<Func, std::stop_token, Args...>;

            std::promise<Ret> pr;
            auto fut = pr.get_future();

            std::stop_source src;
            std::stop_token  tok = src.get_token();

            RawTask wrapped =
                [pr = std::move(pr), func = std::forward<Func>(func), 
                tup = std::forward_as_tuple(std::forward<Args>(args)...), 
                tok] () mutable
                {
#if ENABLE_EXCEPTIONS
                    try {
#endif
                        if constexpr (std::is_void_v<Ret>)
                        {
                            std::apply(
                                [&](auto&&... xs)
                                {
                                    func(tok, std::forward<decltype(xs)>(xs)...);
                                },
                                tup
                            );
                            pr.set_value();
                        }
                        else
                        {
                            pr.set_value(
                                std::apply(
                                    [&](auto&&... xs)
                                    {
                                        return func(tok, std::forward<decltype(xs)>(xs)...);
                                    },
                                    tup
                                )
                            );
                        }
#if ENABLE_EXCEPTIONS
                    }
                    catch (...) { pr.set_exception(std::current_exception()); }
#endif
                };

            if (!enqueue(std::move(wrapped)))
                throw std::runtime_error("ThreadPool queue closed");

            return task_handle<Ret>{ std::move(fut), std::move(src) };
        }

        /**
         * @brief Submits a non-cancellable task to the thread pool
         * 
         * @tparam Func Type of the callable
         * @tparam Args Types of the arguments
         * @param func Function to execute
         * @param args Additional arguments to pass to the function
         * @return std::future containing the result of the task
         * @throws std::runtime_error if the thread pool is closed
         * 
         * This overload is for regular functions that don't accept a stop_token.
         * These tasks cannot be cancelled once submitted.
         *
         * @warning The arguments @p args are captured **by reference** (via
         *          std::forward_as_tuple) and are dereferenced later, when a worker
         *          thread runs the task. The referenced objects MUST stay alive until
         *          the task finishes executing. Do NOT pass temporaries / rvalues or
         *          any local that may go out of scope before the task runs — doing so
         *          is a use-after-free. When in doubt, use submit_copy(), which
         *          decay-copies the arguments into the task.
         */
        template <typename Func, typename... Args>
        requires (!std::invocable<Func, std::stop_token, Args...>) &&
        std::invocable<Func, Args...>
        auto submit(Func&& func, Args&&... args)
        {
            using Ret = std::invoke_result_t<Func, Args...>;
            std::promise<Ret> pr;
            auto fut = pr.get_future();

            RawTask wrapped =
                [pr = std::move(pr),
                func = std::forward<Func>(func),
                tup = std::forward_as_tuple(std::forward<Args>(args)...)]() mutable
                {
#if ENABLE_EXCEPTIONS
                    try {
#endif
                        if constexpr (std::is_void_v<Ret>)
                        {
                            std::apply(func, tup);
                            pr.set_value();
                        }
                        else
                        {
                            pr.set_value(std::apply(func, tup));
                        }
#if ENABLE_EXCEPTIONS
                    }
                    catch (...) { pr.set_exception(std::current_exception()); }
#endif
                };

            if (!enqueue(std::move(wrapped)))
                throw std::runtime_error("ThreadPool queue closed");

            return fut;
        }

        /**
         * @brief Value-semantics variant of submit() (cancellable overload).
         *
         * Behaves like submit(), but every argument is **decay-copied** into the
         * task before it is enqueued, so temporaries / rvalues / soon-to-expire
         * locals are all safe. Move-only arguments passed by value are moved into
         * the stored tuple and moved again into @p func at call time. Prefer this
         * overload unless you specifically need by-reference capture and can
         * guarantee the arguments outlive task execution.
         */
        template <typename Func, typename... Args>
        requires std::invocable<Func, std::stop_token, Args...>
        auto submit_copy(Func&& func, Args&&... args)
        {
            using Ret = std::invoke_result_t<Func, std::stop_token, Args...>;

            std::promise<Ret> pr;
            auto fut = pr.get_future();

            std::stop_source src;
            std::stop_token  tok = src.get_token();

            RawTask wrapped =
                [pr = std::move(pr), func = std::forward<Func>(func),
                tup = std::make_tuple(std::forward<Args>(args)...),
                tok]() mutable
                {
#if ENABLE_EXCEPTIONS
                    try {
#endif
                        if constexpr (std::is_void_v<Ret>)
                        {
                            std::apply(
                                [&](auto&... xs) { func(tok, std::move(xs)...); },
                                tup);
                            pr.set_value();
                        }
                        else
                        {
                            pr.set_value(std::apply(
                                [&](auto&... xs) { return func(tok, std::move(xs)...); },
                                tup));
                        }
#if ENABLE_EXCEPTIONS
                    }
                    catch (...) { pr.set_exception(std::current_exception()); }
#endif
                };

            if (!enqueue(std::move(wrapped)))
                throw std::runtime_error("ThreadPool queue closed");

            return task_handle<Ret>{ std::move(fut), std::move(src) };
        }

        /**
         * @brief Value-semantics variant of submit() (non-cancellable overload).
         * @see The cancellable submit_copy() above; arguments are decay-copied so
         *      temporaries are safe (unlike submit()).
         */
        template <typename Func, typename... Args>
        requires (!std::invocable<Func, std::stop_token, Args...>) &&
        std::invocable<Func, Args...>
        auto submit_copy(Func&& func, Args&&... args)
        {
            using Ret = std::invoke_result_t<Func, Args...>;
            std::promise<Ret> pr;
            auto fut = pr.get_future();

            RawTask wrapped =
                [pr = std::move(pr),
                func = std::forward<Func>(func),
                tup = std::make_tuple(std::forward<Args>(args)...)]() mutable
                {
#if ENABLE_EXCEPTIONS
                    try {
#endif
                        if constexpr (std::is_void_v<Ret>)
                        {
                            std::apply([&](auto&... xs) { func(std::move(xs)...); }, tup);
                            pr.set_value();
                        }
                        else
                        {
                            pr.set_value(std::apply(
                                [&](auto&... xs) { return func(std::move(xs)...); }, tup));
                        }
#if ENABLE_EXCEPTIONS
                    }
                    catch (...) { pr.set_exception(std::current_exception()); }
#endif
                };

            if (!enqueue(std::move(wrapped)))
                throw std::runtime_error("ThreadPool queue closed");

            return fut;
        }

        /**
         * @brief Gracefully closes the pool: stop accepting new tasks, let the
         *        workers drain *all* already-queued work, then join them.
         *
         * Every task that was successfully submitted before close() runs to
         * completion, so no waiting future is ever left broken. Workers are NOT
         * asked to stop — they exit naturally once the queue is closed and empty
         * (BlockingQueue::pop returns false only then). For an immediate teardown
         * that abandons still-queued tasks, use shutdown_now().
         */
        /// Number of worker threads owned by this pool.
        [[nodiscard]] std::size_t worker_count() const noexcept
        {
            return _workers.size();
        }

        /// Submit a fire-and-forget task without blocking or allocating a
        /// promise/future. Callables that do not fit RawTask's inline storage
        /// are rejected at compile time so successful submission is heap-free.
        template <class F>
        requires (
            std::is_nothrow_invocable_r_v<void, std::decay_t<F>&> &&
            std::is_nothrow_constructible_v<std::decay_t<F>, F&&> &&
            RawTask::template stores_inplace<std::decay_t<F>>)
        [[nodiscard]] EQueuePushResult trySubmitDetached(F&& function) noexcept
        {
            RawTask task{std::forward<F>(function)};
            pending_tasks_.fetch_add(1, std::memory_order_acq_rel);
            const EQueuePushResult result = _tasks.tryPush(std::move(task));
            if (result != EQueuePushResult::ACCEPTED)
                completeTask();
            return result;
        }

        /// Wait until all accepted tasks have completed. Callers must prevent
        /// concurrent producers if they require a stable shutdown boundary.
        void drain() noexcept
        {
            std::unique_lock lock{idle_mutex_};
            idle_changed_.wait(lock, [this]
            {
                return pending_tasks_.load(std::memory_order_acquire) == 0;
            });
        }

        void close(EThreadPoolDrainMode mode) noexcept
        {
            if (mode == EThreadPoolDrainMode::DRAIN)
                close();
            else
                shutdown_now();
        }

        void close()
        {
            _tasks.close();   // refuse new pushes; pop() drains the rest, then returns false
            join();           // deliberately no request_stop: workers drain before exiting
        }

        /**
         * @brief Immediately shuts the pool down, abandoning still-queued tasks.
         *
         * Requests stop on every worker so each exits right after its current task
         * instead of draining the queue. Tasks that never started have their
         * promises destroyed unfulfilled, so their futures observe
         * std::future_error(broken_promise). Use close() for a graceful drain.
         */
        void shutdown_now()
        {
            _tasks.close();
            for (auto& w : _workers)
                w.request_stop();
            join();
            RawTask abandoned;
            while (_tasks.try_pop(abandoned))
            {
                abandoned.reset();
                completeTask();
            }
        }

        /**
         * @brief Waits for all worker threads to complete
         * 
         * This method joins all worker threads, blocking until they have all finished.
         * It should typically be called after close() or when shutting down the thread pool.
         */
        void join()
        {
            for (auto& th : _workers)
                if (th.joinable()) th.join();
        }

    private:
        /**
         * @brief Main worker thread function that processes tasks from the queue
         * 
         * @param st Stop token that signals when the worker should terminate
         * 
         * This function continuously pulls tasks from the queue and executes them
         * until either the stop token is activated or the queue is closed and empty.
         */
        void worker_loop(std::stop_token st)
        {
            RawTask task;
            while (!st.stop_requested() && _tasks.pop(task))
            {
                task();
                task.reset();
                completeTask();
            }
        }

        [[nodiscard]] bool enqueue(RawTask task)
        {
            pending_tasks_.fetch_add(1, std::memory_order_acq_rel);
            if (_tasks.push(std::move(task))) return true;
            completeTask();
            return false;
        }

        void completeTask() noexcept
        {
            if (pending_tasks_.fetch_sub(1, std::memory_order_acq_rel) != 1)
                return;
            std::lock_guard lock{idle_mutex_};
            idle_changed_.notify_all();
        }

        // Declaration order matters: _tasks must outlive _workers so that workers
        // (which pop from _tasks) are destroyed first. ~ThreadPool also calls
        // close()/join() before any member is destroyed, so workers are already
        // finished by the time these are torn down.
        TaskQueue                 _tasks;    ///< Selected bounded/unbounded task queue
        std::vector<std::jthread> _workers;  ///< Collection of worker threads
        std::atomic<std::size_t>  pending_tasks_{0};
        std::mutex                idle_mutex_;
        std::condition_variable   idle_changed_;
    };
}
