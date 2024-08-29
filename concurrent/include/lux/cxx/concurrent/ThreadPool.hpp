#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <future>
#include <optional>
#include <type_traits>

namespace lux::cxx
{
	class ThreadPool
	{
	public:
		using Worker = std::thread;

		explicit ThreadPool(const size_t threads = 10) : stop(false)
		{
			for (size_t i = 0; i < threads; ++i) {
				workers.emplace_back(
					[this]()
					{
						this->worker_inner_thread();
					}
				);
			}
		}

		ThreadPool(const ThreadPool&) = delete;
		ThreadPool& operator=(const ThreadPool&) = delete;

		ThreadPool(ThreadPool&& other) noexcept = delete;
		ThreadPool& operator=(ThreadPool&& other) noexcept = delete;

		~ThreadPool()
		{
			stop = true;
			condition.notify_all();
			for (std::thread& worker : workers) {
				if (worker.joinable())
				{
					worker.join();
				}
			}
		}

		template<typename Func, typename... Args, typename RetType = std::invoke_result_t<Func, Args...>>
		auto pushTask(Func&& func, Args&&... args) -> std::future<RetType>
		{
			auto task = new std::packaged_task<RetType()>(
				[args = std::make_tuple(std::forward<Args>(args)...), _func = std::forward<Func>(func)]() mutable -> RetType {
					return std::apply(std::move(_func), std::move(args));
				}
			);

			auto future = task->get_future();

			{
				std::scoped_lock lock(tasks_mutex);
				tasks.emplace(
					[task]()
					{
						(*task)();
						delete task;
					}
				);
			}

			condition.notify_all();
			return future;
		}

	private:

		void worker_inner_thread()
		{
			while (!stop)
			{
				std::unique_lock lock(this->tasks_mutex);

				this->condition.wait(
					lock,
					[this]() {
						return this->stop || !this->tasks.empty();
					}
				);

				if (this->stop && this->tasks.empty()) {
					return;
				}

				auto task = std::move(this->tasks.front());
				this->tasks.pop();

				lock.unlock();
				task();
			}
		}

		std::vector<Worker>					workers;
		std::queue<std::function<void()>>	tasks;
		std::mutex							tasks_mutex;
		std::condition_variable				condition;
		std::atomic_bool					stop;
	};
}