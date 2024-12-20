#pragma once
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>

namespace lux::cxx
{
	template<typename T, class _Container = std::deque<T>>
	class BlockingQueue
	{
	public:
		explicit BlockingQueue(size_t capacity = 64)
			: _capacity(capacity), _exit(false) {}

		~BlockingQueue()
		{
			if (!_exit)
			{
				close();
			}
		}

		BlockingQueue(BlockingQueue&&) = default;
		BlockingQueue& operator=(BlockingQueue&&) = default;

		void close()
		{
			_exit = true;
			_cv.notify_all();
		}

		[[nodiscard]] bool closed() const
		{
			return _exit;
		}

		[[nodiscard]] bool pop(T& item)
		{
			std::unique_lock<std::mutex> lock(_mutex);
			while (_queue.empty() && !_exit) {
				_cv.wait(lock);
			}

			if (_exit) {
				return false;
			}

			if (!_queue.empty())
			{
				item = std::move(_queue.front());
				_queue.pop();
				_cv.notify_one();
				return true;
			}

			return false;
		}

		template<class OutputIterator>
		[[nodiscard]] size_t pop_bulk(OutputIterator dest, size_t maxCount)
		{
			std::unique_lock<std::mutex> lock(_mutex);
			size_t count = 0;
			while (count < maxCount && !_queue.empty() && !_exit)
			{
				*dest = std::move(_queue.front());
				_queue.pop();
				++dest;
				++count;
			}

			if (count > 0)
			{
				_cv.notify_one();
			}

			return count;
		}

		[[nodiscard]] bool try_pop(T& item) 
		{
			std::scoped_lock lock(_mutex);

			if (!_queue.empty()) {
				item = std::move(_queue.front());
				_queue.pop();
				_cv.notify_one();
				return true;
			}
			return false;
		}

		template<class OutputIterator>
		size_t try_pop_bulk(OutputIterator dest, size_t maxCount)
		{
			std::scoped_lock lock(_mutex);
			size_t count = 0;
			while (count < maxCount && !_queue.empty() && !_exit)
			{
				*dest = std::move(_queue.front());
				_queue.pop();
				++dest;
				++count;
			}

			if (count > 0)
			{
				_cv.notify_one();
			}

			return count;
		}

		template<class U>
		[[nodiscard]] bool try_push(U&& element)
		{
			{
				std::scoped_lock lock(_mutex);

				if (_exit)
					return false;

				if (_queue.size() < _capacity) {
					_queue.push(std::forward<U>(element));
					_cv.notify_one();
					return true;
				}
			}
			return false;
		}

		template<class U>
		bool push(U&& element)
		{
			{
				std::unique_lock<std::mutex> lock(_mutex);
				while (_queue.size() >= _capacity && !_exit) {
					_cv.wait(lock);
				}

				if (_exit)
					return false;

				_queue.push(std::forward<U>(element));
			}
			_cv.notify_one();

			return true;
		}

		template<typename InputIterator>
		bool push_bulk(InputIterator itemFirst, size_t count)
		{
			std::unique_lock<std::mutex> lock(_mutex);
			while (_queue.size() + count > _capacity && !_exit) {
				_cv.wait(lock);
			}

			if (_exit) {
				return false;
			}

			for (size_t i = 0; i < count; ++i)
			{
				_queue.push(*itemFirst);
				++itemFirst;
			}

			_cv.notify_all();
			return true;
		}

		template<typename InputIterator>
		bool try_push_bulk(InputIterator itemFirst, size_t count)
		{
			std::scoped_lock lock(_mutex);

			if (_queue.size() + count > _capacity || _exit) {
				return false;
			}

			for (size_t i = 0; i < count; ++i)
			{
				_queue.push(*itemFirst);
				++itemFirst;
			}

			_cv.notify_all();
			return true;
		}

		[[nodiscard]] bool empty() const
		{
			std::scoped_lock lock(_mutex);
			return _queue.empty();
		}

		[[nodiscard]] size_t size() const
		{
			std::scoped_lock lock(_mutex);
			return _queue.size();
		}

	private:
		std::size_t					_capacity;
		std::condition_variable		_cv;
		mutable  std::mutex			_mutex;
		std::atomic_bool			_exit;
		std::queue<T, _Container>   _queue;
	};
}
