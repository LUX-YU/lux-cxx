#pragma once
/*
 * Copyright (c) 2026 Chenhui Yu
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

#include <atomic>
#include <future>
#include <stop_token>
#include <utility>
#include <vector>

#include "ThreadPool.hpp"

namespace lux::cxx
{
    /**
     * @brief Run @p fn(i) exactly once for every i in [0, count) — in parallel
     *        on @p pool when given, as a plain inline loop otherwise.
     *
     * Scheduling: ONE task per pool worker (not per index), all pulling
     * indices from a shared atomic cursor, and the CALLING thread drains the
     * same cursor instead of sleeping — so wakeups per call are bounded by
     * the worker count, load self-balances across uneven index costs, and
     * the final future joins usually find the helpers already finished.
     * Designed for small index counts with sub-millisecond bodies (e.g. image
     * pyramid levels), where a per-index submit pays a kernel wake/sleep
     * round-trip comparable to the work item itself.
     *
     * Requirements: @p fn must be safe to invoke concurrently for DISTINCT
     * indices (it is never invoked twice for the same index). Worker
     * exceptions propagate to the caller via the future joins; note the
     * caller-thread portion runs first, so an exception from the caller's own
     * indices propagates immediately while helpers keep draining until the
     * cursor empties.
     *
     * Passing a null @p pool (or count <= 1) degrades to the inline serial
     * loop — callers can treat the pool as an optional capability.
     */
    template <typename Fn>
    void parallelForIndex(ThreadPool* pool, int count, Fn&& fn)
    {
        if (count <= 0)
            return;
        if (pool == nullptr || count == 1 || pool->worker_count() == 0)
        {
            for (int i = 0; i < count; ++i)
                fn(i);
            return;
        }

        std::atomic<int> cursor{0};
        auto drain = [&fn, &cursor, count]()
        {
            for (int i = cursor.fetch_add(1, std::memory_order_relaxed);
                 i < count;
                 i = cursor.fetch_add(1, std::memory_order_relaxed))
            {
                fn(i);
            }
        };

        std::vector<std::future<void>> pending;
        pending.reserve(pool->worker_count());
        for (std::size_t helper = 0; helper < pool->worker_count(); ++helper)
        {
            pending.push_back(
                pool->submit([&drain](std::stop_token) { drain(); }).fut);
        }
        drain();   // the calling thread works too instead of sleeping
        for (auto& f : pending)
            f.get();
    }
} // namespace lux::cxx
