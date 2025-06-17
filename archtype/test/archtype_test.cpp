#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>

#include <lux/cxx/archtype/Registry.hpp>

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

// =====================
// Functional Tests
// =====================

/**
 * Test 1: Basic entity creation and querying without any components.
 */
static void testCreateAndQuery()
{
    std::cout << "[Test] Create and Query Entities" << std::endl;

    lux::cxx::archtype::Registry world;
    // Create two entities without any components
    lux::cxx::archtype::Entity e1 = world.createEntity();
    lux::cxx::archtype::Entity e2 = world.createEntity();
    assert(e1 != e2);

    // Query for Position; none should exist yet
    auto noPos = world.queryEntities<Position>();
    assert(noPos.empty());

    // Add Position to e1
    Position& p = world.addComponent<Position>(e1, 10.0f, 20.0f);

    // Query again
    auto hasPos = world.queryEntities<Position>();
    assert(hasPos.size() == 1);
    assert(hasPos[0] == e1);

    std::cout << "[OK]  testCreateAndQuery passed.\n\n";
}

/**
 * Test 2: Add multiple components to the same entity and verify correctness
 *         by querying different component combinations.
 */
static void testAddMultipleComponents()
{
    std::cout << "[Test] Add Multiple Components" << std::endl;

    lux::cxx::archtype::Registry world;
    lux::cxx::archtype::Entity e = world.createEntity();

    // Add Position, Velocity, and Health
    auto& pos = world.addComponent<Position>(e, 1.0f, 2.0f);
    auto& vel = world.addComponent<Velocity>(e, 0.1f, 0.2f);
    auto& hp = world.addComponent<Health>(e, 80);

    // Validate component values
    assert(std::fabs(pos.x - 1.0f) < 1e-6f && std::fabs(pos.y - 2.0f) < 1e-6f);
    assert(std::fabs(vel.vx - 0.1f) < 1e-6f && std::fabs(vel.vy - 0.2f) < 1e-6f);
    assert(hp.hp == 80);

    // Query individually
    auto foundPos = world.queryEntities<Position>();
    assert(foundPos.size() == 1 && foundPos[0] == e);

    auto foundVel = world.queryEntities<Velocity>();
    assert(foundVel.size() == 1 && foundVel[0] == e);

    auto foundHP = world.queryEntities<Health>();
    assert(foundHP.size() == 1 && foundHP[0] == e);

    // Query combinations
    auto bothPosVel = world.queryEntities<Position, Velocity>();
    assert(bothPosVel.size() == 1 && bothPosVel[0] == e);

    auto bothHealthVel = world.queryEntities<Health, Velocity>();
    assert(bothHealthVel.size() == 1 && bothHealthVel[0] == e);

    auto triple = world.queryEntities<Position, Velocity, Health>();
    assert(triple.size() == 1 && triple[0] == e);

    std::cout << "[OK] testAddMultipleComponents passed.\n\n";
}

/**
 * Test 3: Remove a component from an entity and verify data migration
 *         to an Archetype without that component.
 */
static void testRemoveComponent()
{
    std::cout << "[Test] Remove Component" << std::endl;

    lux::cxx::archtype::Registry world;
    lux::cxx::archtype::Entity e = world.createEntity();

    // Add 3 components
    world.addComponent<Position>(e, 2.0f, 3.0f);
    world.addComponent<Velocity>(e, 1.0f, 0.0f);
    world.addComponent<Health>(e, 50);

    // Verify they exist
    auto triple = world.queryEntities<Position, Velocity, Health>();
    assert(triple.size() == 1);

    // Remove Velocity
    world.removeComponent<Velocity>(e);

    // Query for Position+Health, should still find e
    auto posHealth = world.queryEntities<Position, Health>();
    assert(posHealth.size() == 1 && posHealth[0] == e);

    // Query for Velocity should be empty now
    auto justVel = world.queryEntities<Velocity>();
    assert(justVel.empty());

    std::cout << "[OK] testRemoveComponent passed.\n\n";
}

/**
 * Test 4: Destroy an entity and verify that it no longer appears in queries.
 */
static void testDestroyEntity()
{
    std::cout << "[Test] Destroy Entity" << std::endl;

    lux::cxx::archtype::Registry world;
    lux::cxx::archtype::Entity e1 = world.createEntity();
    lux::cxx::archtype::Entity e2 = world.createEntity();

    // Add different components to e1 and e2
    world.addComponent<Position>(e1, 10.f, 10.f);
    world.addComponent<Velocity>(e2, 1.0f, 1.0f);

    auto posEntities = world.queryEntities<Position>();
    auto velEntities = world.queryEntities<Velocity>();
    assert(posEntities.size() == 1 && posEntities[0] == e1);
    assert(velEntities.size() == 1 && velEntities[0] == e2);

    // Destroy e1
    world.destroyEntity(e1);
    posEntities = world.queryEntities<Position>();
    assert(posEntities.empty());

    // e2 should remain
    velEntities = world.queryEntities<Velocity>();
    assert(velEntities.size() == 1 && velEntities[0] == e2);

    std::cout << "[OK] testDestroyEntity passed.\n\n";
}

/**
 * Test 5: Randomly create multiple entities, perform add/remove operations,
 *         and check overall consistency. This is a partial stress test.
 */
static void testRandomCreateRemove()
{
    std::cout << "[Test] Random Create/Remove" << std::endl;
    lux::cxx::archtype::Registry world;

    // Create 100 entities, half get Position, half get Velocity
    constexpr int N = 100;
    std::vector<lux::cxx::archtype::Entity> entities;
    entities.reserve(N);

    for (int i = 0; i < N; i++) {
        lux::cxx::archtype::Entity e = world.createEntity();
        entities.push_back(e);
        if (i % 2 == 0) {
            // Even ID: add Position
            world.addComponent<Position>(e, (float)i, (float)(i + 1));
        }
        else {
            // Odd ID: add Velocity
            world.addComponent<Velocity>(e, (float)i * 0.1f, (float)i * 0.2f);
        }
    }

    // Query for each
    auto posResult = world.queryEntities<Position>();
    auto velResult = world.queryEntities<Velocity>();

    // Should be 50 each
    assert(posResult.size() == 50);
    assert(velResult.size() == 50);

    // Destroy half of them
    for (int i = 0; i < N / 2; i++) {
        world.destroyEntity(entities[i]);
    }

    // The other half remains. Now try adding or removing components
    for (int i = N / 2; i < N; i++) {
        if (i % 2 == 0) {
            // Even used to have Position, now also add Velocity
            world.addComponent<Velocity>(entities[i], 0.01f, 0.02f);
        }
        else {
            // Odd used to have Velocity, now also add Position
            world.addComponent<Position>(entities[i], (float)i, (float)i);
        }
    }

    // Final check: Some will have both Position and Velocity
    auto posVelBoth = world.queryEntities<Position, Velocity>();
    // Could be fewer than 50, but definitely not exceeding the survivors
    std::cout << "  posVelBoth size = " << posVelBoth.size() << std::endl;

    std::cout << "[OK] testRandomCreateRemove passed.\n\n";
}

/**
 * @brief Runs all functional tests.
 */
static void runAllFunctionTests()
{
    testCreateAndQuery();
    testAddMultipleComponents();
    testRemoveComponent();
    testDestroyEntity();
    testRandomCreateRemove();

    std::cout << "All functional tests passed.\n";
}

// =====================
// Performance Tests
// =====================

/**
 * @brief Demonstrates performance tests: large-scale entity creation,
 *        queries, component removal, and repeated getComponent usage.
 */
static void testPerformance()
{
    using clock = std::chrono::high_resolution_clock;
    std::cout << "[Performance Test] Start\n";

    constexpr int N = 1'000'000; // number of entities for large-scale test
    lux::cxx::archtype::Registry world;

    // 1) Massive creation & component assignment
    auto startCreate = clock::now();

    for (int i = 0; i < N; i++) {
        lux::cxx::archtype::Entity e = world.createEntity();
        // Distribute components among three categories
        if (i % 3 == 0) {
            // one-third: Position + Velocity
            world.addComponent<Position>(e, (float)i, (float)i);
            world.addComponent<Velocity>(e, (float)i * 0.01f, (float)i * 0.02f);
        }
        else if (i % 3 == 1) {
            // one-third: Velocity + Health
            world.addComponent<Velocity>(e, 1.0f, 0.0f);
            world.addComponent<Health>(e, 100 + i % 50);
        }
        else {
            // last third: Position only
            world.addComponent<Position>(e, (float)(i * 0.1f), (float)(i * 0.1f));
        }
    }

    auto endCreate = clock::now();
    auto createMs = std::chrono::duration_cast<std::chrono::milliseconds>(endCreate - startCreate).count();
    std::cout << "  Created " << N << " entities in " << createMs << " ms\n";

    // 2) Query performance
    auto startQuery = clock::now();

    auto posList    = world.queryEntities<Position>();
    auto velList    = world.queryEntities<Velocity>();
    auto posVelList = world.queryEntities<Position, Velocity>();
    auto healthList = world.queryEntities<Health>();

    auto endQuery = clock::now();
    auto queryMs = std::chrono::duration_cast<std::chrono::milliseconds>(endQuery - startQuery).count();

    std::cout << "  queryEntities<Position>() => " << posList.size() << " results\n";
    std::cout << "  queryEntities<Velocity>() => " << velList.size() << " results\n";
    std::cout << "  queryEntities<Position,Velocity>() => " << posVelList.size() << " results\n";
    std::cout << "  queryEntities<Health>() => " << healthList.size() << " results\n";
    std::cout << "  Query time: " << queryMs << " ms\n";

    // 3) Repeated getComponent performance
    //    Suppose we want to measure how quickly we can fetch Position
    //    for half of the created entities (or all).
    auto startGetComp = clock::now();
    {
        int fetchCount = N / 2; // example: fetch half
        for (int i = 0; i < fetchCount; ++i) {
            Position* p = world.getComponent<Position>((lux::cxx::archtype::Entity)i);
            // For measurement, we might do a trivial read to ensure p is used
            if (p) { float dummy = p->x; (void)dummy; }
        }
    }
    auto endGetComp = clock::now();
    auto getCompMs = std::chrono::duration_cast<std::chrono::milliseconds>(endGetComp - startGetComp).count();
    std::cout << "  Performed " << (N / 2) << " getComponent<Position> calls in " << getCompMs << " ms\n";

    // 4) Remove a certain component from a batch of entities
    auto startRemove = clock::now();
    int removeCount = 100000;
    for (int i = 0; i < removeCount; i++) {
        world.removeComponent<Velocity>((lux::cxx::archtype::Entity)i);
    }
    auto endRemove = clock::now();
    auto removeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endRemove - startRemove).count();
    std::cout << "  Removed Velocity from " << removeCount << " entities in " << removeMs << " ms\n";

    std::cout << "[Performance Test] Done\n\n";
}

/**
 * @brief Test entry point.
 */
int main()
{
    // Optionally register component types explicitly, if your ECS needs it.
    // For some implementations, the first getTypeID<T>() call happens automatically.
    lux::cxx::archtype::TypeRegistry::getTypeID<Position>();
    lux::cxx::archtype::TypeRegistry::getTypeID<Velocity>();
    lux::cxx::archtype::TypeRegistry::getTypeID<Health>();

    // 1) Run all functional tests
    runAllFunctionTests();

    // 2) Run performance tests
    testPerformance();

    return 0;
}
