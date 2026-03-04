// ============================================================================
// object_pool_benchmark.cpp
// ----------------------------------------------------------------------------
// Benchmark: ObjectPool vs raw new/delete
// Measures single-threaded and multi-threaded throughput.
// ============================================================================
#include <iostream>
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <string>
#include <cstdio>
#include <numeric>

#include <lux/cxx/memory/ObjectPool.hpp>

// ─── helpers ────────────────────────────────────────────────────────────────

using Clock = std::chrono::high_resolution_clock;

struct SmallObj  { int x; int y; };                              // 8 bytes
struct MediumObj { char data[64]; int id; };                     // 68 bytes
struct LargeObj  { char data[256]; double values[8]; int id; };  // ~324 bytes

template <typename Func>
double measure_ms(Func&& fn)
{
    auto t0 = Clock::now();
    fn();
    auto t1 = Clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

static void print_result(const char* label, double pool_ms, double raw_ms, int ops)
{
    double speedup = raw_ms / pool_ms;
    double pool_ns = pool_ms * 1e6 / ops;
    double raw_ns  = raw_ms  * 1e6 / ops;
    std::printf("  %-40s  pool: %7.2f ms (%5.1f ns/op)  raw: %7.2f ms (%5.1f ns/op)  speedup: %.2fx\n",
                label, pool_ms, pool_ns, raw_ms, raw_ns, speedup);
}

// ─── 1. Sequential acquire/release (warm pool) ─────────────────────────────
//    Acquire one, release one, repeat. Free list always has a slot.

template <typename T>
void bench_sequential(const char* name, int ops)
{
    lux::cxx::ObjectPool<T, 64, false> pool;
    pool.reserve(1);

    double pool_ms = measure_ms([&]
    {
        for (int i = 0; i < ops; ++i)
        {
            auto* p = pool.acquire();
            pool.release(p);
        }
    });

    double raw_ms = measure_ms([&]
    {
        for (int i = 0; i < ops; ++i)
        {
            auto* p = new T{};
            delete p;
        }
    });

    print_result(name, pool_ms, raw_ms, ops);
}

// ─── 2. Batch acquire then batch release ────────────────────────────────────
//    Tests throughput when many objects are alive simultaneously.

template <typename T>
void bench_batch(const char* name, int batch_size)
{
    lux::cxx::ObjectPool<T, 256, false> pool;
    pool.reserve(batch_size);

    std::vector<T*> ptrs(batch_size);

    double pool_ms = measure_ms([&]
    {
        for (int i = 0; i < batch_size; ++i)
            ptrs[i] = pool.acquire();
        for (int i = 0; i < batch_size; ++i)
            pool.release(ptrs[i]);
    });

    double raw_ms = measure_ms([&]
    {
        for (int i = 0; i < batch_size; ++i)
            ptrs[i] = new T{};
        for (int i = 0; i < batch_size; ++i)
            delete ptrs[i];
    });

    print_result(name, pool_ms, raw_ms, batch_size);
}

// ─── 3. Interleaved acquire/release (realistic pattern) ─────────────────────
//    Acquire N objects, release half, acquire half, release all.

template <typename T>
void bench_interleaved(const char* name, int ops)
{
    int half = ops / 2;
    lux::cxx::ObjectPool<T, 256, false> pool;
    pool.reserve(ops);

    std::vector<T*> ptrs(ops);

    double pool_ms = measure_ms([&]
    {
        for (int i = 0; i < ops; ++i)
            ptrs[i] = pool.acquire();
        for (int i = 0; i < half; ++i)
            pool.release(ptrs[i]);
        for (int i = 0; i < half; ++i)
            ptrs[i] = pool.acquire();
        for (int i = 0; i < ops; ++i)
            pool.release(ptrs[i]);
    });

    double raw_ms = measure_ms([&]
    {
        for (int i = 0; i < ops; ++i)
            ptrs[i] = new T{};
        for (int i = 0; i < half; ++i)
            delete ptrs[i];
        for (int i = 0; i < half; ++i)
            ptrs[i] = new T{};
        for (int i = 0; i < ops; ++i)
            delete ptrs[i];
    });

    print_result(name, pool_ms, raw_ms, ops * 2);
}

// ─── 4. Multi-threaded (lock-free pool vs raw new/delete) ───────────────────

template <typename T>
void bench_mt(const char* name, int threads_n, int ops_per_thread)
{
    // Lock-free pool
    lux::cxx::ObjectPool<T, 256, true> pool;
    pool.reserve(threads_n * 64);

    double pool_ms = measure_ms([&]
    {
        std::vector<std::thread> threads;
        threads.reserve(threads_n);
        for (int t = 0; t < threads_n; ++t)
        {
            threads.emplace_back([&]
            {
                for (int i = 0; i < ops_per_thread; ++i)
                {
                    auto* p = pool.acquire();
                    pool.release(p);
                }
            });
        }
        for (auto& th : threads) th.join();
    });

    double raw_ms = measure_ms([&]
    {
        std::vector<std::thread> threads;
        threads.reserve(threads_n);
        for (int t = 0; t < threads_n; ++t)
        {
            threads.emplace_back([&]
            {
                for (int i = 0; i < ops_per_thread; ++i)
                {
                    auto* p = new T{};
                    delete p;
                }
            });
        }
        for (auto& th : threads) th.join();
    });

    int total_ops = threads_n * ops_per_thread;
    print_result(name, pool_ms, raw_ms, total_ops);
}

// ─── 5. pool_ptr overhead vs raw acquire/release ───────────────────────────

template <typename T>
void bench_pool_ptr(const char* name, int ops)
{
    lux::cxx::ObjectPool<T, 64, false> pool;
    pool.reserve(1);

    double scoped_ms = measure_ms([&]
    {
        for (int i = 0; i < ops; ++i)
        {
            auto sp = pool.acquire_scoped();
        }
    });

    double raw_pool_ms = measure_ms([&]
    {
        for (int i = 0; i < ops; ++i)
        {
            auto* p = pool.acquire();
            pool.release(p);
        }
    });

    double speedup = raw_pool_ms / scoped_ms;
    double scoped_ns  = scoped_ms  * 1e6 / ops;
    double raw_ns     = raw_pool_ms * 1e6 / ops;
    std::printf("  %-40s  pool_ptr: %7.2f ms (%5.1f ns/op)  raw_pool: %7.2f ms (%5.1f ns/op)  ratio: %.2fx\n",
                name, scoped_ms, scoped_ns, raw_pool_ms, raw_ns, speedup);
}

// ─── main ───────────────────────────────────────────────────────────────────

int main()
{
    constexpr int N  = 1'000'000;
    constexpr int MT_OPS = 500'000;

    std::cout << "========================================================\n";
    std::cout << "  ObjectPool Benchmark  (N = " << N << ")\n";
    std::cout << "========================================================\n\n";

    std::cout << "--- Sequential acquire/release (warm pool, single slot) ---\n";
    bench_sequential<SmallObj> ("SmallObj (8B) sequential",  N);
    bench_sequential<MediumObj>("MediumObj (68B) sequential", N);
    bench_sequential<LargeObj> ("LargeObj (~324B) sequential", N);

    std::cout << "\n--- Batch acquire-all then release-all ---\n";
    bench_batch<SmallObj> ("SmallObj (8B) batch 100K",  100'000);
    bench_batch<MediumObj>("MediumObj (68B) batch 100K", 100'000);
    bench_batch<LargeObj> ("LargeObj (~324B) batch 100K", 100'000);

    std::cout << "\n--- Interleaved acquire/release (realistic) ---\n";
    bench_interleaved<SmallObj> ("SmallObj (8B) interleaved 100K",  100'000);
    bench_interleaved<MediumObj>("MediumObj (68B) interleaved 100K", 100'000);

    std::cout << "\n--- Multi-threaded (lock-free pool vs raw new/delete) ---\n";
    int hw = std::thread::hardware_concurrency();
    std::printf("  hardware_concurrency = %d\n", hw);
    bench_mt<SmallObj> ("SmallObj 4T sequential",  4, MT_OPS);
    bench_mt<SmallObj> ("SmallObj 8T sequential",  8, MT_OPS);
    bench_mt<MediumObj>("MediumObj 4T sequential", 4, MT_OPS);
    bench_mt<MediumObj>("MediumObj 8T sequential", 8, MT_OPS);

    std::cout << "\n--- pool_ptr overhead vs raw acquire/release ---\n";
    bench_pool_ptr<SmallObj> ("SmallObj pool_ptr overhead",  N);
    bench_pool_ptr<MediumObj>("MediumObj pool_ptr overhead", N);

    std::cout << "\n========================================================\n";
    std::cout << "  Done.\n";
    std::cout << "========================================================\n";
    return 0;
}
