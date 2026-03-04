// ============================================================================
// object_pool_test.cpp
// ----------------------------------------------------------------------------
// Comprehensive tests for lux::cxx::ObjectPool and lux::cxx::pool_ptr.
// Covers both single-threaded (ThreadSafe=false) and lock-free (ThreadSafe=true)
// modes, including multi-threaded stress tests.
// ============================================================================
#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <set>
#include <thread>
#include <atomic>
#include <algorithm>
#include <memory>

#include <lux/cxx/memory/ObjectPool.hpp>

// ─── helpers ────────────────────────────────────────────────────────────────

static std::atomic<int> g_ctor_count{0};
static std::atomic<int> g_dtor_count{0};

struct Tracked
{
    int value;
    explicit Tracked(int v = 0) : value(v) { g_ctor_count.fetch_add(1, std::memory_order_relaxed); }
    ~Tracked() { g_dtor_count.fetch_add(1, std::memory_order_relaxed); }
};

static void reset_counters()
{
    g_ctor_count.store(0, std::memory_order_relaxed);
    g_dtor_count.store(0, std::memory_order_relaxed);
}

// ─── 1. basic acquire/release (single-threaded) ────────────────────────────

void test_basic_acquire_release()
{
    std::cout << "  basic acquire/release ... ";
    reset_counters();

    lux::cxx::ObjectPool<Tracked, 4, false> pool;

    auto* a = pool.acquire(10);
    assert(a != nullptr);
    assert(a->value == 10);

    auto* b = pool.acquire(20);
    assert(b != nullptr);
    assert(b->value == 20);
    assert(a != b);

    pool.release(a);
    pool.release(b);

    assert(g_ctor_count.load() == 2);
    assert(g_dtor_count.load() == 2);

    std::cout << "passed\n";
}

// ─── 2. pointer stability across chunks ────────────────────────────────────

void test_pointer_stability()
{
    std::cout << "  pointer stability ... ";

    lux::cxx::ObjectPool<int, 4, false> pool;

    // Acquire more than one chunk's worth of objects.
    std::vector<int*> ptrs;
    for (int i = 0; i < 20; ++i)
        ptrs.push_back(pool.acquire(i));

    // All pointers must be distinct.
    std::set<int*> unique(ptrs.begin(), ptrs.end());
    assert(unique.size() == 20);

    // Values intact.
    for (int i = 0; i < 20; ++i)
        assert(*ptrs[i] == i);

    // Release all.
    for (auto* p : ptrs)
        pool.release(p);

    std::cout << "passed\n";
}

// ─── 3. slot reuse ─────────────────────────────────────────────────────────

void test_slot_reuse()
{
    std::cout << "  slot reuse ... ";

    lux::cxx::ObjectPool<int, 4, false> pool;

    auto* a = pool.acquire(1);
    auto* b = pool.acquire(2);
    void* addr_a = a;
    void* addr_b = b;

    pool.release(b);
    pool.release(a);

    // Next two acquires should reuse the same slots (LIFO free list).
    auto* c = pool.acquire(3);
    auto* d = pool.acquire(4);

    // The addresses should be the same as a and b (in some order).
    assert((static_cast<void*>(c) == addr_a || static_cast<void*>(c) == addr_b));
    assert((static_cast<void*>(d) == addr_a || static_cast<void*>(d) == addr_b));
    assert(c != d);

    pool.release(c);
    pool.release(d);

    std::cout << "passed\n";
}

// ─── 4. pool_ptr RAII ──────────────────────────────────────────────────────

void test_pool_ptr_raii()
{
    std::cout << "  pool_ptr RAII ... ";
    reset_counters();

    lux::cxx::ObjectPool<Tracked, 8, false> pool;

    {
        auto sp = pool.acquire_scoped(42);
        assert(sp);
        assert(sp->value == 42);
        assert((*sp).value == 42);
        assert(sp.get() != nullptr);
    } // sp destroyed here → release called automatically

    assert(g_ctor_count.load() == 1);
    assert(g_dtor_count.load() == 1);

    std::cout << "passed\n";
}

// ─── 5. pool_ptr move semantics ────────────────────────────────────────────

void test_pool_ptr_move()
{
    std::cout << "  pool_ptr move ... ";
    reset_counters();

    lux::cxx::ObjectPool<Tracked, 8, false> pool;

    auto sp1 = pool.acquire_scoped(100);
    auto* raw = sp1.get();

    // Move construct.
    auto sp2 = std::move(sp1);
    assert(!sp1);
    assert(sp2);
    assert(sp2.get() == raw);

    // Move assign.
    lux::cxx::pool_ptr<Tracked, 8, false> sp3;
    sp3 = std::move(sp2);
    assert(!sp2);
    assert(sp3);
    assert(sp3.get() == raw);

    sp3.reset();
    assert(!sp3);
    assert(g_dtor_count.load() == 1);

    std::cout << "passed\n";
}

// ─── 6. pool_ptr release() (manual detach) ─────────────────────────────────

void test_pool_ptr_release()
{
    std::cout << "  pool_ptr release() ... ";
    reset_counters();

    lux::cxx::ObjectPool<Tracked, 8, false> pool;

    auto sp = pool.acquire_scoped(77);
    auto* raw = sp.release();
    assert(!sp);
    assert(raw->value == 77);

    // Must manually release since pool_ptr no longer owns it.
    pool.release(raw);

    assert(g_ctor_count.load() == 1);
    assert(g_dtor_count.load() == 1);

    std::cout << "passed\n";
}

// ─── 7. reserve / capacity ─────────────────────────────────────────────────

void test_reserve_capacity()
{
    std::cout << "  reserve / capacity ... ";

    lux::cxx::ObjectPool<int, 4, false> pool;

    assert(pool.capacity() == 0);
    assert(pool.chunk_count() == 0);

    pool.reserve(10);
    // With ChunkSize=4, need at least ceil(10/4)=3 chunks → 12 slots.
    assert(pool.capacity() >= 10);
    assert(pool.chunk_count() >= 3);

    // Pre-allocated constructor.
    lux::cxx::ObjectPool<int, 8, false> pool2(20);
    assert(pool2.capacity() >= 20);

    std::cout << "passed\n";
}

// ─── 8. release(nullptr) is a no-op ────────────────────────────────────────

void test_release_nullptr()
{
    std::cout << "  release(nullptr) ... ";
    reset_counters();

    lux::cxx::ObjectPool<Tracked, 4, false> pool;
    pool.release(nullptr); // must not crash

    assert(g_dtor_count.load() == 0);

    std::cout << "passed\n";
}

// ─── 9. move-only types ────────────────────────────────────────────────────

void test_move_only_types()
{
    std::cout << "  move-only types ... ";

    lux::cxx::ObjectPool<std::unique_ptr<int>, 4, false> pool;

    auto* p = pool.acquire(std::make_unique<int>(99));
    assert(p != nullptr);
    assert(**p == 99);

    pool.release(p);

    std::cout << "passed\n";
}

// ─── 10. lock-free basic (ThreadSafe=true) ─────────────────────────────────

void test_lockfree_basic()
{
    std::cout << "  lock-free basic ... ";
    reset_counters();

    lux::cxx::ObjectPool<Tracked, 8, true> pool;

    auto* a = pool.acquire(1);
    auto* b = pool.acquire(2);
    assert(a->value == 1);
    assert(b->value == 2);

    pool.release(a);
    pool.release(b);

    assert(g_ctor_count.load() == 2);
    assert(g_dtor_count.load() == 2);

    std::cout << "passed\n";
}

// ─── 11. lock-free pool_ptr ────────────────────────────────────────────────

void test_lockfree_pool_ptr()
{
    std::cout << "  lock-free pool_ptr ... ";
    reset_counters();

    lux::cxx::ObjectPool<Tracked, 8, true> pool;

    {
        auto sp = pool.acquire_scoped(42);
        assert(sp->value == 42);
    }

    assert(g_ctor_count.load() == 1);
    assert(g_dtor_count.load() == 1);

    std::cout << "passed\n";
}

// ─── 12. multi-threaded stress test ────────────────────────────────────────

void test_mt_stress()
{
    std::cout << "  multi-threaded stress ... ";

    constexpr int NUM_THREADS      = 8;
    constexpr int OPS_PER_THREAD   = 10000;

    lux::cxx::ObjectPool<int, 64, true> pool;
    std::atomic<int> total_acquired{0};
    std::atomic<int> total_released{0};

    auto worker = [&]()
    {
        std::vector<int*> local;
        local.reserve(OPS_PER_THREAD);

        for (int i = 0; i < OPS_PER_THREAD; ++i)
        {
            auto* p = pool.acquire(i);
            assert(p != nullptr);
            assert(*p == i);
            local.push_back(p);
            total_acquired.fetch_add(1, std::memory_order_relaxed);

            // Release roughly half the objects periodically to exercise
            // concurrent acquire+release interleaving.
            if (local.size() > 50)
            {
                for (std::size_t j = 0; j < 25; ++j)
                {
                    pool.release(local.back());
                    local.pop_back();
                    total_released.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }

        // Release remaining.
        for (auto* p : local)
        {
            pool.release(p);
            total_released.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; ++i)
        threads.emplace_back(worker);
    for (auto& t : threads)
        t.join();

    assert(total_acquired.load() == NUM_THREADS * OPS_PER_THREAD);
    assert(total_released.load() == NUM_THREADS * OPS_PER_THREAD);

    std::cout << "passed\n";
}

// ─── 13. multi-threaded with pool_ptr ──────────────────────────────────────

void test_mt_pool_ptr()
{
    std::cout << "  multi-threaded pool_ptr ... ";

    constexpr int NUM_THREADS    = 4;
    constexpr int OPS_PER_THREAD = 5000;

    lux::cxx::ObjectPool<std::string, 32, true> pool;
    std::atomic<int> alive{0};

    auto worker = [&]()
    {
        for (int i = 0; i < OPS_PER_THREAD; ++i)
        {
            auto sp = pool.acquire_scoped("hello");
            assert(sp);
            assert(*sp == "hello");
            alive.fetch_add(1, std::memory_order_relaxed);
        } // pool_ptr release here
    };

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; ++i)
        threads.emplace_back(worker);
    for (auto& t : threads)
        t.join();

    assert(alive.load() == NUM_THREADS * OPS_PER_THREAD);

    std::cout << "passed\n";
}

// ─── 14. large objects & alignment ─────────────────────────────────────────

void test_large_aligned_objects()
{
    std::cout << "  large aligned objects ... ";

    struct alignas(64) BigObj
    {
        char data[256];
        int  id;
    };

    lux::cxx::ObjectPool<BigObj, 4, false> pool;

    auto* a = pool.acquire();
    a->id = 1;
    auto* b = pool.acquire();
    b->id = 2;

    // Check alignment.
    assert(reinterpret_cast<std::uintptr_t>(a) % 64 == 0);
    assert(reinterpret_cast<std::uintptr_t>(b) % 64 == 0);

    pool.release(a);
    pool.release(b);

    std::cout << "passed\n";
}

// ─── main ───────────────────────────────────────────────────────────────────

int main()
{
    std::cout << "ObjectPool tests (single-threaded):\n";
    test_basic_acquire_release();
    test_pointer_stability();
    test_slot_reuse();
    test_pool_ptr_raii();
    test_pool_ptr_move();
    test_pool_ptr_release();
    test_reserve_capacity();
    test_release_nullptr();
    test_move_only_types();
    test_large_aligned_objects();

    std::cout << "\nObjectPool tests (lock-free):\n";
    test_lockfree_basic();
    test_lockfree_pool_ptr();
    test_mt_stress();
    test_mt_pool_ptr();

    std::cout << "\nAll ObjectPool tests passed!\n";
    return 0;
}
