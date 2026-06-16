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
    /**
     * @class Timer
     * @brief Schedules callbacks to fire after a delay, dispatching each onto a
     *        ThreadPool when it expires.
     *
     * @warning Lifetime: the ThreadPool passed in MUST outlive the Timer, because
     *          the dispatch thread submits to it. Conversely, a callback may hold a
     *          reference to the Timer (e.g. a self-re-arming task that calls
     *          addTimer). The destructor therefore stops the dispatch thread AND
     *          waits for every already-dispatched callback to finish before tearing
     *          down its state, so the pool can never run a callback against a
     *          destroyed Timer.
     */
    class Timer
    {
    public:
        explicit Timer(ThreadPool& pool)
            : thread_pool_(pool), stop_flag_(false) {}

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

            // The dispatch thread is joined, so no new callbacks will be handed to
            // the pool. But callbacks already submitted may still be queued or
            // running there, and they can reference *this (the test's repeated_task
            // re-arms itself via addTimer). The pool outlives this Timer, so unless
            // we wait here it could run such a callback after our members are gone
            // -> use-after-free (access violation), or hang on a half-destroyed
            // mutex. Block until every dispatched callback has finished.
            std::unique_lock<std::mutex> lock(mutex_);
            done_cv_.wait(lock, [this]() { return inflight_ == 0; });
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

                        if (auto now = std::chrono::steady_clock::now(); it->first <= now)
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

                if (task && !stop_flag_.load(std::memory_order_relaxed))
                {
                    // Register this dispatch BEFORE handing it to the pool, so the
                    // destructor's wait sees it. The callback may reference *this,
                    // and the pool outlives the Timer; tracking it is what lets
                    // ~Timer guarantee no callback runs against freed state.
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        ++inflight_;
                    }

                    // submit() throws if the pool was closed (typical during teardown:
                    // the pool may shut down while this thread is still looping). Let
                    // that escape and it propagates out of the std::thread callable ->
                    // std::terminate. Guard it so teardown is clean.
                    try
                    {
                        thread_pool_.submit(
                            [this, task = std::move(task)]() mutable
                            {
                                task();
                                // Decrement and notify under the lock so ~Timer can
                                // never destroy done_cv_ while this thread is still
                                // inside notify_all() (a condition_variable teardown
                                // race).
                                std::lock_guard<std::mutex> lock(mutex_);
                                --inflight_;
                                done_cv_.notify_all();
                            }
                        );
                    }
                    catch (...)
                    {
                        // Pool closed: the wrapper will never run, so release the
                        // in-flight count we just took before we stop dispatching.
                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            --inflight_;
                            done_cv_.notify_all();
                        }
                        break; // pool closed - stop dispatching
                    }
                }
            }
        }

        using TimerMap = std::multimap<std::chrono::steady_clock::time_point, std::function<void()>>;

        std::thread             timer_thread_;
        ThreadPool&             thread_pool_;
        std::mutex              mutex_;
        std::atomic_bool        stop_flag_;
        std::condition_variable cv_;
        TimerMap                timers_;
        std::condition_variable done_cv_;      // notified when an in-flight callback finishes
        std::size_t             inflight_ = 0; // callbacks dispatched to the pool but not yet finished
    };
}
