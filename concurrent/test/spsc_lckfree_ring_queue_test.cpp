#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <type_traits>
#include <utility>

#include <lux/cxx/concurrent/LockFreeQueue.hpp>

static const int TEST_CAPACITY = 8;

using lux::cxx::SpscLockFreeRingQueue;
using lux::cxx::EQueuePushResult;
using lux::cxx::EQueuePopResult;

// ── conditional-noexcept contract (compile-time) ─────────────────────────────
// emplace/push/pop must be noexcept for nothrow element types, and must NOT be
// noexcept for throwing ones (otherwise a throwing T turns a normal failure into
// std::terminate inside a noexcept function).
namespace
{
    struct NothrowElem
    {
        NothrowElem() noexcept {}
        explicit NothrowElem(int) noexcept {}
        NothrowElem(NothrowElem&&) noexcept = default;
        NothrowElem& operator=(NothrowElem&&) noexcept = default;
    };
    struct ThrowingElem
    {
        ThrowingElem() {}
        ThrowingElem(ThrowingElem&&) {}                              // may throw
        ThrowingElem& operator=(ThrowingElem&&) { return *this; }    // may throw
    };
    struct MoveConstructOnly
    {
        explicit MoveConstructOnly(int value) noexcept : value(value) {}
        MoveConstructOnly(const MoveConstructOnly&) = delete;
        MoveConstructOnly& operator=(const MoveConstructOnly&) = delete;
        MoveConstructOnly(MoveConstructOnly&&) noexcept = default;
        MoveConstructOnly& operator=(MoveConstructOnly&&) = delete;
        int value = 0;
    };

    static_assert(noexcept(std::declval<SpscLockFreeRingQueue<NothrowElem>&>().tryEmplace()),
                  "tryEmplace must be noexcept for nothrow-constructible T");
    static_assert(noexcept(std::declval<SpscLockFreeRingQueue<NothrowElem>&>().tryPop(
                      std::declval<NothrowElem&>())),
                  "tryPop must be noexcept for nothrow move/destroy T");
    static_assert(!noexcept(std::declval<SpscLockFreeRingQueue<ThrowingElem>&>().tryEmplace(
                      std::declval<ThrowingElem>())),
                  "tryEmplace must NOT be noexcept for throwing-constructible T");
    static_assert(!noexcept(std::declval<SpscLockFreeRingQueue<ThrowingElem>&>().tryPop(
                      std::declval<ThrowingElem&>())),
                  "tryPop must NOT be noexcept for throwing move T");
    static_assert(noexcept(
        std::declval<SpscLockFreeRingQueue<MoveConstructOnly>&>().tryPopValue()),
        "value tryPop must be noexcept for nothrow move construction");
}

/**
 * @brief Single-thread test to verify basic push/pop without concurrency.
 */
void testSingleThread()
{
    std::cout << "[testSingleThread] Start\n";

    SpscLockFreeRingQueue<int> queue(TEST_CAPACITY);
    assert(queue.capacity() == TEST_CAPACITY);

    // Push until full
    for (int i = 0; i < TEST_CAPACITY; ++i)
    {
        assert(queue.tryPush(i) == EQueuePushResult::ACCEPTED);
    }
    // Now it should be full
    assert(queue.tryPush(999) == EQueuePushResult::FULL);

    // Pop and check
    for (int i = 0; i < TEST_CAPACITY; ++i)
    {
        int val = -1;
        const auto result = queue.tryPop(val);
        assert(result == EQueuePopResult::VALUE);
        assert(val == i);
    }
    // Now it should be empty
    int dummy = 0;
    const auto popFail = queue.tryPop(dummy);
    assert(popFail == EQueuePopResult::EMPTY && "Queue should be empty now.");

    // Test close
    queue.close();
    // Closed queue rejects pushes
    assert(queue.tryPush(123) == EQueuePushResult::CLOSED);

    std::cout << "[testSingleThread] Passed\n";
}

void testMoveConstructOnlyPop()
{
    SpscLockFreeRingQueue<MoveConstructOnly> queue(4);
    assert(queue.tryEmplace(42) == EQueuePushResult::ACCEPTED);
    auto value = queue.tryPopValue();
    assert(value.result == EQueuePopResult::VALUE);
    assert(value.value->value == 42);
    assert(queue.tryPopValue().result == EQueuePopResult::EMPTY);
}

void testMinimumCapacity()
{
    SpscLockFreeRingQueue<int> queue(1);
    assert(queue.capacity() == 1);
    assert(queue.tryPush(7) == EQueuePushResult::ACCEPTED);
    assert(queue.tryPush(8) == EQueuePushResult::FULL);
    int value = 0;
    assert(queue.tryPop(value) == EQueuePopResult::VALUE);
    assert(value == 7);
}

/**
 * @brief Multi-thread test with a single producer and single consumer.
 */
void testMultiThread()
{
    std::cout << "[testMultiThread] Start\n";

    SpscLockFreeRingQueue<int> queue(TEST_CAPACITY);
    const int TOTAL_ITEMS = 100;

    // Producer: push 100 items
    std::thread producer([&]() {
        for (int i = 0; i < TOTAL_ITEMS; ++i)
        {
            // Spin until push succeeds
            while (queue.tryPush(i) != EQueuePushResult::ACCEPTED)
            {
                // The queue might be full, wait briefly
                std::this_thread::yield();
            }
        }
        // Then close
        queue.close();
    });

    // Consumer: pop until closed and empty
    int count = 0;
    std::vector<int> results;
    results.reserve(TOTAL_ITEMS);

    std::thread consumer([&]() {
        while (true)
        {
            int val = -1;
            const auto result = queue.tryPop(val);
            if (result == EQueuePopResult::VALUE)
            {
                results.push_back(val);
            }
            else
            {
                if (result == EQueuePopResult::CLOSED_AND_DRAINED)
                {
                    // Once closed and no item popped, we end
                    break;
                }
                else
                {
                    // queue empty but not closed
                    std::this_thread::yield();
                }
            }
        }
    });

    producer.join();
    consumer.join();

    // Verify we got all items
    assert(results.size() == (size_t)TOTAL_ITEMS);
    for (int i = 0; i < TOTAL_ITEMS; ++i)
    {
        assert(results[i] == i);
    }

    std::cout << "[testMultiThread] Passed\n";
}


int main()
{
    testSingleThread();
    testMoveConstructOnlyPop();
    testMinimumCapacity();
    testMultiThread();

    std::cout << "All SPSC tests passed successfully!\n";
    return 0;
}
