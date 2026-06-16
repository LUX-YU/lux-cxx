// Throughput + allocation benchmark for lux::cxx::ser (JSON & XML backends).
// Build RELEASE. Single-threaded; reports ops/s, ns/op, MB/s and allocations/op.
//
//   cmake -S <src> -B <bld> -DCMAKE_BUILD_TYPE=Release -DENABLE_REFLECTION=ON \
//         -DENABLE_SERIALIZATION_BENCH=ON
//   cmake --build <bld> --target serialization_bench

#include "bench_types.hpp"
#include "bench_types.serialize.hpp"

#include <lux/cxx/serialization/json.hpp>
#include <lux/cxx/serialization/xml.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>

// ---- process-wide allocation counter (single-threaded bench, plain global) ----
static std::size_t g_allocs = 0;
void* operator new(std::size_t n)             { ++g_allocs; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc(); return p; }
void* operator new[](std::size_t n)           { ++g_allocs; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc(); return p; }
void  operator delete(void* p) noexcept                 { std::free(p); }
void  operator delete[](void* p) noexcept               { std::free(p); }
void  operator delete(void* p, std::size_t) noexcept    { std::free(p); }
void  operator delete[](void* p, std::size_t) noexcept  { std::free(p); }

using namespace lux::cxx::ser;
using sclock = std::chrono::steady_clock;

static volatile std::uint64_t g_sink = 0;

template <class F>
static void measure(const char* name, std::size_t bytes_per_op, F&& f)
{
    // allocations per op (deterministic for fixed input)
    f();
    const std::size_t a0 = g_allocs;
    f();
    const std::size_t apo = g_allocs - a0;

    // throughput (time-budgeted to ~0.5s)
    for (int i = 0; i < 50; ++i) f();
    const double target = 0.5;
    std::uint64_t iters = 0; double elapsed = 0;
    const auto t0 = sclock::now();
    while (elapsed < target)
    {
        for (int k = 0; k < 500; ++k) { f(); ++iters; }
        elapsed = std::chrono::duration<double>(sclock::now() - t0).count();
    }
    const double ns   = elapsed * 1e9 / double(iters);
    const double ops  = double(iters) / elapsed;
    const double mbps = double(bytes_per_op) * double(iters) / elapsed / 1e6;
    std::printf("  %-20s %11.0f ops/s  %8.1f ns/op  %7.1f MB/s  %4zu allocs/op\n",
                name, ops, ns, mbps, apo);
}

template <class T>
static void run(const char* label, const T& obj)
{
    const std::string j = to_json(obj);
    const std::string x = to_xml(obj, "root");
    std::printf("\n[%s]   json=%zu B   xml=%zu B\n", label, j.size(), x.size());

    measure("JSON serialize",   j.size(), [&]{ std::string s = to_json(obj); g_sink += s.size(); });
    measure("JSON deserialize", j.size(), [&]{ auto r = from_json<T>(j);     g_sink += r.has_value(); });
    JsonParser jp;   // persists across calls -> amortizes the parse buffers (steady state)
    measure("JSON deser(reuse)",j.size(), [&]{ auto r = from_json<T>(jp, j); g_sink += r.has_value(); });
    measure("XML  serialize",   x.size(), [&]{ std::string s = to_xml(obj, "root"); g_sink += s.size(); });
    measure("XML  deserialize", x.size(), [&]{ auto r = from_xml<T>(x);      g_sink += r.has_value(); });
}

int main()
{
    SmallMsg sm{ 42, "GetUser", "alice", true, 1.875, 200 };

    MediumCfg mc;
    mc.name    = "gateway";
    mc.workers = 8;
    mc.primary = { "10.0.0.1", 5432, true };
    mc.replica = { "10.0.0.2", 5432, false };
    mc.tags    = { "prod", "us-east", "tier1" };

    Batch batch;
    batch.source = "sensor-array";
    batch.records.reserve(1000);
    for (int i = 0; i < 1000; ++i)
        batch.records.push_back(Record{ 1700000000LL + i, "k" + std::to_string(i), i * 1.5 });

    std::printf("lux::cxx::ser throughput (Release, single thread)\n");
    std::printf("================================================\n");
    run("SmallMsg  (6 scalar/string fields)", sm);
    run("MediumCfg (nested + small array)",   mc);
    run("Batch     (1000 records)",           batch);

    std::printf("\n(sink=%llu)\n", (unsigned long long)g_sink);
    return 0;
}
