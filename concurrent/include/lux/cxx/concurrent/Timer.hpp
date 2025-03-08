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
			if (timer_thread_.joinable())
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
                	thread_pool_.submit(std::move(task));
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