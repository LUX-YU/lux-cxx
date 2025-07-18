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
     */
    class ThreadPool
    {
        using RawTask = move_only_function<void()>;  ///< Type-erased task function

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

            if (!_tasks.push(std::move(wrapped)))
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

            if (!_tasks.push(std::move(wrapped)))
                throw std::runtime_error("ThreadPool queue closed");

            return fut;
        }

        /**
         * @brief Closes the thread pool, preventing new tasks from being submitted
         * 
         * This method closes the task queue and requests all worker threads to stop
         * after they finish their current tasks. It then waits for all workers to join.
         */
        void close()
        {
            _tasks.close();
            for (auto& w : _workers)
                w.request_stop();
            join();
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
            }
        }

        std::vector<std::jthread> _workers;  ///< Collection of worker threads
        BlockingQueue<RawTask>    _tasks;    ///< Thread-safe queue of pending tasks
    };
}