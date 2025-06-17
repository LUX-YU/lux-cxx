// entt_tests.cpp
// ============================================================
// Functional- & Performance-tests for EnTT, mirroring the tests
// in lux::cxx::archtype::Registry example.
// ============================================================

#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cmath>
#include <chrono>

#include <entt/entt.hpp>

// =====================
// Example Components
// =====================
struct Position {
    float x{}, y{};
    Position() = default;
    Position(float px, float py) : x(px), y(py) {}
};

struct Velocity {
    float vx{}, vy{};
    Velocity() = default;
    Velocity(float vx_, float vy_) : vx(vx_), vy(vy_) {}
};

struct Health {
    int hp{ 100 };
    Health() = default;
    explicit Health(int h) : hp(h) {}
};

// ------------------------------------------------------------
// Helper – copy a view’s entities into a std::vector
// ------------------------------------------------------------
template<class View>
static std::vector<entt::entity> toVector(const View& view)
{
    std::vector<entt::entity> out;
    // out.reserve(view.size());
    for (auto ent : view) { out.push_back(ent); }
    return out;
}

// =====================
// Functional Tests
// =====================

/**
 * Test 1: Basic entity creation and querying without any components.
 */
static void testCreateAndQuery()
{
    std::cout << "[Test] Create and Query Entities\n";

    entt::registry reg;
    entt::entity e1 = reg.create();
    entt::entity e2 = reg.create();
    assert(e1 != e2);

    // No Position yet
    auto posNone = toVector(reg.view<Position>());
    assert(posNone.empty());

    // Add Position to e1
    reg.emplace<Position>(e1, 10.f, 20.f);

    auto posOne = toVector(reg.view<Position>());
    assert(posOne.size() == 1 && posOne[0] == e1);

    std::cout << "[OK]  testCreateAndQuery passed.\n\n";
}

/**
 * Test 2: Add multiple components to the same entity and verify correctness.
 */
static void testAddMultipleComponents()
{
    std::cout << "[Test] Add Multiple Components\n";

    entt::registry reg;
    entt::entity e = reg.create();

    auto& pos = reg.emplace<Position>(e, 1.f, 2.f);
    auto& vel = reg.emplace<Velocity>(e, 0.1f, 0.2f);
    auto& hp = reg.emplace<Health>(e, 80);

    // Validate component values
    assert(std::fabs(pos.x - 1.f) < 1e-6f && std::fabs(pos.y - 2.f) < 1e-6f);
    assert(std::fabs(vel.vx - 0.1f) < 1e-6f && std::fabs(vel.vy - 0.2f) < 1e-6f);
    assert(hp.hp == 80);

    // Individual queries
    assert(toVector(reg.view<Position>()) == std::vector{ e });
    assert(toVector(reg.view<Velocity>()) == std::vector{ e });
    assert(toVector(reg.view<Health>()) == std::vector{ e });

    // Combination queries
    assert(toVector(reg.view<Position, Velocity>()) == std::vector{ e });
    assert(toVector(reg.view<Health, Velocity>()) == std::vector{ e });
    assert(toVector(reg.view<Position, Velocity, Health>()) == std::vector{ e });

    std::cout << "[OK] testAddMultipleComponents passed.\n\n";
}

/**
 * Test 3: Remove a component from an entity and verify migration.
 */
static void testRemoveComponent()
{
    std::cout << "[Test] Remove Component\n";

    entt::registry reg;
    entt::entity e = reg.create();

    reg.emplace<Position>(e, 2.f, 3.f);
    reg.emplace<Velocity>(e, 1.f, 0.f);
    reg.emplace<Health>(e, 50);

    assert(!toVector(reg.view<Position, Velocity, Health>()).empty());

    // Remove Velocity
    reg.remove<Velocity>(e);

    assert(toVector(reg.view<Position, Health>()) == std::vector{ e });
    assert(toVector(reg.view<Velocity>()).empty());

    std::cout << "[OK] testRemoveComponent passed.\n\n";
}

/**
 * Test 4: Destroy an entity and verify it disappears from queries.
 */
static void testDestroyEntity()
{
    std::cout << "[Test] Destroy Entity\n";

    entt::registry reg;
    entt::entity e1 = reg.create();
    entt::entity e2 = reg.create();

    reg.emplace<Position>(e1, 10.f, 10.f);
    reg.emplace<Velocity>(e2, 1.f, 1.f);

    assert(toVector(reg.view<Position>()) == std::vector{ e1 });
    assert(toVector(reg.view<Velocity>()) == std::vector{ e2 });

    reg.destroy(e1);

    assert(toVector(reg.view<Position>()).empty());
    assert(toVector(reg.view<Velocity>()) == std::vector{ e2 });

    std::cout << "[OK] testDestroyEntity passed.\n\n";
}

/**
 * Test 5: Randomly create/remove entities – partial stress test.
 */
static void testRandomCreateRemove()
{
    std::cout << "[Test] Random Create/Remove\n";

    entt::registry reg;
    constexpr int N = 100;
    std::vector<entt::entity> entities;
    entities.reserve(N);

    for (int i = 0; i < N; ++i) {
        entt::entity e = reg.create();
        entities.push_back(e);
        if (i % 2 == 0)
            reg.emplace<Position>(e, (float)i, (float)(i + 1));
        else
            reg.emplace<Velocity>(e, (float)i * 0.1f, (float)i * 0.2f);
    }

    assert(toVector(reg.view<Position>()).size() == 50);
    assert(toVector(reg.view<Velocity>()).size() == 50);

    // Destroy half
    for (int i = 0; i < N / 2; ++i)
        reg.destroy(entities[i]);

    // Add/mix components in survivors
    for (int i = N / 2; i < N; ++i) {
        auto e = entities[i];
        if (i % 2 == 0) {   // had Position
            reg.emplace_or_replace<Velocity>(e, 0.01f, 0.02f);
        }
        else {          // had Velocity
            reg.emplace_or_replace<Position>(e, (float)i, (float)i);
        }
    }

    std::cout << "  posVelBoth size = "
        << toVector(reg.view<Position, Velocity>()).size() << '\n';

    std::cout << "[OK] testRandomCreateRemove passed.\n\n";
}

/**
 * Runs all functional tests.
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

static void testPerformance()
{
    using clock = std::chrono::high_resolution_clock;
    std::cout << "[Performance Test] Start\n";

    constexpr int N = 1'000'000;
    entt::registry reg;
    std::vector<entt::entity> entities;
    entities.reserve(N);

    // 1) Massive creation & component assignment
    auto t0 = clock::now();
    for (int i = 0; i < N; ++i) {
        entt::entity e = reg.create();
        entities.push_back(e);
        switch (i % 3) {
        case 0:
            reg.emplace<Position>(e, (float)i, (float)i);
            reg.emplace<Velocity>(e, (float)i * 0.01f, (float)i * 0.02f);
            break;
        case 1:
            reg.emplace<Velocity>(e, 1.f, 0.f);
            reg.emplace<Health>(e, 100 + i % 50);
            break;
        default:
            reg.emplace<Position>(e, (float)i * 0.1f, (float)i * 0.1f);
            break;
        }
    }
    auto t1 = clock::now();
    std::cout << "  Created " << N << " entities in "
        << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
        << " ms\n";

    // 2) Query performance
    auto q0 = clock::now();
    auto posView = reg.view<Position>();
    auto velView = reg.view<Velocity>();
    auto posVelView = reg.view<Position, Velocity>();
    auto healthView = reg.view<Health>();
    auto q1 = clock::now();

    std::cout << "  queryEntities<Position>() => "
        << posView.size()    // ① 这里改用 size()
        << " results\n";
    std::cout << "  queryEntities<Velocity>() => "
        << velView.size()    // ② 这里改用 size()
        << " results\n";
    std::cout << "  queryEntities<Position,Velocity>() => "
        << posVelView.size_hint()     // multi-component 仍用 size_hint()
        << " results\n";
    std::cout << "  queryEntities<Health>() => "
        << healthView.size()  // ③ 这里改用 size()
        << " results\n";
    std::cout << "  Query time: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(q1 - q0).count()
        << " ms\n";

    // 3) Repeated getComponent performance (fetch half)
    auto g0 = clock::now();
    for (int i = 0; i < N / 2; ++i) {
        if (auto p = reg.try_get<Position>(entities[i])) {
            volatile float dummy = p->x; (void)dummy;
        }
    }
    auto g1 = clock::now();
    std::cout << "  Performed " << (N / 2)
        << " getComponent<Position> calls in "
        << std::chrono::duration_cast<std::chrono::milliseconds>(g1 - g0).count()
        << " ms\n";

    // 4) Remove Velocity from first 100 000 entities
    constexpr int removeCount = 100'000;
    auto r0 = clock::now();
    for (int i = 0; i < removeCount; ++i)
        reg.remove<Velocity>(entities[i]);
    auto r1 = clock::now();
    std::cout << "  Removed Velocity from " << removeCount << " entities in "
        << std::chrono::duration_cast<std::chrono::milliseconds>(r1 - r0).count()
        << " ms\n";

    std::cout << "[Performance Test] Done\n\n";
}

/**
 * Test entry point.
 */
int main()
{
    runAllFunctionTests();
    testPerformance();
    return 0;
}
