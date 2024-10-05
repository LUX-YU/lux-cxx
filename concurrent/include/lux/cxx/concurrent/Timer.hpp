#pragma once
#include <map>
#include "ThreadPool.hpp"

namespace lux::cxx
{
	class Timer
    {
	public:
    	explicit Timer(ThreadPool& pool)
          : thread_pool_(pool), stop_flag_(false){}

        Timer(const Timer&) = delete;
        Timer& operator=(const Timer&) = delete;

        ~Timer()
        {
        	{
            	std::scoped_lock lock(mutex_);
            	stop_flag_ = true;
        	}
        	cv_.notify_all();
        	timer_thread_.join();
        }

		void addTimer(const int delay_ms, std::function<void()> task)
        {
        	{
		        auto expiry_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
		        std::lock_guard<std::mutex> lock(mutex_);
            	timers_.emplace(expiry_time, std::move(task));
        	}
        	cv_.notify_all();
    	}

		void start()
		{
        	timer_thread_ = std::thread([this]() { timer_thread(); });
        }

    private:
      	void timer_thread()
    	{
        	while (true)
          	{
            	std::function<void()> task;

            	{
                	std::unique_lock<std::mutex> lock(mutex_);
                	if (stop_flag_) break;

            		if (timers_.empty())
                    {
                    	cv_.wait(lock);
                	}
                    else
                    {
	                    const auto it = timers_.begin();

	                    if(auto now = std::chrono::steady_clock::now(); it->first <= now)
                        {
                        	task = it->second;
                        	timers_.erase(it);
                    	}
                        else
                        {
                        	cv_.wait_until(lock, it->first);
                    	}
                	}
            	}

            	if (task)
                {
                	thread_pool_.pushTask(std::move(task));
            	}
        	}
    	}

      	using TimerMap = std::multimap<std::chrono::steady_clock::time_point, std::function<void()>>;

   		std::thread 			timer_thread_;
      	ThreadPool& 			thread_pool_;
        std::mutex 				mutex_;
        std::atomic_bool 		stop_flag_;
    	std::condition_variable cv_;
        TimerMap 				timers_;
   	};
}