// orth_tree_test.cpp
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <random>
#include <string>
#include <vector>

// Adjust the header path to match your project layout.
#include "lux/cxx/container/OrthTree.hpp"

namespace
{
    // ----------------------------
    // Minimal test framework (no third-party dependencies)
    // ----------------------------
    #define LUX_TEST_ASSERT(expr)                                                      \
        do {                                                                          \
            if (!(expr)) {                                                            \
                std::cerr << "[TEST FAIL] " << __FILE__ << ":" << __LINE__            \
                          << "  expr: " << #expr << "\n";                             \
                std::exit(1);                                                         \
            }                                                                         \
        } while (0)

    struct Timer
    {
        using clock = std::chrono::steady_clock;
        clock::time_point t0;

        void start() { t0 = clock::now(); }

        double ms() const
        {
            auto t1 = clock::now();
            return std::chrono::duration<double, std::milli>(t1 - t0).count();
        }
    };

    // ----------------------------
    // Point types used in tests
    // ----------------------------
    struct Point3f
    {
        std::array<float, 3> position{};
        std::uint32_t id = 0;
        std::uint32_t payload = 0;
    };

    struct Point2f
    {
        std::array<float, 2> xy{};
        std::uint32_t id = 0;
        std::uint32_t payload = 0;
    };

    struct GetXY
    {
        const std::array<float, 2>& operator()(const Point2f& p) const noexcept { return p.xy; }
    };

    // ----------------------------
    // Utility helpers
    // ----------------------------
    template <std::size_t Dim>
    lux::cxx::Box<float, Dim> make_root_bounds(float mn = -1.0f, float mx = 1.0f)
    {
        std::array<float, Dim> a{};
        std::array<float, Dim> b{};
        for (std::size_t i = 0; i < Dim; ++i) { a[i] = mn; b[i] = mx; }
        return lux::cxx::Box<float, Dim>(a, b);
    }

    template <class VecLike, std::size_t Dim>
    bool in_box(const lux::cxx::Box<float, Dim>& box, const VecLike& p)
    {
        return box.contains(p);
    }

    template <class VecLikeA, class VecLikeB, std::size_t Dim>
    bool in_ball(const VecLikeA& p, const VecLikeB& c, float r)
    {
        const float r2 = r * r;
        float d2 = 0.0f;
        for (std::size_t i = 0; i < Dim; ++i)
        {
            float d = float(p[i]) - float(c[i]);
            d2 += d * d;
            if (d2 > r2) return false;
        }
        return d2 <= r2;
    }

    template <class PointT>
    void sort_unique_ids(std::vector<std::uint32_t>& ids)
    {
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    }

    // ----------------------------
    // Random data generation
    // ----------------------------
    std::vector<Point3f> gen_points3(std::size_t n, std::uint64_t seed, float mn=-1.0f, float mx=1.0f)
    {
        std::mt19937_64 rng(seed);
        std::uniform_real_distribution<float> dist(mn, mx);

        std::vector<Point3f> pts;
        pts.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            Point3f p;
            p.position = { dist(rng), dist(rng), dist(rng) };
            p.id = static_cast<std::uint32_t>(i + 1);
            p.payload = static_cast<std::uint32_t>(i);
            pts.push_back(p);
        }
        return pts;
    }

    std::vector<Point2f> gen_points2(std::size_t n, std::uint64_t seed, float mn=-1.0f, float mx=1.0f)
    {
        std::mt19937_64 rng(seed);
        std::uniform_real_distribution<float> dist(mn, mx);

        std::vector<Point2f> pts;
        pts.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            Point2f p;
            p.xy = { dist(rng), dist(rng) };
            p.id = static_cast<std::uint32_t>(i + 1);
            p.payload = static_cast<std::uint32_t>(i);
            pts.push_back(p);
        }
        return pts;
    }

    // ----------------------------
    // Command-line argument parsing
    // ----------------------------
    struct Args
    {
        std::size_t n_points = 200'000; // default point count
        std::size_t n_queries = 2'000;  // default query count
        std::uint64_t seed = 12345;
        bool run_2d = true;
        bool run_3d = true;
    };

    Args parse_args(int argc, char** argv)
    {
        Args a;
        for (int i = 1; i < argc; ++i)
        {
            const char* s = argv[i];
            auto starts = [&](const char* p) {
                return std::strncmp(s, p, std::strlen(p)) == 0;
            };

            if (starts("--n="))
                a.n_points = std::stoull(s + 4);
            else if (starts("--q="))
                a.n_queries = std::stoull(s + 4);
            else if (starts("--seed="))
                a.seed = std::stoull(s + 7);
            else if (std::strcmp(s, "--2d-only") == 0)
                a.run_3d = false, a.run_2d = true;
            else if (std::strcmp(s, "--3d-only") == 0)
                a.run_2d = false, a.run_3d = true;
            else if (std::strcmp(s, "--help") == 0)
            {
                std::cout
                    << "Usage:\n"
                    << "  test_orthtree [--n=NUM_POINTS] [--q=NUM_QUERIES] [--seed=SEED]\n"
                    << "               [--2d-only | --3d-only]\n"
                    << "Defaults:\n"
                    << "  --n=200000  --q=2000  --seed=12345\n";
                std::exit(0);
            }
        }
        return a;
    }

    // ============================================================
    // Correctness test: 3D (Orthtree)
    // ============================================================
    void test_correctness_orthtree_3d()
    {
        std::cout << "[Correctness] Orthtree 3D\n";

        using Tree = lux::cxx::Orthtree<Point3f, 3, float>;
        Tree::Config cfg;
        cfg.root_bounds = make_root_bounds<3>(-1.0f, 1.0f);
        cfg.max_depth = 8;
        cfg.max_points_per_leaf = 128;

        Tree tree(cfg);

        // Insert some points.
        auto pts = gen_points3(10'000, 1);
        for (auto& p : pts)
            LUX_TEST_ASSERT(tree.insert(p));

        // Inserting an out-of-bounds point should fail.
        {
            Point3f out{};
            out.position = { 10.0f, 0.0f, 0.0f };
            out.id = 999999;
            LUX_TEST_ASSERT(!tree.insert(out));
        }

        // Query a sub-region and verify the result matches the baseline (compare id sets).
        lux::cxx::Box<float,3> region({-0.25f,-0.25f,-0.25f},{0.25f,0.25f,0.25f});

        std::vector<std::uint32_t> expect_ids;
        std::vector<std::uint32_t> got_ids;

        for (const auto& p : pts)
            if (in_box(region, p.position))
                expect_ids.push_back(p.id);

        tree.forEachPointInBox(region, [&](const Point3f& p) {
            got_ids.push_back(p.id);
        });

        sort_unique_ids<Point3f>(expect_ids);
        sort_unique_ids<Point3f>(got_ids);

        LUX_TEST_ASSERT(expect_ids == got_ids);

        // Mark points inside the box as deleted; count must match the baseline.
        std::size_t removed = tree.markDeletedInBox(region);
        LUX_TEST_ASSERT(removed == expect_ids.size());

        // Re-query the same box; should be empty (all points are now marked deleted).
        got_ids.clear();
        tree.forEachPointInBox(region, [&](const Point3f& p) {
            got_ids.push_back(p.id);
        });
        LUX_TEST_ASSERT(got_ids.empty());

        // After compaction the total alive count must remain unchanged.
        // Note: totalAlivePoints() already reflects the post-deletion alive count;
        // this just verifies that compaction itself does not alter the alive count.
        std::uint64_t alive_before = tree.totalAlivePoints();
        std::size_t compact_removed = tree.compactDirtyLeaves();
        (void)compact_removed;
        std::uint64_t alive_after = tree.totalAlivePoints();
        LUX_TEST_ASSERT(alive_before == alive_after);

        // Snapshot: with skip_empty_leaves=true no fully-dead leaf should appear.
        auto snap = tree.captureSnapshot(true);
        for (const auto& leaf : snap.leaves)
        {
            // Leaves may still contain dead points if not yet compacted,
            // but every captured leaf must have at least one alive point.
            std::size_t alive_cnt = 0;
            for (auto a : leaf.alive) if (a) ++alive_cnt;
            LUX_TEST_ASSERT(alive_cnt > 0);
        }
    }

    // ============================================================
    // Correctness test: 2D (Orthtree)
    // ============================================================
    void test_correctness_orthtree_2d()
    {
        std::cout << "[Correctness] Orthtree 2D\n";

        using Tree = lux::cxx::Orthtree<Point2f, 2, float, GetXY>;
        Tree::Config cfg;
        cfg.root_bounds = make_root_bounds<2>(-1.0f, 1.0f);
        cfg.max_depth = 8;
        cfg.max_points_per_leaf = 64;

        Tree tree(cfg, GetXY{});

        auto pts = gen_points2(10'000, 2);
        for (auto& p : pts)
            LUX_TEST_ASSERT(tree.insert(p));

        lux::cxx::Box<float,2> region({-0.5f,-0.1f},{0.2f,0.4f});

        std::vector<std::uint32_t> expect_ids;
        std::vector<std::uint32_t> got_ids;

        for (const auto& p : pts)
            if (in_box(region, p.xy))
                expect_ids.push_back(p.id);

        tree.forEachPointInBox(region, [&](const Point2f& p) {
            got_ids.push_back(p.id);
        });

        sort_unique_ids<Point2f>(expect_ids);
        sort_unique_ids<Point2f>(got_ids);

        LUX_TEST_ASSERT(expect_ids == got_ids);

        // Delete points inside a 2-D circle.
        std::array<float,2> c{0.0f, 0.0f};
        float r = 0.33f;

        std::size_t expect_removed = 0;
        for (const auto& p : pts)
            if (in_ball<std::array<float,2>, std::array<float,2>, 2>(p.xy, c, r))
                ++expect_removed;

        std::size_t removed = tree.markDeletedInBall(c, r);
        LUX_TEST_ASSERT(removed == expect_removed);

        // Compaction must not change the alive count.
        auto alive_before = tree.totalAlivePoints();
        tree.compactDirtyLeaves();
        auto alive_after = tree.totalAlivePoints();
        LUX_TEST_ASSERT(alive_before == alive_after);
    }

    // ============================================================
    // Correctness test: 3D (OrthtreePmr)
    // ============================================================
    void test_correctness_orthtree_pmr_3d()
    {
        std::cout << "[Correctness] OrthtreePmr 3D\n";

        std::pmr::unsynchronized_pool_resource pool;

        using Tree = lux::cxx::OrthtreePmr<Point3f, 3, float>;
        Tree::Config cfg;
        cfg.root_bounds = make_root_bounds<3>(-1.0f, 1.0f);
        cfg.max_depth = 8;
        cfg.max_points_per_leaf = 128;

        Tree tree(cfg, {}, &pool);

        auto pts = gen_points3(10'000, 3);
        for (auto& p : pts)
            LUX_TEST_ASSERT(tree.insert(p));

        lux::cxx::Box<float,3> region({-0.25f,-0.25f,-0.25f},{0.25f,0.25f,0.25f});

        std::size_t expect_removed = 0;
        for (const auto& p : pts)
            if (in_box(region, p.position))
                ++expect_removed;

        std::size_t removed = tree.markDeletedInBox(region);
        LUX_TEST_ASSERT(removed == expect_removed);

        auto alive_before = tree.totalAlivePoints();
        tree.compactDirtyLeaves();
        auto alive_after = tree.totalAlivePoints();
        LUX_TEST_ASSERT(alive_before == alive_after);

        // Snapshot (memory for the snapshot comes from the default resource).
        auto snap = tree.captureSnapshot(true);
        for (const auto& leaf : snap.leaves)
        {
            std::size_t alive_cnt = 0;
            for (auto a : leaf.alive) if (a) ++alive_cnt;
            LUX_TEST_ASSERT(alive_cnt > 0);
        }
    }

    // ============================================================
    // Correctness test: 2D (OrthtreePmr)
    // ============================================================
    void test_correctness_orthtree_pmr_2d()
    {
        std::cout << "[Correctness] OrthtreePmr 2D\n";

        std::pmr::unsynchronized_pool_resource pool;

        using Tree = lux::cxx::OrthtreePmr<Point2f, 2, float, GetXY>;
        Tree::Config cfg;
        cfg.root_bounds = make_root_bounds<2>(-1.0f, 1.0f);
        cfg.max_depth = 8;
        cfg.max_points_per_leaf = 64;

        Tree tree(cfg, GetXY{}, &pool);

        auto pts = gen_points2(10'000, 4);
        for (auto& p : pts)
            LUX_TEST_ASSERT(tree.insert(p));

        std::array<float,2> c{0.1f, -0.2f};
        float r = 0.5f;

        std::size_t expect_removed = 0;
        for (const auto& p : pts)
            if (in_ball<std::array<float,2>, std::array<float,2>, 2>(p.xy, c, r))
                ++expect_removed;

        std::size_t removed = tree.markDeletedInBall(c, r);
        LUX_TEST_ASSERT(removed == expect_removed);

        auto alive_before = tree.totalAlivePoints();
        tree.compactDirtyLeaves();
        auto alive_after = tree.totalAlivePoints();
        LUX_TEST_ASSERT(alive_before == alive_after);
    }

    // ============================================================
    // Performance test helper: generate random query boxes
    // ============================================================
    template <std::size_t Dim>
    std::vector<lux::cxx::Box<float, Dim>> gen_query_boxes(std::size_t q, std::uint64_t seed)
    {
        std::mt19937_64 rng(seed);
        std::uniform_real_distribution<float> cdist(-1.0f, 1.0f);
        std::uniform_real_distribution<float> sdist(0.01f, 0.15f);

        std::vector<lux::cxx::Box<float, Dim>> boxes;
        boxes.reserve(q);

        for (std::size_t i = 0; i < q; ++i)
        {
            std::array<float, Dim> c{};
            std::array<float, Dim> half{};
            for (std::size_t k = 0; k < Dim; ++k)
            {
                c[k] = cdist(rng);
                half[k] = sdist(rng);
            }

            std::array<float, Dim> mn{};
            std::array<float, Dim> mx{};
            for (std::size_t k = 0; k < Dim; ++k)
            {
                mn[k] = std::max(-1.0f, c[k] - half[k]);
                mx[k] = std::min( 1.0f, c[k] + half[k]);
            }

            boxes.emplace_back(mn, mx);
        }
        return boxes;
    }

    // ============================================================
    // Performance test: Orthtree 3D
    // ============================================================
    void perf_orthtree_3d(const Args& a)
    {
        std::cout << "\n[Performance] Orthtree 3D\n";

        using Tree = lux::cxx::Orthtree<Point3f, 3, float>;
        Tree::Config cfg;
        cfg.root_bounds = make_root_bounds<3>(-1.0f, 1.0f);
        cfg.max_depth = 10;
        cfg.max_points_per_leaf = 512;

        auto pts = gen_points3(a.n_points, a.seed);

        Tree tree(cfg);

        Timer t;

        // Insert all points.
        t.start();
        for (const auto& p : pts) tree.insert(p);
        double insert_ms = t.ms();
        std::cout << "Insert: " << a.n_points << " points, " << insert_ms << " ms, "
                  << (double(a.n_points) / (insert_ms / 1000.0)) << " pts/s\n";

        // Query.
        auto boxes = gen_query_boxes<3>(a.n_queries, a.seed + 111);

        std::uint64_t total_hits = 0;
        t.start();
        for (const auto& b : boxes)
        {
            tree.forEachPointInBox(b, [&](const Point3f&) { ++total_hits; });
        }
        double query_ms = t.ms();
        std::cout << "Query: " << a.n_queries << " boxes, " << query_ms << " ms, "
                  << (double(a.n_queries) / (query_ms / 1000.0)) << " queries/s, "
                  << "total_hits=" << total_hits << "\n";

        // Delete points inside a fixed box.
        lux::cxx::Box<float,3> del_box({-0.3f,-0.3f,-0.3f},{0.3f,0.3f,0.3f});
        t.start();
        std::size_t removed = tree.markDeletedInBox(del_box);
        double del_ms = t.ms();
        std::cout << "Delete(box): removed=" << removed << ", " << del_ms << " ms\n";

        // Compact.
        t.start();
        std::size_t compact_removed = tree.compactDirtyLeaves();
        double compact_ms = t.ms();
        std::cout << "Compact: removed=" << compact_removed << ", " << compact_ms << " ms\n";

        // Snapshot.
        t.start();
        auto snap = tree.captureSnapshot(true);
        double snap_ms = t.ms();
        std::cout << "Snapshot: leaves=" << snap.leaves.size() << ", " << snap_ms << " ms\n";
    }

    // ============================================================
    // Performance test: OrthtreePmr 3D
    // ============================================================
    void perf_orthtree_pmr_3d(const Args& a)
    {
        std::cout << "\n[Performance] OrthtreePmr 3D (PMR)\n";

        std::pmr::unsynchronized_pool_resource pool;

        using Tree = lux::cxx::OrthtreePmr<Point3f, 3, float>;
        Tree::Config cfg;
        cfg.root_bounds = make_root_bounds<3>(-1.0f, 1.0f);
        cfg.max_depth = 10;
        cfg.max_points_per_leaf = 512;

        auto pts = gen_points3(a.n_points, a.seed);

        Tree tree(cfg, {}, &pool);

        Timer t;

        // Insert all points.
        t.start();
        for (const auto& p : pts) tree.insert(p);
        double insert_ms = t.ms();
        std::cout << "Insert: " << a.n_points << " points, " << insert_ms << " ms, "
                  << (double(a.n_points) / (insert_ms / 1000.0)) << " pts/s\n";

        // Query.
        auto boxes = gen_query_boxes<3>(a.n_queries, a.seed + 222);

        std::uint64_t total_hits = 0;
        t.start();
        for (const auto& b : boxes)
        {
            tree.forEachPointInBox(b, [&](const Point3f&) { ++total_hits; });
        }
        double query_ms = t.ms();
        std::cout << "Query: " << a.n_queries << " boxes, " << query_ms << " ms, "
                  << (double(a.n_queries) / (query_ms / 1000.0)) << " queries/s, "
                  << "total_hits=" << total_hits << "\n";

        // Delete points inside a fixed box.
        lux::cxx::Box<float,3> del_box({-0.3f,-0.3f,-0.3f},{0.3f,0.3f,0.3f});
        t.start();
        std::size_t removed = tree.markDeletedInBox(del_box);
        double del_ms = t.ms();
        std::cout << "Delete(box): removed=" << removed << ", " << del_ms << " ms\n";

        // Compact.
        t.start();
        std::size_t compact_removed = tree.compactDirtyLeaves();
        double compact_ms = t.ms();
        std::cout << "Compact: removed=" << compact_removed << ", " << compact_ms << " ms\n";

        // Snapshot
        t.start();
        auto snap = tree.captureSnapshot(true);
        double snap_ms = t.ms();
        std::cout << "Snapshot: leaves=" << snap.leaves.size() << ", " << snap_ms << " ms\n";
    }

    // Optional 2-D performance test (complement to the 3-D version above).
    void perf_orthtree_2d(const Args& a)
    {
        std::cout << "\n[Performance] Orthtree 2D\n";

        using Tree = lux::cxx::Orthtree<Point2f, 2, float, GetXY>;
        Tree::Config cfg;
        cfg.root_bounds = make_root_bounds<2>(-1.0f, 1.0f);
        cfg.max_depth = 12;
        cfg.max_points_per_leaf = 256;

        auto pts = gen_points2(a.n_points, a.seed + 10);

        Tree tree(cfg, GetXY{});

        Timer t;

        t.start();
        for (const auto& p : pts) tree.insert(p);
        double insert_ms = t.ms();
        std::cout << "Insert: " << a.n_points << " points, " << insert_ms << " ms, "
                  << (double(a.n_points) / (insert_ms / 1000.0)) << " pts/s\n";

        auto boxes = gen_query_boxes<2>(a.n_queries, a.seed + 333);

        std::uint64_t total_hits = 0;
        t.start();
        for (const auto& b : boxes)
        {
            tree.forEachPointInBox(b, [&](const Point2f&) { ++total_hits; });
        }
        double query_ms = t.ms();
        std::cout << "Query: " << a.n_queries << " boxes, " << query_ms << " ms, "
                  << (double(a.n_queries) / (query_ms / 1000.0)) << " queries/s, "
                  << "total_hits=" << total_hits << "\n";

        lux::cxx::Box<float,2> del_box({-0.3f,-0.3f},{0.3f,0.3f});
        t.start();
        std::size_t removed = tree.markDeletedInBox(del_box);
        double del_ms = t.ms();
        std::cout << "Delete(box): removed=" << removed << ", " << del_ms << " ms\n";

        t.start();
        std::size_t compact_removed = tree.compactDirtyLeaves();
        double compact_ms = t.ms();
        std::cout << "Compact: removed=" << compact_removed << ", " << compact_ms << " ms\n";

        t.start();
        auto snap = tree.captureSnapshot(true);
        double snap_ms = t.ms();
        std::cout << "Snapshot: leaves=" << snap.leaves.size() << ", " << snap_ms << " ms\n";
    }

    void perf_orthtree_pmr_2d(const Args& a)
    {
        std::cout << "\n[Performance] OrthtreePmr 2D (PMR)\n";

        std::pmr::unsynchronized_pool_resource pool;

        using Tree = lux::cxx::OrthtreePmr<Point2f, 2, float, GetXY>;
        Tree::Config cfg;
        cfg.root_bounds = make_root_bounds<2>(-1.0f, 1.0f);
        cfg.max_depth = 12;
        cfg.max_points_per_leaf = 256;

        auto pts = gen_points2(a.n_points, a.seed + 20);

        Tree tree(cfg, GetXY{}, &pool);

        Timer t;

        t.start();
        for (const auto& p : pts) tree.insert(p);
        double insert_ms = t.ms();
        std::cout << "Insert: " << a.n_points << " points, " << insert_ms << " ms, "
                  << (double(a.n_points) / (insert_ms / 1000.0)) << " pts/s\n";

        auto boxes = gen_query_boxes<2>(a.n_queries, a.seed + 444);

        std::uint64_t total_hits = 0;
        t.start();
        for (const auto& b : boxes)
        {
            tree.forEachPointInBox(b, [&](const Point2f&) { ++total_hits; });
        }
        double query_ms = t.ms();
        std::cout << "Query: " << a.n_queries << " boxes, " << query_ms << " ms, "
                  << (double(a.n_queries) / (query_ms / 1000.0)) << " queries/s, "
                  << "total_hits=" << total_hits << "\n";

        lux::cxx::Box<float,2> del_box({-0.3f,-0.3f},{0.3f,0.3f});
        t.start();
        std::size_t removed = tree.markDeletedInBox(del_box);
        double del_ms = t.ms();
        std::cout << "Delete(box): removed=" << removed << ", " << del_ms << " ms\n";

        t.start();
        std::size_t compact_removed = tree.compactDirtyLeaves();
        double compact_ms = t.ms();
        std::cout << "Compact: removed=" << compact_removed << ", " << compact_ms << " ms\n";

        // Snapshot
        t.start();
        auto snap = tree.captureSnapshot(true);
        double snap_ms = t.ms();
        std::cout << "Snapshot: leaves=" << snap.leaves.size() << ", " << snap_ms << " ms\n";
    }
}

int main(int argc, char** argv)
{
    const auto args = parse_args(argc, argv);

    std::cout << "==== Orthtree / OrthtreePmr Tests ====\n";
    std::cout << "Config: n_points=" << args.n_points
              << "  n_queries=" << args.n_queries
              << "  seed=" << args.seed << "\n\n";

    // -------- Correctness --------
    if (args.run_3d)
    {
        test_correctness_orthtree_3d();
        test_correctness_orthtree_pmr_3d();
    }
    if (args.run_2d)
    {
        test_correctness_orthtree_2d();
        test_correctness_orthtree_pmr_2d();
    }

    std::cout << "\nAll correctness tests passed.\n";

    if (args.run_3d)
    {
        perf_orthtree_3d(args);
        perf_orthtree_pmr_3d(args);
    }
    if (args.run_2d)
    {
        perf_orthtree_2d(args);
        perf_orthtree_pmr_2d(args);
    }

    std::cout << "\nDone.\n";
    return 0;
}