#include <iostream>
#include <thread>
#include <vector>
#include <cassert>

#include <lux/cxx/concurrent/LockFreeQueue.hpp>

static const int TEST_CAPACITY = 8;

using lux::cxx::SpscLockFreeRingQueue;

/**
 * @brief Single-thread test to verify basic push/pop without concurrency.
 */
void testSingleThread()
{
    std::cout << "[testSingleThread] Start\n";

    SpscLockFreeRingQueue<int> queue(TEST_CAPACITY);

    // Push until full
    for (int i = 0; i < TEST_CAPACITY - 1; ++i)  // capacity-1 is the max usable in ring
    {
        bool success = queue.push(i);
        assert(success);
    }
    // Now it should be full
    bool shouldFail = queue.push(999);
    assert(!shouldFail && "Queue should be full now.");

    // Pop and check
    for (int i = 0; i < TEST_CAPACITY - 1; ++i)
    {
        int val = -1;
        bool success = queue.pop(val);
        assert(success);
        assert(val == i);
    }
    // Now it should be empty
    int dummy = 0;
    bool popFail = queue.pop(dummy);
    assert(!popFail && "Queue should be empty now.");

    // Test close
    queue.close();
    // Closed queue rejects pushes
    bool pushClosed = queue.push(123);
    assert(!pushClosed && "Should fail push since closed.");

    std::cout << "[testSingleThread] Passed\n";
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
            while (!queue.push(i))
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
            if (queue.pop(val))
            {
                results.push_back(val);
            }
            else
            {
                if (queue.closed())
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
    testMultiThread();

    std::cout << "All SPSC tests passed successfully!\n";
    return 0;
}
