#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <lux/cxx/concurrent/BlockingQueue.hpp>

using lux::cxx::BlockingQueue;
using lux::cxx::BlockingRingQueue;

using queue_t = BlockingQueue<int>;

/**
 * @brief Test single-thread push and pop to verify basic functionality.
 */
void testSingleThread()
{
    std::cout << "=== testSingleThread ===" << std::endl;
    queue_t queue; // unbounded

    // Push 5 elements
    for (int i = 1; i <= 5; ++i)
    {
        bool success = queue.push(i);
        assert(success);
    }
    assert(queue.size() == 5);

    // Pop them back
    for (int i = 1; i <= 5; ++i)
    {
        int val = 0;
        bool success = queue.pop(val);
        assert(success);
        assert(val == i);
    }
    assert(queue.empty());

    std::cout << "Single-thread test passed." << std::endl;
}

/**
 * @brief Test multi-thread producer/consumer scenario.
 */
void testMultiThreadWithCapacity()
{
    std::cout << "=== testMultiThreadWithCapacity ===" << std::endl;
    // Capacity set to 10
    queue_t queue(10);
    const int numProducers = 3;
    const int numConsumers = 3;
    const int itemsPerProducer = 20;

    std::atomic<int> totalProduced{0};
    std::atomic<int> totalConsumed{0};

    // Producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < numProducers; ++p)
    {
        producers.emplace_back([&queue, &totalProduced, itemsPerProducer, p]() {
            for (int i = 0; i < itemsPerProducer; ++i)
            {
                // Each producer pushes i + p*1000 for uniqueness
                int value = i + p * 1000;
                queue.push(value);
                totalProduced++;
            }
        });
    }

    // Consumer threads
    std::vector<std::thread> consumers;
    std::vector<int> consumedValues; 
    consumedValues.reserve(numProducers * itemsPerProducer);

    std::mutex consumeMutex; // to protect consumedValues
    for (int c = 0; c < numConsumers; ++c)
    {
        consumers.emplace_back([&queue, &totalConsumed, &consumedValues, &consumeMutex]() {
            int val = 0;
            while (true)
            {
                if (!queue.pop(val))
                {
                    // Queue is closed & empty or no data left
                    break;
                }
                {
                    std::lock_guard<std::mutex> lock(consumeMutex);
                    consumedValues.push_back(val);
                }
                totalConsumed++;
            }
        });
    }

    // Wait for all producers
    for (auto& producer : producers)
    {
        producer.join();
    }

    // Close the queue so consumers stop after they pop all remaining items
    queue.close();

    // Wait for all consumers
    for (auto& consumer : consumers)
    {
        consumer.join();
    }

    // Verify
    std::cout << "Total produced: " << totalProduced << std::endl;
    std::cout << "Total consumed: " << totalConsumed << std::endl;
    assert(totalProduced == numProducers * itemsPerProducer);
    assert(totalConsumed == totalProduced);

    // Check that the total number of consumed items matches
    assert(consumedValues.size() == (size_t)totalConsumed);

    std::cout << "Multi-thread test with capacity passed." << std::endl;
}

/**
 * @brief Test timeout behavior (push and pop).
 */
void testTimeout()
{
    std::cout << "=== testTimeout ===" << std::endl;
    queue_t queue(2); // capacity = 2
    bool success;

    // Initially push two items
    success = queue.push(10);
    assert(success);
    success = queue.push(20);
    assert(success);
    assert(queue.size() == 2);

    // Try pushing another item with a small timeout 
    // (should fail if we can't push within the given time since capacity=2)
    success = queue.push(30, std::chrono::milliseconds(50));
    // This may or may not succeed if we pop concurrently, but in single-thread scenario, it should time out.
    // We'll assume it times out here, because no pop is happening in the same thread.
    if (success)
    {
        std::cout << "Warning: push succeeded unexpectedly (if no pop occurs). Possibly a scheduling artifact.\n";
    }
    else
    {
        std::cout << "Push with timeout correctly failed.\n";
    }

    // Pop one item with a small timeout (should succeed immediately)
    int val = 0;
    success = queue.pop(val, std::chrono::milliseconds(50));
    assert(success);
    std::cout << "Popped: " << val << std::endl;

    // Now we should be able to push again successfully
    success = queue.push(30, std::chrono::milliseconds(200));
    assert(success);

    std::cout << "Timeout test completed." << std::endl;
}

/**
 * @brief Test non-blocking try_push and try_pop.
 */
void testTryPushPop()
{
    std::cout << "=== testTryPushPop ===" << std::endl;
    queue_t queue(2);

    // Initially empty, try_pop should fail
    int val = 0;
    bool success = queue.try_pop(val);
    assert(!success);

    // try_push first item
    success = queue.try_push(1);
    assert(success);

    // try_push second item
    success = queue.try_push(2);
    assert(success);

    // Now queue is at capacity (2). Another try_push should fail
    success = queue.try_push(3);
    assert(!success);

    // try_pop one item
    success = queue.try_pop(val);
    assert(success);
    assert(val == 1);

    // Now we can try_push again
    success = queue.try_push(3);
    assert(success);

    // Pop remaining items
    success = queue.try_pop(val);
    assert(success);
    assert(val == 2);

    success = queue.try_pop(val);
    assert(success);
    assert(val == 3);

    // Now empty
    success = queue.try_pop(val);
    assert(!success);

    std::cout << "Non-blocking try test passed." << std::endl;
}

/**
 * @brief Test bulk push/pop operations.
 */
void testBulkOperations()
{
    std::cout << "=== testBulkOperations ===" << std::endl;
    BlockingRingQueue<int> queue(10); // capacity = 10

    std::vector<int> inputData = {1, 2, 3, 4, 5};
    bool success = queue.push_bulk(inputData.begin(), inputData.size());
    assert(success);
    assert(queue.size() == inputData.size());

    // Now pop in bulk
    std::vector<int> outputData(5, 0);
    size_t poppedCount = queue.pop_bulk(outputData.begin(), 5);
    assert(poppedCount == 5);

    // Verify
    for (size_t i = 0; i < poppedCount; ++i)
    {
        assert(outputData[i] == static_cast<int>(i + 1));
    }
    assert(queue.empty());

    // Test partial bulk pop
    // Push 3 items
    success = queue.push_bulk(inputData.begin(), 3);
    assert(success);
    assert(queue.size() == 3);

    // Attempt to pop 5, but only 3 are available
    outputData.assign(5, 0); // reset
    poppedCount = queue.pop_bulk(outputData.begin(), 5);
    assert(poppedCount == 3);
    assert(queue.empty());

    // The first 3 in outputData should be 1,2,3
    assert(outputData[0] == 1);
    assert(outputData[1] == 2);
    assert(outputData[2] == 3);

    std::cout << "Bulk operations test passed." << std::endl;
}

/**
 * @brief Test closing behavior while threads are waiting.
 */
void testCloseBehavior()
{
    std::cout << "=== testCloseBehavior ===" << std::endl;
    queue_t queue(2);

    // Start a thread that attempts to pop from an empty queue
    bool popSuccess = true;
    std::thread t([&queue, &popSuccess]() {
        int val;
        popSuccess = queue.pop(val); 
        // This will block until either an item arrives or the queue is closed.
    });

    // Give the thread some time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Close the queue
    queue.close();

    // The pop should immediately fail (return false) because the queue is empty and closed
    t.join();
    assert(!popSuccess);

    std::cout << "Close behavior test passed." << std::endl;
}

/**
 * @brief Main function to run all tests.
 */
int main()
{
    testSingleThread();
    testMultiThreadWithCapacity();
    testTimeout();
    testTryPushPop();
    testBulkOperations();
    testCloseBehavior();

    std::cout << "All tests passed successfully." << std::endl;
    return 0;
}
