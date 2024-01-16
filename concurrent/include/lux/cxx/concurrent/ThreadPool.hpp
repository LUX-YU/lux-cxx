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

		ThreadPool(size_t threads = 10) : stop(false)
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

		template<typename Func, typename... Args,
			typename RetType = typename std::invoke_result<Func, Args...>::type>
		auto pushTask(Func&& func, Args&&... args) -> std::future<RetType>
		{
			auto bind_lambda =
				[...args = std::forward<Args>(args), _func = std::forward<Func>(func)]
				() -> RetType
				{
					return _func(args...);
				};

			auto task = new std::packaged_task<RetType()>(std::move(bind_lambda));

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
				std::unique_lock<std::mutex> lock(this->tasks_mutex);

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

		std::vector<Worker> workers;
		std::queue<std::function<void()>> tasks;

		std::mutex tasks_mutex;
		std::condition_variable condition;
		std::atomic_bool stop;
	};
}