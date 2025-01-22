#pragma once

#include <vector>
#include <thread>
#include <future>
#include <functional>
#include <memory>
#include <type_traits>
#include "BlockingQueue.hpp"
#include <lux/cxx/compile_time/move_only_function.hpp>

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