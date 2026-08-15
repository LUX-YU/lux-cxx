// ============================================================================
// container_bench.cpp
// ----------------------------------------------------------------------------
// Microbenchmarks for the lux containers against peers:
//   * SmallVector<int,N>          vs std::vector<int>
//   * SparseSet / BasicSparseSet / SlotMap  vs std::unordered_map
//   * (optional, LUX_BENCH_WITH_ENTT) BasicSparseSet vs entt::storage / sparse_set
//
// Build (Release!) via the container/test CMake gate:
//   -DENABLE_CONTAINER_BENCH=ON                 (std/self comparisons)
//   -DENABLE_CONTAINER_BENCH=ON -DENABLE_COMPARE_WITH_ENTT=ON   (adds EnTT section)
// Uses the shared bench_common.hpp harness (median of 5, do_not_optimize sink).
// Each section asserts correctness (matching sums) BEFORE timing.
// ============================================================================
#include "bench_common.hpp"   // from archtype/test (added as an include dir)

#include <lux/cxx/container/SmallVector.hpp>
#include <lux/cxx/container/SparseSet.hpp>
#include <lux/cxx/container/BasicSparseSet.hpp>
#include <lux/cxx/container/SlotMap.hpp>
#include <lux/cxx/container/StableSlotMap.hpp>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <random>
#include <memory>
#include <unordered_map>
#include <vector>

#if defined(LUX_BENCH_WITH_ENTT)
#  include <entt/entt.hpp>
#endif

using bench::Position;
using bench::measure;
using bench::measure_setup_work;
using bench::report;
using bench::do_not_optimize;

// ---------------------------------------------------------------------------
// SmallVector<int,N> vs std::vector<int>
// ---------------------------------------------------------------------------
static void bench_small_vector()
{
    std::printf("\n############ SmallVector vs std::vector ############\n");

    // (1) MANY-SMALL: build a fresh vector of M ints, sum, discard, K times.
    //     SmallVector's win is eliding the first heap allocation when M <= N.
    for (int M : { 8, 64 })
    {
        const int K = 300000;
        bench::header("many-small fresh-vector churn (build M ints + sum)", M);

        auto sv16 = measure([&] {
            long long acc = 0;
            for (int k = 0; k < K; ++k) {
                lux::cxx::SmallVector<int, 16> v;
                for (int i = 0; i < M; ++i) v.push_back(i);
                for (std::size_t i = 0; i < v.size(); ++i) acc += v[i];
            }
            do_not_optimize(acc);
        });
        report("lux::SmallVector<int,16>", sv16, std::size_t(K) * M);

        auto stdv = measure([&] {
            long long acc = 0;
            for (int k = 0; k < K; ++k) {
                std::vector<int> v;
                for (int i = 0; i < M; ++i) v.push_back(i);
                for (std::size_t i = 0; i < v.size(); ++i) acc += v[i];
            }
            do_not_optimize(acc);
        });
        report("std::vector<int>", stdv, std::size_t(K) * M);
    }

    // (2) GROW one big vector then ITERATE (expect rough parity — both contiguous).
    {
        const int N = 1000000;
        bench::header("grow push_back then sum", N);
        auto sv = measure([&] {
            lux::cxx::SmallVector<int, 16> v;
            for (int i = 0; i < N; ++i) v.push_back(i);
            long long acc = 0; for (std::size_t i = 0; i < v.size(); ++i) acc += v[i];
            do_not_optimize(acc);
        });
        report("lux::SmallVector<int,16>", sv, N);
        auto stdv = measure([&] {
            std::vector<int> v;
            for (int i = 0; i < N; ++i) v.push_back(i);
            long long acc = 0; for (std::size_t i = 0; i < v.size(); ++i) acc += v[i];
            do_not_optimize(acc);
        });
        report("std::vector<int>", stdv, N);
    }
}

// ---------------------------------------------------------------------------
// Associative: SparseSet / BasicSparseSet / SlotMap vs std::unordered_map
// Dense keys 0..N-1, value = Position. (Sparse/huge keys would blow up the
// dense-array sets' memory — see the report note — so we benchmark the dense
// case the lux sets are designed for.)
// ---------------------------------------------------------------------------
static void bench_associative()
{
    std::printf("\n############ SparseSet / BasicSparseSet / SlotMap vs unordered_map (dense keys) ############\n");

    using u32 = std::uint32_t;
    std::mt19937 rng(123);

    for (int N : { 100000, 1000000 })
    {
        // shuffled lookup order (shared shape across containers)
        std::vector<u32> order(N);
        for (int i = 0; i < N; ++i) order[i] = u32(i);
        std::shuffle(order.begin(), order.end(), std::mt19937(123));

        bench::header("INSERT N (dense keys)", N);

        report("lux::SparseSet", measure_setup_work(
            [] { return lux::cxx::SparseSet<u32, Position>{}; },
            [&](auto& s) { for (int i = 0; i < N; ++i) s.insert(u32(i), Position(float(i), 0)); }), N);
        report("lux::BasicSparseSet", measure_setup_work(
            [] { return lux::cxx::BasicSparseSet<u32, Position>{}; },
            [&](auto& s) { for (int i = 0; i < N; ++i) s.insert(u32(i), Position(float(i), 0)); }), N);
        report("lux::SlotMap", measure_setup_work(
            [] { return lux::cxx::SlotMap<Position>{}; },
            [&](auto& s) { for (int i = 0; i < N; ++i) (void)s.insert(Position(float(i), 0)); }), N);
        report("std::unordered_map", measure_setup_work(
            [] { return std::unordered_map<u32, Position>{}; },
            [&](auto& m) { for (int i = 0; i < N; ++i) m.emplace(u32(i), Position(float(i), 0)); }), N);

        // ----- ITERATE: sum Position.x over all elements -----
        bench::header("ITERATE (sum x over all elements)", N);
        {
            lux::cxx::SparseSet<u32, Position> ss;       for (int i = 0; i < N; ++i) ss.insert(u32(i), Position(float(i), 0));
            lux::cxx::BasicSparseSet<u32, Position> bs;  for (int i = 0; i < N; ++i) bs.insert(u32(i), Position(float(i), 0));
            lux::cxx::SlotMap<Position> sm;              for (int i = 0; i < N; ++i) (void)sm.insert(Position(float(i), 0));
            std::unordered_map<u32, Position> um;        um.reserve(N); for (int i = 0; i < N; ++i) um.emplace(u32(i), Position(float(i), 0));

            const double ref = double(N) * (N - 1) / 2.0;   // sum of 0..N-1
            auto sum_ss = [&] { double a = 0; for (auto& p : ss.values()) a += p.x; do_not_optimize(a); return a; };
            auto sum_bs = [&] { double a = 0; for (auto& p : bs.values()) a += p.x; do_not_optimize(a); return a; };
            auto sum_sm = [&] { double a = 0; for (auto& p : sm.values()) a += p.x; do_not_optimize(a); return a; };
            auto sum_um = [&] { double a = 0; for (auto& kv : um) a += kv.second.x; do_not_optimize(a); return a; };
            assert(sum_ss() == ref && sum_bs() == ref && sum_sm() == ref && sum_um() == ref);

            report("lux::SparseSet       values()", measure([&] { do_not_optimize(sum_ss()); }), N);
            report("lux::BasicSparseSet  values()", measure([&] { do_not_optimize(sum_bs()); }), N);
            report("lux::SlotMap         values()", measure([&] { do_not_optimize(sum_sm()); }), N);
            report("std::unordered_map   buckets ", measure([&] { do_not_optimize(sum_um()); }), N);
        }

        // ----- RANDOM-LOOKUP: tryGet/find each key in shuffled order -----
        bench::header("RANDOM-LOOKUP (shuffled keys)", N);
        {
            lux::cxx::SparseSet<u32, Position> ss;       for (int i = 0; i < N; ++i) ss.insert(u32(i), Position(float(i), 0));
            lux::cxx::BasicSparseSet<u32, Position> bs;  for (int i = 0; i < N; ++i) bs.insert(u32(i), Position(float(i), 0));
            std::unordered_map<u32, Position> um;        um.reserve(N); for (int i = 0; i < N; ++i) um.emplace(u32(i), Position(float(i), 0));
            // SlotMap owns its key space: capture its keys in insertion order.
            lux::cxx::SlotMap<Position> sm;
            std::vector<decltype(sm.insert(Position{}))> keys; keys.reserve(N);
            for (int i = 0; i < N; ++i) keys.push_back(sm.insert(Position(float(i), 0)));

            report("lux::SparseSet       tryGet", measure([&] {
                double a = 0; for (u32 k : order) a += ss.tryGet(k)->x; do_not_optimize(a); }), N);
            report("lux::BasicSparseSet  tryGet", measure([&] {
                double a = 0; for (u32 k : order) a += bs.tryGet(k)->x; do_not_optimize(a); }), N);
            report("lux::SlotMap         find",   measure([&] {
                double a = 0; for (u32 k : order) a += sm.find(keys[k])->x; do_not_optimize(a); }), N);
            report("std::unordered_map   find",   measure([&] {
                double a = 0; for (u32 k : order) a += um.find(k)->second.x; do_not_optimize(a); }), N);
        }
    }
}

// ---------------------------------------------------------------------------
// Stable-address storage: block allocation vs one heap allocation per value.
// Both baselines preserve pointee addresses while their owner grows.
// ---------------------------------------------------------------------------
static void bench_stable_storage()
{
    std::printf("\n############ StableSlotMap vs per-element heap allocation ############\n");
    for (int N : {100000, 1000000})
    {
        bench::header("INSERT stable-address values", N);
        report("lux::StableSlotMap", measure_setup_work(
            [] { return lux::cxx::StableSlotMap<Position>{}; },
            [&](auto& values)
            {
                values.reserve(static_cast<std::size_t>(N));
                for (int i = 0; i < N; ++i)
                {
                    values.emplace(float(i), 0.0F);
                }
            }
        ), N);
        report("vector<unique_ptr<Position>>", measure_setup_work(
            [] { return std::vector<std::unique_ptr<Position>>{}; },
            [&](auto& values)
            {
                values.reserve(static_cast<std::size_t>(N));
                for (int i = 0; i < N; ++i)
                {
                    values.push_back(std::make_unique<Position>(float(i), 0.0F));
                }
            }
        ), N);

        lux::cxx::StableSlotMap<Position> stable;
        stable.reserve(static_cast<std::size_t>(N));
        std::vector<std::unique_ptr<Position>> individual;
        individual.reserve(static_cast<std::size_t>(N));
        for (int i = 0; i < N; ++i)
        {
            stable.emplace(float(i), 0.0F);
            individual.push_back(std::make_unique<Position>(float(i), 0.0F));
        }

        bench::header("ITERATE stable-address values", N);
        report("lux::StableSlotMap", measure([&]
        {
            double sum = 0;
            for (const auto& value : stable) sum += value.x;
            do_not_optimize(sum);
        }), N);
        report("vector<unique_ptr<Position>>", measure([&]
        {
            double sum = 0;
            for (const auto& value : individual) sum += value->x;
            do_not_optimize(sum);
        }), N);
    }
}

#if defined(LUX_BENCH_WITH_ENTT)
// ---------------------------------------------------------------------------
// lux::BasicSparseSet<u32,Position> vs entt::storage<Position> (dense keys).
// NOTE: entt uses PAGED sparse storage, so for huge/sparse keys entt wins on
// memory; this dense-key comparison isolates raw insert/iterate/lookup cost.
// ---------------------------------------------------------------------------
static void bench_vs_entt()
{
    std::printf("\n############ lux::BasicSparseSet vs entt::storage<Position> (dense keys) ############\n");
    using u32 = std::uint32_t;

    for (int N : { 100000, 1000000 })
    {
        std::vector<u32> order(N);
        for (int i = 0; i < N; ++i) order[i] = u32(i);
        std::shuffle(order.begin(), order.end(), std::mt19937(123));

        bench::header("INSERT N (dense)", N);
        report("lux::BasicSparseSet", measure_setup_work(
            [] { return lux::cxx::BasicSparseSet<u32, Position>{}; },
            [&](auto& s) { for (int i = 0; i < N; ++i) s.insert(u32(i), Position(float(i), 0)); }), N);
        report("entt::storage<Position>", measure_setup_work(
            [] { return entt::storage<Position>{}; },
            [&](auto& s) { for (int i = 0; i < N; ++i) s.emplace(entt::entity(i), float(i), 0.f); }), N);

        // ITERATE
        bench::header("ITERATE (sum x)", N);
        {
            lux::cxx::BasicSparseSet<u32, Position> bs; for (int i = 0; i < N; ++i) bs.insert(u32(i), Position(float(i), 0));
            entt::storage<Position> es;                 for (int i = 0; i < N; ++i) es.emplace(entt::entity(i), float(i), 0.f);
            report("lux::BasicSparseSet values()", measure([&] {
                double a = 0; for (auto& p : bs.values()) a += p.x; do_not_optimize(a); }), N);
            report("entt::storage each()", measure([&] {
                double a = 0; for (auto&& [e, p] : es.each()) a += p.x; do_not_optimize(a); }), N);
        }

        // RANDOM-LOOKUP
        bench::header("RANDOM-LOOKUP (shuffled)", N);
        {
            lux::cxx::BasicSparseSet<u32, Position> bs; for (int i = 0; i < N; ++i) bs.insert(u32(i), Position(float(i), 0));
            entt::storage<Position> es;                 for (int i = 0; i < N; ++i) es.emplace(entt::entity(i), float(i), 0.f);
            report("lux::BasicSparseSet tryGet", measure([&] {
                double a = 0; for (u32 k : order) a += bs.tryGet(k)->x; do_not_optimize(a); }), N);
            report("entt::storage get", measure([&] {
                double a = 0; for (u32 k : order) a += es.get(entt::entity(k)).x; do_not_optimize(a); }), N);
        }
    }
}
#endif

int main()
{
    std::printf("==================================================================\n");
    std::printf(" lux::cxx container microbenchmark (Release, median of 5)\n");
#if defined(LUX_BENCH_WITH_ENTT)
    std::printf(" EnTT comparison: ENABLED\n");
#else
    std::printf(" EnTT comparison: disabled (configure with -DENABLE_COMPARE_WITH_ENTT=ON)\n");
#endif
    std::printf("==================================================================\n");

    bench_small_vector();
    bench_associative();
    bench_stable_storage();
#if defined(LUX_BENCH_WITH_ENTT)
    bench_vs_entt();
#endif
    return 0;
}
