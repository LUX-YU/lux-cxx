// ============================================================================
// ecs_fixes_test.cpp
// ----------------------------------------------------------------------------
// Regression tests for the ECS hardening fixes:
//   #6  Signal: disconnect during publish must not corrupt/overrun the loop.
//   #7  Registry::emplace: a throwing component constructor must leave the
//       entity consistent (no destroyed-but-live slot, no half-migration).
//   #4  ComponentIndex: swap-on-remove relocation must refresh the moved
//       entity's cached pointer (no stale/dangling C*).
//   #5  Observer: destroying the Registry before the Observer must not cause a
//       use-after-free in ~Observer (dangling Signal*).
// Kept separate from archtype_test.cpp so it does not depend on unrelated tests.
// ============================================================================
#include <lux/cxx/archtype/Registry.hpp>
#include <lux/cxx/archtype/Index.hpp>
#include <lux/cxx/archtype/Observer.hpp>

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace lux::cxx::archtype;

namespace
{
    struct Pos   { int x = 0; };
    struct Other { int v = 0; Other(int vv = 0) : v(vv) {} };

    struct Throwing
    {
        int v = 0;
        static inline bool armed = false;   // when true, the value ctor throws
        static inline int  live  = 0;       // construct/destruct balance
        Throwing(int vv = 0) : v(vv) { if (armed) throw std::runtime_error("boom"); ++live; }
        Throwing(const Throwing& o) : v(o.v) { ++live; }
        Throwing(Throwing&& o) noexcept : v(o.v) { ++live; }
        Throwing& operator=(const Throwing&) = default;
        Throwing& operator=(Throwing&&) noexcept = default;
        ~Throwing() { --live; }
    };

    int g_failures = 0;
    void check(bool c, const char* d)
    {
        if (c) std::cout << "[PASS] " << d << "\n";
        else { std::cerr << "[FAIL] " << d << "\n"; ++g_failures; }
    }
}

// #6 — disconnect during publish (would overrun the index loop pre-fix).
static void test_signal_disconnect_during_publish()
{
    Registry w;
    auto& sig = w.on_construct<Pos>();

    int calls[3] = { 0, 0, 0 };
    Signal::ConnectionId ids[3] = {};
    ids[0] = sig.connect([&](Registry&, Entity) { ++calls[0]; });
    ids[1] = sig.connect([&](Registry&, Entity) { ++calls[1]; sig.disconnect(ids[2]); });
    ids[2] = sig.connect([&](Registry&, Entity) { ++calls[2]; });

    w.create<Pos>(Pos{ 1 });   // publish #1: B disconnects C mid-dispatch
    check(calls[0] == 1 && calls[1] == 1, "publish dispatched live listeners");
    check(calls[2] == 0, "listener disconnected mid-publish is not invoked");

    w.create<Pos>(Pos{ 2 });   // publish #2: C is gone, A and B remain
    check(calls[0] == 2 && calls[1] == 2 && calls[2] == 0,
          "post-publish compaction kept the right listeners");
}

// #7 — throwing ctor leaves the entity consistent.
static void test_emplace_exception_safety()
{
    // (a) overwrite path: existing component must survive a throwing replacement.
    {
        Registry w;
        Throwing::armed = false; Throwing::live = 0;
        Entity e = w.create();
        w.emplace<Throwing>(e, 10);
        check(w.get<Throwing>(e).v == 10, "overwrite: initial value");

        Throwing::armed = true;
        bool threw = false;
        try { w.emplace<Throwing>(e, 20); } catch (const std::exception&) { threw = true; }
        Throwing::armed = false;
        check(threw, "overwrite: throwing ctor propagated");
        check(w.get<Throwing>(e).v == 10, "overwrite: original component intact after throw");

        w.destroy(e);
        check(Throwing::live == 0, "overwrite: no leak / double-destroy (ctor/dtor balanced)");
    }

    // (b) migration path: adding a new component whose ctor throws must not
    //     half-migrate the entity.
    {
        Registry w;
        Throwing::armed = false; Throwing::live = 0;
        Entity e = w.create();
        w.emplace<Other>(e, 7);

        Throwing::armed = true;
        bool threw = false;
        try { w.emplace<Throwing>(e, 5); } catch (const std::exception&) { threw = true; }
        Throwing::armed = false;
        check(threw, "migration: throwing ctor propagated");
        check(w.has<Other>(e) && w.get<Other>(e).v == 7, "migration: entity keeps its other component");
        check(!w.has<Throwing>(e), "migration: failed component was not added");

        w.destroy(e);
        check(Throwing::live == 0, "migration: no leak / double-destroy");
    }
}

// #4 — ComponentIndex refreshes the entity relocated by swap-on-remove.
static void test_component_index_relocation()
{
    Registry w;
    std::vector<Entity> es;
    for (int i = 0; i < 5; ++i) es.push_back(w.create<Pos>(Pos{ i }));   // all in one chunk

    ComponentIndex<Pos> idx(w);
    for (int i = 0; i < 5; ++i)
        check(idx.try_get(es[i]) && idx.try_get(es[i])->x == i, "index initial lookup");

    // Destroying a non-last entity swaps the last entity (es[4]) into its row.
    w.destroy(es[0]);

    check(idx.try_get(es[0]) == nullptr, "destroyed entity removed from index");

    Pos* via_index = idx.try_get(es[4]);
    Pos* via_reg   = w.try_get<Pos>(es[4]);   // authoritative slow path
    check(via_index == via_reg, "relocated entity's cached pointer was refreshed (not stale)");
    check(via_index && via_index->x == 4, "relocated entity's value is intact via the index");
}

// #5 — Registry destroyed before Observer must not UAF in ~Observer.
static void test_observer_teardown_order()
{
    auto* reg = new Registry();
    auto* obs = new Observer();
    obs->observe_construct<Pos>(*reg);
    obs->observe_destroy<Pos>(*reg);
    reg->create<Pos>(Pos{ 1 });          // give the observer some state

    delete reg;   // Registry dies first -> Signals (and signals_ array) are gone
    delete obs;   // ~Observer must skip the now-dead Signals (no use-after-free)

    check(true, "observer destroyed after its registry without UAF");
}

int main()
{
    std::cout << "ECS hardening regression tests:\n";
    test_signal_disconnect_during_publish();
    test_emplace_exception_safety();
    test_component_index_relocation();
    test_observer_teardown_order();
    std::cout << "\n=== Results ===\nFailures: " << g_failures << "\n";
    return g_failures;
}
