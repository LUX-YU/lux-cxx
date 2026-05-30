#pragma once
// ============================================================================
// Shared benchmark scaffolding used by archtype_bench.cpp and entt_bench.cpp.
// Both files include this header so the methodology is identical: same
// warmup count, same statistic (median over 5 runs), same do_not_optimize
// sink, and the same component schema.
// ============================================================================

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace bench {

    using Clock = std::chrono::high_resolution_clock;

    // ---- Shared components ------------------------------------------------
    struct Position { float x{}, y{}; Position() = default; Position(float a, float b) : x(a), y(b) {} };
    struct Velocity { float vx{}, vy{}; Velocity() = default; Velocity(float a, float b) : vx(a), vy(b) {} };
    struct Health   { int   hp{100};   Health()   = default; explicit Health(int h)    : hp(h) {} };

    // ---- Measurement ------------------------------------------------------
    struct Stats {
        double min_ms      = 0;
        double median_ms   = 0;
        double max_ms      = 0;
        int    runs        = 0;
    };

    inline Stats summarize(std::vector<double> times) {
        std::sort(times.begin(), times.end());
        Stats s;
        s.runs      = static_cast<int>(times.size());
        s.min_ms    = times.front();
        s.median_ms = times[times.size() / 2];
        s.max_ms    = times.back();
        return s;
    }

    /// Self-contained measurement: f() must reset any state it mutates.
    template<class F>
    Stats measure(F&& f, int runs = 5, int warmup = 1) {
        for (int i = 0; i < warmup; ++i) f();
        std::vector<double> times;
        times.reserve(runs);
        for (int i = 0; i < runs; ++i) {
            const auto t0 = Clock::now();
            f();
            const auto t1 = Clock::now();
            times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }
        return summarize(std::move(times));
    }

    /// Two-phase measurement: build fresh state with @p setup_fn (untimed),
    /// then time @p work_fn(state). Setup runs once per measured iteration so
    /// destructive workloads (create / migration / churn) can rerun cleanly
    /// without state from a previous iteration leaking in.
    template<class SetupF, class WorkF>
    Stats measure_setup_work(SetupF setup_fn, WorkF work_fn, int runs = 5, int warmup = 1) {
        for (int i = 0; i < warmup; ++i) {
            auto s = setup_fn();
            work_fn(s);
        }
        std::vector<double> times;
        times.reserve(runs);
        for (int i = 0; i < runs; ++i) {
            auto s = setup_fn();
            const auto t0 = Clock::now();
            work_fn(s);
            const auto t1 = Clock::now();
            times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }
        return summarize(std::move(times));
    }

    // ---- Reporting --------------------------------------------------------
    inline void header(const char* tag, int N) {
        std::printf("\n=== %s, N = %d  (median of 5 runs, after 1 warmup) ===\n", tag, N);
    }

    inline void report(const char* label, const Stats& s) {
        std::printf("  %-50s  median %8.3f ms   min %8.3f   max %8.3f\n",
                    label, s.median_ms, s.min_ms, s.max_ms);
    }

    inline void report(const char* label, const Stats& s, std::size_t entities) {
        const double ns = (entities == 0) ? 0.0 : s.median_ms * 1e6 / static_cast<double>(entities);
        std::printf("  %-50s  median %8.3f ms   min %8.3f   max %8.3f   (%.2f ns/entity)\n",
                    label, s.median_ms, s.min_ms, s.max_ms, ns);
    }

    inline void report_count(const char* label, std::size_t count) {
        std::printf("  %-50s  %zu\n", label, count);
    }

    // ---- Do-not-optimize --------------------------------------------------
    // Forces the compiler to materialize @p v in memory and emits a compiler
    // fence, preventing dead-code elimination of work whose result we throw
    // away. Modeled after Google Benchmark's pattern.
    template<class T>
    inline void do_not_optimize(T const& v) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        asm volatile("" : : "r,m"(v) : "memory");
#else
        const volatile char& sink = *reinterpret_cast<const volatile char*>(&v);
        (void)sink;
        std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
    }

    // ---- Optional cache flusher ------------------------------------------
    /// Walks a ~16 MB buffer to evict the L1/L2 working set before measurement.
    class CacheFlusher {
    public:
        CacheFlusher() : buf_(kSize / sizeof(int), 0) {}
        void flush() {
            volatile int sink = 0;
            const int step = 16; // every 64-byte line
            for (std::size_t i = 0; i < buf_.size(); i += step) sink += buf_[i];
            do_not_optimize(sink);
        }
    private:
        static constexpr std::size_t kSize = 16 * 1024 * 1024;
        std::vector<int> buf_;
    };

} // namespace bench
