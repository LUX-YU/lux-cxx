#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>

#include <lux/cxx/archtype/Registry.hpp>
#include <lux/cxx/archtype/CommandBuffer.hpp>
#include <lux/cxx/archtype/Group.hpp>
#include <lux/cxx/archtype/Observer.hpp>
#include <lux/cxx/archtype/Index.hpp>

// =====================
// Example Components
// =====================
struct Position {
    float x, y;
    Position() : x(0.0f), y(0.0f) {}
    Position(float px, float py) : x(px), y(py) {}
};

struct Velocity {
    float vx, vy;
    Velocity() : vx(0.0f), vy(0.0f) {}
    Velocity(float vx_, float vy_) : vx(vx_), vy(vy_) {}
};

struct Health {
    int hp;
    Health() : hp(100) {}
    Health(int h) : hp(h) {}
};

using lux::cxx::archtype::Registry;
using lux::cxx::archtype::Entity;

// =====================
// Functional Tests
// =====================

static void testCreateAndQuery()
{
    std::cout << "[Test] Create and Query Entities" << std::endl;

    Registry world;
    Entity e1 = world.createEntity();
    Entity e2 = world.createEntity();
    assert(e1 != e2);

    auto noPos = world.queryEntities<Position>();
    assert(noPos.empty());

    world.addComponent<Position>(e1, 10.0f, 20.0f);

    auto hasPos = world.queryEntities<Position>();
    assert(hasPos.size() == 1);
    assert(hasPos[0] == e1);

    std::cout << "[OK]  testCreateAndQuery passed.\n\n";
}

static void testAddMultipleComponents()
{
    std::cout << "[Test] Add Multiple Components" << std::endl;

    Registry world;
    Entity e = world.createEntity();

    auto& pos = world.addComponent<Position>(e, 1.0f, 2.0f);
    auto& vel = world.addComponent<Velocity>(e, 0.1f, 0.2f);
    auto& hp  = world.addComponent<Health>(e, 80);

    assert(std::fabs(pos.x - 1.0f) < 1e-6f && std::fabs(pos.y - 2.0f) < 1e-6f);
    assert(std::fabs(vel.vx - 0.1f) < 1e-6f && std::fabs(vel.vy - 0.2f) < 1e-6f);
    assert(hp.hp == 80);

    auto foundPos = world.queryEntities<Position>();
    assert(foundPos.size() == 1 && foundPos[0] == e);

    auto foundVel = world.queryEntities<Velocity>();
    assert(foundVel.size() == 1 && foundVel[0] == e);

    auto foundHP = world.queryEntities<Health>();
    assert(foundHP.size() == 1 && foundHP[0] == e);

    auto bothPosVel = world.queryEntities<Position, Velocity>();
    assert(bothPosVel.size() == 1 && bothPosVel[0] == e);

    auto bothHealthVel = world.queryEntities<Health, Velocity>();
    assert(bothHealthVel.size() == 1 && bothHealthVel[0] == e);

    auto triple = world.queryEntities<Position, Velocity, Health>();
    assert(triple.size() == 1 && triple[0] == e);

    std::cout << "[OK] testAddMultipleComponents passed.\n\n";
}

static void testRemoveComponent()
{
    std::cout << "[Test] Remove Component" << std::endl;

    Registry world;
    Entity e = world.createEntity();

    world.addComponent<Position>(e, 2.0f, 3.0f);
    world.addComponent<Velocity>(e, 1.0f, 0.0f);
    world.addComponent<Health>(e, 50);

    auto triple = world.queryEntities<Position, Velocity, Health>();
    assert(triple.size() == 1);

    world.removeComponent<Velocity>(e);

    auto posHealth = world.queryEntities<Position, Health>();
    assert(posHealth.size() == 1 && posHealth[0] == e);

    auto justVel = world.queryEntities<Velocity>();
    assert(justVel.empty());

    std::cout << "[OK] testRemoveComponent passed.\n\n";
}

static void testDestroyEntity()
{
    std::cout << "[Test] Destroy Entity" << std::endl;

    Registry world;
    Entity e1 = world.createEntity();
    Entity e2 = world.createEntity();

    world.addComponent<Position>(e1, 10.f, 10.f);
    world.addComponent<Velocity>(e2, 1.0f, 1.0f);

    auto posEntities = world.queryEntities<Position>();
    auto velEntities = world.queryEntities<Velocity>();
    assert(posEntities.size() == 1 && posEntities[0] == e1);
    assert(velEntities.size() == 1 && velEntities[0] == e2);

    world.destroyEntity(e1);
    assert(!world.valid(e1));

    posEntities = world.queryEntities<Position>();
    assert(posEntities.empty());

    velEntities = world.queryEntities<Velocity>();
    assert(velEntities.size() == 1 && velEntities[0] == e2);

    std::cout << "[OK] testDestroyEntity passed.\n\n";
}

static void testRandomCreateRemove()
{
    std::cout << "[Test] Random Create/Remove" << std::endl;
    Registry world;

    constexpr int N = 100;
    std::vector<Entity> entities;
    entities.reserve(N);

    for (int i = 0; i < N; i++) {
        Entity e = world.createEntity();
        entities.push_back(e);
        if (i % 2 == 0) {
            world.addComponent<Position>(e, (float)i, (float)(i + 1));
        } else {
            world.addComponent<Velocity>(e, (float)i * 0.1f, (float)i * 0.2f);
        }
    }

    assert(world.queryEntities<Position>().size() == 50);
    assert(world.queryEntities<Velocity>().size() == 50);

    for (int i = 0; i < N / 2; i++) {
        world.destroyEntity(entities[i]);
    }

    for (int i = N / 2; i < N; i++) {
        if (i % 2 == 0) {
            world.addComponent<Velocity>(entities[i], 0.01f, 0.02f);
        } else {
            world.addComponent<Position>(entities[i], (float)i, (float)i);
        }
    }

    auto posVelBoth = world.queryEntities<Position, Velocity>();
    std::cout << "  posVelBoth size = " << posVelBoth.size() << std::endl;

    std::cout << "[OK] testRandomCreateRemove passed.\n\n";
}

// ---- New API checks --------------------------------------------------------

static void testNewAPI()
{
    std::cout << "[Test] New API (create<Cs...>, view, each)" << std::endl;
    Registry world;

    // Multi-component create — entity lands directly in {P, V}
    Entity a = world.create<Position, Velocity>(Position{1.f, 2.f}, Velocity{3.f, 4.f});
    Entity b = world.create<Position>(Position{5.f, 6.f});

    assert(world.has<Position>(a) && world.has<Velocity>(a));
    assert(world.has<Position>(b) && !world.has<Velocity>(b));

    // each: lambda taking (Entity, Cs&...)
    int seen = 0;
    world.view<Position, Velocity>().each([&](Entity e, Position& p, Velocity& v) {
        assert(e == a);
        assert(p.x == 1.f && v.vx == 3.f);
        p.x += v.vx;
        ++seen;
    });
    assert(seen == 1);
    assert(world.get<Position>(a).x == 4.f);

    // each: lambda without Entity
    world.view<Position>().each([](Position& p) { p.y += 1.f; });
    assert(world.get<Position>(a).y == 3.f);
    assert(world.get<Position>(b).y == 7.f);

    // for_each_chunk SoA access
    std::size_t total = 0;
    world.view<Position>().for_each_chunk(
        [&](std::size_t n, Entity*, Position*) { total += n; });
    assert(total == 2);

    // size() — extra parens to keep MSVC's assert macro from splitting on
    // commas inside template-argument lists.
    assert(world.view<Position>().size() == 2);
    assert((world.view<Position, Velocity>().size() == 1));

    // exclude
    auto ev = world.view<Position>().exclude<Velocity>();
    seen = 0;
    ev.each([&](Entity e, Position&) { assert(e == b); ++seen; });
    assert(seen == 1);

    // valid() catches stale handle after destroy
    Entity c = world.create<Position>(Position{});
    Entity stale = c;
    world.destroy(c);
    assert(!world.valid(stale));

    std::cout << "[OK] testNewAPI passed.\n\n";
}

// ---- Signals ---------------------------------------------------------------

static void testSignals()
{
    std::cout << "[Test] Signals (on_construct / on_update / on_destroy)" << std::endl;
    Registry world;

    int ctor_p = 0, ctor_v = 0;
    int updt_p = 0;
    int dtor_p = 0, dtor_v = 0;

    world.on_construct<Position>().connect([&](Registry&, Entity) { ++ctor_p; });
    world.on_construct<Velocity>().connect([&](Registry&, Entity) { ++ctor_v; });
    world.on_update<Position>().connect([&](Registry&, Entity)    { ++updt_p; });
    world.on_destroy<Position>().connect([&](Registry&, Entity)   { ++dtor_p; });
    world.on_destroy<Velocity>().connect([&](Registry&, Entity)   { ++dtor_v; });

    // create<Cs...> fires on_construct for each Cs.
    Entity a = world.create<Position, Velocity>(Position{1,2}, Velocity{3,4});
    assert(ctor_p == 1 && ctor_v == 1);

    // emplace on a NEW component fires on_construct.
    Entity b = world.create();
    world.emplace<Position>(b, 5.f, 6.f);
    assert(ctor_p == 2);

    // emplace on an EXISTING component fires on_update.
    world.emplace<Position>(b, 7.f, 8.f);
    assert(updt_p == 1);
    assert(world.get<Position>(b).x == 7.f);

    // erase fires on_destroy.
    world.erase<Position>(b);
    assert(dtor_p == 1);

    // destroy fires on_destroy for each remaining component (none here).
    world.destroy(b);

    // destroy on an entity with components fires on_destroy for each.
    world.destroy(a);
    assert(dtor_p == 2 && dtor_v == 1);

    // disconnect_all silences further events.
    world.on_construct<Position>().disconnect_all();
    Entity c = world.create<Position>(Position{});
    assert(ctor_p == 2);  // unchanged
    (void)c;

    std::cout << "[OK] testSignals passed.\n\n";
}

// ---- CommandBuffer ---------------------------------------------------------

// ---- A-zone features -------------------------------------------------------

struct Player {};   // tag: empty class
struct Frozen {};   // tag

static void testEmplaceVariants()
{
    std::cout << "[Test] emplace_or_replace / get_or_emplace / patch" << std::endl;
    Registry world;
    Entity e = world.create();

    // emplace_or_replace: inserts the first time, overwrites the second.
    world.emplace_or_replace<Position>(e, 1.f, 2.f);
    assert(world.get<Position>(e).x == 1.f);
    world.emplace_or_replace<Position>(e, 9.f, 9.f);
    assert(world.get<Position>(e).x == 9.f);

    // get_or_emplace: returns existing, doesn't overwrite.
    auto& p = world.get_or_emplace<Position>(e, 42.f, 42.f);
    assert(p.x == 9.f); // unchanged
    Entity e2 = world.create();
    auto& p2 = world.get_or_emplace<Position>(e2, 7.f, 8.f);
    assert(p2.x == 7.f); // newly constructed

    // patch: mutate in place + fire on_update.
    int updates = 0;
    world.on_update<Position>().connect([&](Registry&, Entity) { ++updates; });
    world.patch<Position>(e, [](Position& q) { q.x += 100.f; });
    assert(world.get<Position>(e).x == 109.f);
    assert(updates == 1);

    std::cout << "[OK] testEmplaceVariants passed.\n\n";
}

static void testTagComponents()
{
    std::cout << "[Test] Tag / empty components" << std::endl;
    Registry world;

    Entity a = world.create<Position, Player>(Position{1.f, 2.f}, Player{});
    Entity b = world.create<Position>(Position{3.f, 4.f});

    assert(world.has<Player>(a));
    assert(!world.has<Player>(b));

    // view filter on tag
    int hits = 0;
    world.view<Position, Player>().each([&](Entity e, Position&, Player&) {
        assert(e == a);
        ++hits;
    });
    assert(hits == 1);

    // exclude on tag
    hits = 0;
    world.view<Position>().exclude<Player>().each([&](Entity e, Position&) {
        assert(e == b);
        ++hits;
    });
    assert(hits == 1);

    // emplace tag on b → migrates to {Position, Player}
    world.emplace<Player>(b);
    assert((world.view<Position, Player>().size() == 2));

    // erase tag
    world.erase<Player>(a);
    assert(!world.has<Player>(a));

    std::cout << "[OK] testTagComponents passed.\n\n";
}

static void testMultiErase()
{
    std::cout << "[Test] Multi-component atomic erase" << std::endl;
    Registry world;

    Entity e = world.create<Position, Velocity, Health, Player>(
        Position{1,2}, Velocity{3,4}, Health{50}, Player{});

    int dtor_p = 0, dtor_v = 0, dtor_h = 0, dtor_tag = 0;
    world.on_destroy<Position>().connect([&](Registry&, Entity) { ++dtor_p; });
    world.on_destroy<Velocity>().connect([&](Registry&, Entity) { ++dtor_v; });
    world.on_destroy<Health>().connect([&](Registry&, Entity)   { ++dtor_h; });
    world.on_destroy<Player>().connect([&](Registry&, Entity)   { ++dtor_tag; });

    // Remove Velocity + Player in one migration.
    world.erase<Velocity, Player>(e);

    assert(dtor_v == 1 && dtor_tag == 1);
    assert(dtor_p == 0 && dtor_h == 0);

    assert(world.has<Position>(e));
    assert(world.has<Health>(e));
    assert(!world.has<Velocity>(e));
    assert(!world.has<Player>(e));

    // Multi-erase that targets components the entity doesn't have: should no-op for those.
    world.erase<Velocity, Frozen>(e); // neither present
    assert(world.has<Position>(e) && world.has<Health>(e));

    // Erasing every remaining component leaves the entity in "no archetype".
    world.erase<Position, Health>(e);
    assert(world.valid(e));
    assert(!world.has<Position>(e) && !world.has<Health>(e));
    assert(dtor_p == 1 && dtor_h == 1);

    std::cout << "[OK] testMultiErase passed.\n\n";
}

static void testContext()
{
    std::cout << "[Test] ctx() singletons" << std::endl;
    struct Time   { float dt; };
    struct Random { unsigned seed; };

    Registry world;
    assert(!world.ctx().contains<Time>());

    auto& t = world.ctx().emplace<Time>(1.f / 60.f);
    assert(t.dt > 0.f);
    assert(world.ctx().contains<Time>());
    assert(world.ctx().get<Time>().dt == t.dt);

    world.ctx().emplace<Random>(42u);
    assert(world.ctx().size() == 2);

    // Overwrite the existing Time.
    world.ctx().emplace<Time>(1.f / 120.f);
    assert(world.ctx().get<Time>().dt == 1.f / 120.f);

    world.ctx().erase<Random>();
    assert(!world.ctx().contains<Random>());
    assert(world.ctx().find<Random>() == nullptr);

    std::cout << "[OK] testContext passed.\n\n";
}

static void testConstView()
{
    std::cout << "[Test] view<const T, U> const-correctness" << std::endl;
    Registry world;
    Entity e = world.create<Position, Velocity>(Position{1.f, 2.f}, Velocity{3.f, 4.f});
    (void)e;

    int seen = 0;
    world.view<const Position, Velocity>().each(
        [&](const Position& p, Velocity& v) {
            // p is read-only here — won't compile if we tried `p.x = ...`
            v.vx += p.x;
            ++seen;
        });
    assert(seen == 1);
    assert(world.get<Velocity>(e).vx == 4.f);

    // for_each_chunk with const pointer
    seen = 0;
    world.view<const Position, Velocity>().for_each_chunk(
        [&](std::size_t n, Entity*, const Position* ps, Velocity* vs) {
            for (std::size_t i = 0; i < n; ++i) {
                vs[i].vy += ps[i].y;
                ++seen;
            }
        });
    assert(seen == 1);
    assert(world.get<Velocity>(e).vy == 6.f);

    std::cout << "[OK] testConstView passed.\n\n";
}

static void testCommandBuffer()
{
    std::cout << "[Test] CommandBuffer" << std::endl;
    Registry world;
    lux::cxx::archtype::CommandBuffer cb;

    // Deferred create + emplace.
    auto bullet = cb.create<Position, Velocity>(Position{1,2}, Velocity{3,4});
    cb.emplace<Health>(bullet, 50);

    // Real-entity ops.
    Entity already = world.create();
    cb.emplace<Position>(already, 9.f, 9.f);

    assert(cb.size() == 3);
    cb.commit(world);
    assert(cb.empty());

    assert((world.view<Position, Velocity, Health>().size() == 1));
    assert(world.has<Position>(already));
    assert(world.get<Position>(already).x == 9.f);

    // Mutating during view iteration via CommandBuffer.
    for (int i = 0; i < 3; ++i) world.create<Position>(Position{(float)i, 0.f});
    int reaped = 0;
    world.view<Position>().each([&](Entity e, Position&) {
        cb.destroy(e);
        ++reaped;
    });
    assert(reaped == 4); // 1 bullet (has Position) + 3 we just made
    cb.commit(world);
    assert(world.view<Position>().size() == 1); // just `already`

    // clear() discards without applying.
    cb.destroy(already);
    assert(!cb.empty());
    cb.clear();
    assert(cb.empty());
    assert(world.valid(already));

    std::cout << "[OK] testCommandBuffer passed.\n\n";
}

// ---- D-zone features -------------------------------------------------------

static void testGroup()
{
    std::cout << "[Test] Group<> persistent cache + auto-refresh" << std::endl;
    Registry world;

    auto g = world.group<Position, Velocity>();
    assert(g.size() == 0);

    // Create entities AFTER the group exists — the group must auto-refresh
    // when a new archetype shows up.
    for (int i = 0; i < 10; ++i) {
        world.create<Position, Velocity>(Position{(float)i, 0.f}, Velocity{1.f, 1.f});
    }
    assert(g.size() == 10);

    int seen = 0;
    g.each([&](Position& p, Velocity& v) { p.x += v.vx; ++seen; });
    assert(seen == 10);

    // Entities in a strict-superset archetype should ALSO match the group.
    world.create<Position, Velocity, Health>(Position{}, Velocity{}, Health{1});
    assert(g.size() == 11);

    // for_each_chunk
    std::size_t total = 0;
    g.for_each_chunk([&](std::size_t n, Entity*, Position*, Velocity*) { total += n; });
    assert(total == 11);

    // exclude
    auto g2 = world.group<Position, Velocity>().exclude<Health>();
    assert(g2.size() == 10);

    std::cout << "[OK] testGroup passed.\n\n";
}

static void testObserver()
{
    std::cout << "[Test] Observer" << std::endl;
    Registry world;
    lux::cxx::archtype::Observer obs;
    obs.observe_construct<Velocity>(world)
       .observe_destroy<Health>(world);

    Entity a = world.create<Position>(Position{});      // no event
    assert(obs.empty());

    world.emplace<Velocity>(a, 1.f, 1.f);                // fires on_construct<Velocity>
    Entity b = world.create<Velocity, Health>(Velocity{}, Health{1}); // on_construct<Velocity>
    assert(obs.size() == 2);

    world.erase<Health>(b);                              // on_destroy<Health>
    assert(obs.size() == 2);                             // b already counted (dedupe)

    obs.clear();
    assert(obs.empty());

    // After clear, new events still tracked.
    Entity c = world.create<Velocity>(Velocity{});
    assert(obs.size() == 1);
    obs.each([&](Entity e) { assert(e == c); });

    // disconnect_all stops further events.
    obs.disconnect_all();
    obs.clear();
    world.create<Velocity>(Velocity{});
    assert(obs.empty());

    std::cout << "[OK] testObserver passed.\n\n";
}

static void testGroupSort()
{
    std::cout << "[Test] Group::sort_by" << std::endl;
    Registry world;

    // Build entities with shuffled Position.x values.
    const float xs[] = { 3.f, 1.f, 4.f, 1.f, 5.f, 9.f, 2.f, 6.f };
    constexpr int N = sizeof(xs) / sizeof(xs[0]);
    std::vector<Entity> ents;
    for (int i = 0; i < N; ++i) {
        ents.push_back(world.create<Position, Velocity>(
            Position{xs[i], 0.f}, Velocity{(float)i, 0.f}));
    }

    auto g = world.group<Position, Velocity>();
    g.sort_by<Position>([](const Position& a, const Position& b) { return a.x < b.x; });

    // After sort, iteration should see ascending Position.x.
    std::vector<float> seen;
    g.each([&](Position& p, Velocity&) { seen.push_back(p.x); });
    std::vector<float> expected(std::begin(xs), std::end(xs));
    std::sort(expected.begin(), expected.end());
    assert(seen == expected);

    // try_get must still resolve correctly post-sort (records were updated).
    for (Entity e : ents) {
        Position* p = world.try_get<Position>(e);
        Velocity* v = world.try_get<Velocity>(e);
        assert(p && v);
        // Velocity.vx was originally i, Position.x was originally xs[i].
        // After sort they should still be paired correctly.
        const int orig_i = static_cast<int>(v->vx);
        assert(p->x == xs[orig_i]);
    }

    std::cout << "[OK] testGroupSort passed.\n\n";
}

// ---- Late-phase optimizations: compact / big sig / view cache / cb batch / index

static void testCompact()
{
    std::cout << "[Test] Registry::compact()" << std::endl;
    Registry world;

    // Create a bunch of entities, then destroy half — leaves empty chunks.
    constexpr int N = 5000;
    std::vector<Entity> ents;
    ents.reserve(N);
    for (int i = 0; i < N; ++i) ents.push_back(world.create<Position>(Position{(float)i, 0.f}));
    for (int i = 0; i < N; ++i) world.destroy(ents[i]);

    const std::size_t freed = world.compact();
    assert(freed > 0); // at least one chunk's worth of buffer reclaimed

    // After compact, reuse should still work — chunks reacquire transparently.
    Entity e = world.create<Position>(Position{42.f, 0.f});
    assert(world.get<Position>(e).x == 42.f);

    std::cout << "[OK] testCompact passed (freed " << freed << " bytes).\n\n";
}

static void testBigSignature()
{
    std::cout << "[Test] Signature multi-block math (smoke)" << std::endl;
    lux::cxx::archtype::Signature s;
    // Test bits across word boundary (only meaningful if kMaxComponents > 64,
    // but the math should be correct for any size).
    s.set(0);
    s.set(63);
    assert(s.test(0) && s.test(63));
    assert(s.count() == 2);
    s.reset(0);
    assert(!s.test(0) && s.test(63) && s.count() == 1);
    std::cout << "[OK] testBigSignature passed.\n\n";
}

static void testViewCacheReuse()
{
    std::cout << "[Test] View reuses cached archetype list" << std::endl;
    Registry world;
    for (int i = 0; i < 100; ++i) {
        world.create<Position, Velocity>(Position{(float)i, 0.f}, Velocity{1.f, 1.f});
    }
    // Repeat view<P,V>() calls; should hit the registry's view cache.
    for (int i = 0; i < 5; ++i) {
        assert(world.view<Position>().size() == 100);
        assert((world.view<Position, Velocity>().size() == 100));
    }
    // Add a new archetype — cache should refresh on next call.
    world.create<Position, Health>(Position{}, Health{1});
    assert(world.view<Position>().size() == 101);
    assert((world.view<Position, Velocity>().size() == 100));
    std::cout << "[OK] testViewCacheReuse passed.\n\n";
}

static void testCommandBufferBatched()
{
    std::cout << "[Test] CommandBuffer batches multiple emplaces on a deferred entity"
              << std::endl;
    Registry world;
    int ctor_p = 0, ctor_v = 0, ctor_h = 0;
    world.on_construct<Position>().connect([&](Registry&, Entity) { ++ctor_p; });
    world.on_construct<Velocity>().connect([&](Registry&, Entity) { ++ctor_v; });
    world.on_construct<Health>().connect([&](Registry&, Entity) { ++ctor_h; });

    lux::cxx::archtype::CommandBuffer cb;
    auto de = cb.create();
    cb.emplace<Position>(de, 1.f, 2.f);
    cb.emplace<Velocity>(de, 3.f, 4.f);
    cb.emplace<Health>(de, 99);

    cb.commit(world);

    // Should produce exactly ONE entity in archetype {P,V,H} with all three.
    assert((world.view<Position, Velocity, Health>().size() == 1));
    assert(ctor_p == 1 && ctor_v == 1 && ctor_h == 1);

    std::cout << "[OK] testCommandBufferBatched passed.\n\n";
}

static void testComponentIndex()
{
    std::cout << "[Test] ComponentIndex<C> opt-in fast try_get" << std::endl;
    Registry world;
    std::vector<Entity> ents;
    for (int i = 0; i < 200; ++i) {
        ents.push_back(world.create<Position>(Position{(float)i, 0.f}));
    }

    lux::cxx::archtype::ComponentIndex<Position> idx(world);

    // Initial state from snapshot.
    for (int i = 0; i < 200; ++i) {
        Position* p = idx.try_get(ents[i]);
        assert(p && p->x == (float)i);
    }

    // Adding a NEW component to half of them: triggers migration → on_migrate
    // hook should refresh the cached Position pointer.
    for (int i = 0; i < 100; ++i) {
        world.emplace<Velocity>(ents[i], 1.f, 1.f);
    }
    for (int i = 0; i < 200; ++i) {
        Position* p = idx.try_get(ents[i]);
        assert(p && p->x == (float)i);  // cache stayed in sync across migration
    }

    // Destroy: removes from index.
    world.destroy(ents[0]);
    assert(idx.try_get(ents[0]) == nullptr);

    // Erase the component: also removes from index.
    world.erase<Position>(ents[150]);
    assert(idx.try_get(ents[150]) == nullptr);

    // New entity created with Position: shows up in index.
    Entity e = world.create<Position>(Position{999.f, 0.f});
    Position* p = idx.try_get(e);
    assert(p && p->x == 999.f);

    std::cout << "[OK] testComponentIndex passed.\n\n";
}

static void runAllFunctionTests()
{
    testCreateAndQuery();
    testAddMultipleComponents();
    testRemoveComponent();
    testDestroyEntity();
    testRandomCreateRemove();
    testNewAPI();
    testSignals();
    testCommandBuffer();
    testEmplaceVariants();
    testTagComponents();
    testMultiErase();
    testContext();
    testConstView();
    testGroup();
    testObserver();
    testGroupSort();
    testCompact();
    testBigSignature();
    testViewCacheReuse();
    testCommandBufferBatched();
    testComponentIndex();

    std::cout << "All functional tests passed.\n";
}

// NOTE: All performance measurement now lives in the dedicated, fair
// side-by-side benchmark `ecs_bench.cpp` (archtype vs entt view AND entt
// owning group, with honest coverage of the operations archetype is slow at).
// This file is the functional correctness suite only.

int main()
{
    lux::cxx::archtype::TypeRegistry::getTypeID<Position>();
    lux::cxx::archtype::TypeRegistry::getTypeID<Velocity>();
    lux::cxx::archtype::TypeRegistry::getTypeID<Health>();

    runAllFunctionTests();
    return 0;
}
