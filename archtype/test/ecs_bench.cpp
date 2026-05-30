// ============================================================================
// ecs_bench.cpp  —  FAIR side-by-side benchmark: lux::cxx::archtype vs EnTT
//
// Why this file exists (replaces the older, misleading benchmarks):
//
//   The previous archtype benchmark measured the WRONG things:
//     - timed queries that only collected entity ids, never touching component
//       data (so it never measured chunk SoA iteration — archetype's only edge);
//     - random getComponent (archetype's worst access pattern);
//     - compared archtype's .each against EnTT's *view*, not its *owning group*.
//
//   EnTT owning groups physically pack the owned component pools into the same
//   contiguous order, giving near-archetype SoA iteration. Comparing against a
//   plain view overstated archtype's advantage. This benchmark compares against
//   BOTH entt::view and entt::group, and also measures the operations archetype
//   is genuinely SLOWER at (add/remove component, structural churn) so the
//   picture is honest in both directions.
//
//   Methodology (shared via bench_common.hpp): median of 5 runs after 1 warmup,
//   do_not_optimize sink, two-phase setup/work timing for destructive ops,
//   identical component structs and identical per-entity arithmetic on each side.
// ============================================================================

#include "bench_common.hpp"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include <entt/entt.hpp>

#include <lux/cxx/archtype/Registry.hpp>
#include <lux/cxx/archtype/Group.hpp>
#include <lux/cxx/archtype/Index.hpp>

namespace ax = lux::cxx::archtype;
using bench::Position;
using bench::Velocity;
using bench::Health;
using bench::Stats;

static constexpr float DT = 1.0f / 60.0f;

// ---- reporting -------------------------------------------------------------
static void section(const char* title, int N) {
    std::printf("\n--- %s   [N = %d] ---\n", title, N);
}

static void row(const char* lib, const char* what, const Stats& s, std::size_t ents) {
    const double ns = ents ? s.median_ms * 1e6 / double(ents) : 0.0;
    std::printf("    %-8s %-28s %9.4f ms   %8.2f ns/op\n", lib, what, s.median_ms, ns);
}

// Headline comparison. speedup = entt/archtype; >1 means archtype faster.
static void key(const char* desc, const Stats& archtype, const Stats& entt_) {
    const double sp = entt_.median_ms / archtype.median_ms;
    if (sp >= 1.0)
        std::printf("    >> %-34s archtype %.2fx FASTER\n", desc, sp);
    else
        std::printf("    >> %-34s entt     %.2fx FASTER\n", desc, 1.0 / sp);
}

template<class Reg, class Ent>
struct Scene {
    std::unique_ptr<Reg> reg;
    std::vector<Ent>     es;
};

// ============================================================================
//  S1 — iterate homogeneous {P,V}, read 2 + write 2 per entity
//       (entt owning group's best case; the most important honest head-to-head)
// ============================================================================
static void s1_iterate_homogeneous(int N) {
    section("S1 iterate homogeneous {P,V}, read+write", N);

    // archtype
    ax::Registry ar; ar.reserve(N);
    for (int i = 0; i < N; ++i)
        ar.create<Position, Velocity>(Position{(float)i, 0.f}, Velocity{1.f, 1.f});

    auto a_each = bench::measure([&] {
        long long acc = 0;
        ar.view<Position, Velocity>().each([&](Position& p, Velocity& v) {
            p.x += v.vx * DT; p.y += v.vy * DT; acc += (long long)p.x;
        });
        bench::do_not_optimize(acc);
    });
    auto a_chunk = bench::measure([&] {
        long long acc = 0;
        ar.view<Position, Velocity>().for_each_chunk(
            [&](std::size_t n, ax::Entity*, Position* ps, Velocity* vs) {
                for (std::size_t i = 0; i < n; ++i) {
                    ps[i].x += vs[i].vx * DT; ps[i].y += vs[i].vy * DT; acc += (long long)ps[i].x;
                }
            });
        bench::do_not_optimize(acc);
    });

    // entt
    entt::registry er;
    er.storage<Position>().reserve(N); er.storage<Velocity>().reserve(N);
    for (int i = 0; i < N; ++i) {
        auto e = er.create();
        er.emplace<Position>(e, (float)i, 0.f);
        er.emplace<Velocity>(e, 1.f, 1.f);
    }
    auto e_view = bench::measure([&] {
        long long acc = 0;
        er.view<Position, Velocity>().each([&](Position& p, Velocity& v) {
            p.x += v.vx * DT; p.y += v.vy * DT; acc += (long long)p.x;
        });
        bench::do_not_optimize(acc);
    });
    auto grp = er.group<Position, Velocity>();   // OWNING group — entt's fastest iteration
    auto e_group = bench::measure([&] {
        long long acc = 0;
        grp.each([&](Position& p, Velocity& v) {
            p.x += v.vx * DT; p.y += v.vy * DT; acc += (long long)p.x;
        });
        bench::do_not_optimize(acc);
    });

    row("archtype", "view.each",          a_each,  N);
    row("archtype", "for_each_chunk",     a_chunk, N);
    row("entt",     "view.each",          e_view,  N);
    row("entt",     "group.each (owning)", e_group, N);
    key("each   vs entt.view :",  a_each,  e_view);
    key("each   vs entt.group:",  a_each,  e_group);
    key("chunk  vs entt.group:",  a_chunk, e_group);
}

// ============================================================================
//  S2 — iterate fragmented: 1/3 {P,V}, 1/3 {P,V,H}, 1/3 {P}; query <P,V>
// ============================================================================
static void s2_iterate_fragmented(int N) {
    section("S2 iterate fragmented (query <P,V> matches 2/3)", N);
    const std::size_t match = (std::size_t)N * 2 / 3;

    ax::Registry ar; ar.reserve(N);
    for (int i = 0; i < N; ++i) {
        if (i % 3 == 0)      ar.create<Position, Velocity>(Position{(float)i,0}, Velocity{1,1});
        else if (i % 3 == 1) ar.create<Position, Velocity, Health>(Position{(float)i,0}, Velocity{1,1}, Health{1});
        else                 ar.create<Position>(Position{(float)i,0});
    }
    auto a_each = bench::measure([&] {
        long long acc = 0;
        ar.view<Position, Velocity>().each([&](Position& p, Velocity& v) {
            p.x += v.vx * DT; acc += (long long)p.x;
        });
        bench::do_not_optimize(acc);
    });

    entt::registry er;
    er.storage<Position>().reserve(N); er.storage<Velocity>().reserve(N);
    for (int i = 0; i < N; ++i) {
        auto e = er.create();
        if (i % 3 == 0)      { er.emplace<Position>(e,(float)i,0.f); er.emplace<Velocity>(e,1.f,1.f); }
        else if (i % 3 == 1) { er.emplace<Position>(e,(float)i,0.f); er.emplace<Velocity>(e,1.f,1.f); er.emplace<Health>(e,1); }
        else                 { er.emplace<Position>(e,(float)i,0.f); }
    }
    auto e_view = bench::measure([&] {
        long long acc = 0;
        er.view<Position, Velocity>().each([&](Position& p, Velocity& v) {
            p.x += v.vx * DT; acc += (long long)p.x;
        });
        bench::do_not_optimize(acc);
    });
    auto grp = er.group<Position, Velocity>();
    auto e_group = bench::measure([&] {
        long long acc = 0;
        grp.each([&](Position& p, Velocity& v) {
            p.x += v.vx * DT; acc += (long long)p.x;
        });
        bench::do_not_optimize(acc);
    });

    row("archtype", "view.each",           a_each,  match);
    row("entt",     "view.each",           e_view,  match);
    row("entt",     "group.each (owning)",  e_group, match);
    key("each vs entt.view :",  a_each, e_view);
    key("each vs entt.group:",  a_each, e_group);
}

// ============================================================================
//  S3 — bulk create {P,V}
// ============================================================================
static void s3_create(int N) {
    section("S3 bulk create {P,V}", N);

    auto a_multi = bench::measure([&] {
        ax::Registry ar; ar.reserve(N);
        for (int i = 0; i < N; ++i)
            ar.create<Position, Velocity>(Position{(float)i,0}, Velocity{1,1});
    });
    auto a_legacy = bench::measure([&] {
        ax::Registry ar; ar.reserve(N);
        for (int i = 0; i < N; ++i) {
            auto e = ar.createEntity();
            ar.addComponent<Position>(e, (float)i, 0.f);
            ar.addComponent<Velocity>(e, 1.f, 1.f);
        }
    });
    auto e_create = bench::measure([&] {
        entt::registry er;
        er.storage<Position>().reserve(N); er.storage<Velocity>().reserve(N);
        for (int i = 0; i < N; ++i) {
            auto e = er.create();
            er.emplace<Position>(e, (float)i, 0.f);
            er.emplace<Velocity>(e, 1.f, 1.f);
        }
    });

    row("archtype", "create<P,V>",        a_multi,  N);
    row("archtype", "createEntity+add x2", a_legacy, N);
    row("entt",     "create+emplace x2",   e_create, N);
    key("create<Cs>     vs entt:", a_multi,  e_create);
    key("legacy add x2  vs entt:", a_legacy, e_create);
}

// ============================================================================
//  S4 — add component {P} -> {P,V}  (archetype migration; entt group upkeep)
// ============================================================================
static void s4_add(int N) {
    section("S4 add component {P} -> {P,V}", N);

    auto a = bench::measure_setup_work(
        [&] {
            Scene<ax::Registry, ax::Entity> sc{ std::make_unique<ax::Registry>(), {} };
            sc.reg->reserve(N); sc.es.reserve(N);
            for (int i = 0; i < N; ++i) sc.es.push_back(sc.reg->create<Position>(Position{(float)i,0}));
            return sc;
        },
        [&](auto& sc) { for (auto e : sc.es) sc.reg->emplace<Velocity>(e, 1.f, 1.f); });

    auto e_ng = bench::measure_setup_work(
        [&] {
            Scene<entt::registry, entt::entity> sc{ std::make_unique<entt::registry>(), {} };
            sc.es.reserve(N);
            for (int i = 0; i < N; ++i) { auto e = sc.reg->create(); sc.reg->emplace<Position>(e,(float)i,0.f); sc.es.push_back(e); }
            return sc;
        },
        [&](auto& sc) { for (auto e : sc.es) sc.reg->emplace<Velocity>(e, 1.f, 1.f); });

    auto e_g = bench::measure_setup_work(
        [&] {
            Scene<entt::registry, entt::entity> sc{ std::make_unique<entt::registry>(), {} };
            sc.es.reserve(N);
            for (int i = 0; i < N; ++i) { auto e = sc.reg->create(); sc.reg->emplace<Position>(e,(float)i,0.f); sc.es.push_back(e); }
            (void)sc.reg->group<Position, Velocity>();  // owning group present -> pays upkeep per add
            return sc;
        },
        [&](auto& sc) { for (auto e : sc.es) sc.reg->emplace<Velocity>(e, 1.f, 1.f); });

    row("archtype", "emplace<V> (migration)", a,    N);
    row("entt",     "emplace<V> (no group)",  e_ng, N);
    row("entt",     "emplace<V> (owning grp)", e_g, N);
    key("vs entt (no group)  :", a, e_ng);
    key("vs entt (owning grp):", a, e_g);
}

// ============================================================================
//  S5 — remove component {P,V} -> {P}
// ============================================================================
static void s5_remove(int N) {
    section("S5 remove component {P,V} -> {P}", N);

    auto a = bench::measure_setup_work(
        [&] {
            Scene<ax::Registry, ax::Entity> sc{ std::make_unique<ax::Registry>(), {} };
            sc.reg->reserve(N); sc.es.reserve(N);
            for (int i = 0; i < N; ++i) sc.es.push_back(sc.reg->create<Position, Velocity>(Position{(float)i,0}, Velocity{1,1}));
            return sc;
        },
        [&](auto& sc) { for (auto e : sc.es) sc.reg->erase<Velocity>(e); });

    auto e_ng = bench::measure_setup_work(
        [&] {
            Scene<entt::registry, entt::entity> sc{ std::make_unique<entt::registry>(), {} };
            sc.es.reserve(N);
            for (int i = 0; i < N; ++i) { auto e = sc.reg->create(); sc.reg->emplace<Position>(e,(float)i,0.f); sc.reg->emplace<Velocity>(e,1.f,1.f); sc.es.push_back(e); }
            return sc;
        },
        [&](auto& sc) { for (auto e : sc.es) sc.reg->remove<Velocity>(e); });

    auto e_g = bench::measure_setup_work(
        [&] {
            Scene<entt::registry, entt::entity> sc{ std::make_unique<entt::registry>(), {} };
            sc.es.reserve(N);
            for (int i = 0; i < N; ++i) { auto e = sc.reg->create(); sc.reg->emplace<Position>(e,(float)i,0.f); sc.reg->emplace<Velocity>(e,1.f,1.f); sc.es.push_back(e); }
            (void)sc.reg->group<Position, Velocity>();
            return sc;
        },
        [&](auto& sc) { for (auto e : sc.es) sc.reg->remove<Velocity>(e); });

    row("archtype", "erase<V> (migration)",  a,    N);
    row("entt",     "remove<V> (no group)",  e_ng, N);
    row("entt",     "remove<V> (owning grp)", e_g, N);
    key("vs entt (no group)  :", a, e_ng);
    key("vs entt (owning grp):", a, e_g);
}

// ============================================================================
//  S6 — random single-component get (shuffled) — archetype's weak spot
// ============================================================================
static void s6_random_get(int N) {
    section("S6 random single-component get (shuffled)", N);

    std::vector<int> idx(N);
    for (int i = 0; i < N; ++i) idx[i] = i;
    std::mt19937 rng(123);
    std::shuffle(idx.begin(), idx.end(), rng);

    ax::Registry ar; ar.reserve(N);
    std::vector<ax::Entity> aes; aes.reserve(N);
    for (int i = 0; i < N; ++i) aes.push_back(ar.create<Position, Velocity>(Position{(float)i,0}, Velocity{1,1}));
    auto a_def = bench::measure([&] {
        long long acc = 0;
        for (int i : idx) if (auto* p = ar.try_get<Position>(aes[i])) acc += (long long)p->x;
        bench::do_not_optimize(acc);
    });
    ax::ComponentIndex<Position> pidx(ar);
    auto a_idx = bench::measure([&] {
        long long acc = 0;
        for (int i : idx) if (auto* p = pidx.try_get(aes[i])) acc += (long long)p->x;
        bench::do_not_optimize(acc);
    });

    entt::registry er;
    er.storage<Position>().reserve(N); er.storage<Velocity>().reserve(N);
    std::vector<entt::entity> ees; ees.reserve(N);
    for (int i = 0; i < N; ++i) { auto e = er.create(); er.emplace<Position>(e,(float)i,0.f); er.emplace<Velocity>(e,1.f,1.f); ees.push_back(e); }
    auto e_get = bench::measure([&] {
        long long acc = 0;
        for (int i : idx) if (auto* p = er.try_get<Position>(ees[i])) acc += (long long)p->x;
        bench::do_not_optimize(acc);
    });

    row("archtype", "try_get (default)", a_def, N);
    row("archtype", "ComponentIndex",    a_idx, N);
    row("entt",     "try_get",           e_get, N);
    key("default vs entt:", a_def, e_get);
    key("indexed vs entt:", a_idx, e_get);
}

// ============================================================================
//  S7 — churn: steady-state mix of destroy+recreate / add / remove / get
// ============================================================================
static void s7_churn(int N) {
    const int OPS = 100'000;
    section("S7 churn (100k mixed ops)", N);

    auto a = bench::measure_setup_work(
        [&] {
            Scene<ax::Registry, ax::Entity> sc{ std::make_unique<ax::Registry>(), {} };
            sc.reg->reserve(N); sc.es.reserve(N);
            for (int i = 0; i < N; ++i) sc.es.push_back(sc.reg->create<Position, Velocity>(Position{(float)i,0}, Velocity{1,1}));
            return sc;
        },
        [&](auto& sc) {
            std::mt19937 rng(7);
            std::uniform_int_distribution<int> d(0, N - 1);
            auto& r = *sc.reg;
            for (int i = 0; i < OPS; ++i) {
                int k = d(rng);
                switch (i & 3) {
                    case 0: r.destroy(sc.es[k]); sc.es[k] = r.create<Position, Velocity>(Position{0,0}, Velocity{1,1}); break;
                    case 1: if (!r.has<Health>(sc.es[k])) r.emplace<Health>(sc.es[k], 1); break;
                    case 2: if (r.has<Health>(sc.es[k])) r.erase<Health>(sc.es[k]); break;
                    case 3: if (auto* p = r.try_get<Position>(sc.es[k])) bench::do_not_optimize(*p); break;
                }
            }
        });

    auto e = bench::measure_setup_work(
        [&] {
            Scene<entt::registry, entt::entity> sc{ std::make_unique<entt::registry>(), {} };
            sc.es.reserve(N);
            for (int i = 0; i < N; ++i) { auto en = sc.reg->create(); sc.reg->emplace<Position>(en,(float)i,0.f); sc.reg->emplace<Velocity>(en,1.f,1.f); sc.es.push_back(en); }
            return sc;
        },
        [&](auto& sc) {
            std::mt19937 rng(7);
            std::uniform_int_distribution<int> d(0, N - 1);
            auto& r = *sc.reg;
            for (int i = 0; i < OPS; ++i) {
                int k = d(rng);
                switch (i & 3) {
                    case 0: r.destroy(sc.es[k]); { auto en = r.create(); r.emplace<Position>(en,0.f,0.f); r.emplace<Velocity>(en,1.f,1.f); sc.es[k] = en; } break;
                    case 1: if (!r.all_of<Health>(sc.es[k])) r.emplace<Health>(sc.es[k], 1); break;
                    case 2: if (r.all_of<Health>(sc.es[k])) r.remove<Health>(sc.es[k]); break;
                    case 3: if (auto* p = r.try_get<Position>(sc.es[k])) bench::do_not_optimize(*p); break;
                }
            }
        });

    row("archtype", "100k mixed ops", a, OPS);
    row("entt",     "100k mixed ops", e, OPS);
    key("churn vs entt:", a, e);
}

// ============================================================================
int main() {
    std::printf("==================================================================\n");
    std::printf(" FAIR ECS benchmark:  lux::cxx::archtype   vs   EnTT\n");
    std::printf(" median of 5 runs, 1 warmup, do_not_optimize sink\n");
    std::printf(" NOTE: entt iteration uses an OWNING GROUP (its fastest path),\n");
    std::printf("       not just a plain view, so the comparison is honest.\n");
    std::printf("       Small N (1000) is engine-realistic scale: note that the\n");
    std::printf("       absolute times there are negligible regardless of winner.\n");
    std::printf("==================================================================\n");

    for (int N : { 1000, 100000, 1000000 }) {
        s1_iterate_homogeneous(N);
        s2_iterate_fragmented(N);
        s3_create(N);
        s4_add(N);
        s5_remove(N);
        s6_random_get(N);
        s7_churn(N);
    }
    return 0;
}
