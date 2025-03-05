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
#include <thread>
#include <future>
#include <functional>
#include <memory>
#include <type_traits>
#include "BlockingQueue.hpp"
#include <lux/cxx/compile_time/move_only_function.hpp>

#define ENABLE_EXCEPTIONS 1

#ifdef ENABLE_EXCEPTIONS
#include <exception>
#endif

namespace lux::cxx
{
    class ThreadPool
    {
    public:
        // 使用 move_only_function<void()> 作为任务类型
        using Task = move_only_function<void()>;

        explicit ThreadPool(size_t threadCount = 10, size_t queueCapacity = 64)
            : _tasks(queueCapacity)
        {
            // 启动工作线程
            for (size_t i = 0; i < threadCount; ++i)
            {
                _workers.emplace_back([this] {
                    workerThreadFunc();
                });
            }
        }

        // 不允许拷贝或移动
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        ~ThreadPool()
        {
            close();
        }

        void join()
        {
            for (auto& th : _workers)
            {
                if (th.joinable()) {
                    th.join();
                }
            }
        }

        void close()
        {
            _tasks.close();
            join();
        }

        /**
         * @brief 提交一个可调用对象到线程池，返回 std::future 用于获取结果
         * @tparam Func  函数或可调用对象
         * @tparam Args  参数类型
         * @return std::future<RetType> 用于获取任务执行结果
         */
        template <typename Func, typename... Args,
                  typename RetType = std::invoke_result_t<Func, Args...>>
        std::future<RetType> submit(Func&& func, Args&&... args)
        {
			std::promise<RetType> promise;
			std::future<RetType> future = promise.get_future();
			Task wrapper_task = [promise = std::move(promise), func = std::forward<Func>(func), args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
#ifdef ENABLE_EXCEPTIONS
                try {
					if constexpr (std::is_void_v<RetType>) {
						std::apply(func, args);
						promise.set_value();
					} else {
						promise.set_value(std::apply(func, args));
					}
				} catch (...) {
					promise.set_exception(std::current_exception());
				}
#else
                if constexpr (std::is_void_v<RetType>) {
                    std::apply(func, args);
                    promise.set_value();
                }
                else {
                    promise.set_value(std::apply(func, args));
                }
#endif
			};

    		if(! _tasks.push(std::move(wrapper_task)))
        		throw std::runtime_error("ThreadPool queue may closed.");

            return future;
        }

    private:
        void workerThreadFunc()
        {
            Task task; // move_only_function<void()>
            while (_tasks.pop(task))
            {
                // 调用任务
                task();
            }
        }

    private:
        std::vector<std::thread> _workers;
        BlockingQueue<Task>      _tasks; // 队列里存放 move_only_function<void()>
    };
} // namespace lux::cxx