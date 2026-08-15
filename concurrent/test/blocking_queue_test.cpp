#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <lux/cxx/concurrent/BlockingQueue.hpp>

using lux::cxx::BlockingQueue;
using lux::cxx::BlockingRingQueue;
using lux::cxx::EQueuePushResult;
using lux::cxx::EQueuePopResult;

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
        assert(queue.waitPush(i) == EQueuePushResult::ACCEPTED);
    }
    assert(queue.size() == 5);

    // Pop them back
    for (int i = 1; i <= 5; ++i)
    {
        int val = 0;
        assert(queue.waitPop(val) == EQueuePopResult::VALUE);
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
                assert(queue.waitPush(value) == EQueuePushResult::ACCEPTED);
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
                if (queue.waitPop(val) != EQueuePopResult::VALUE)
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
    // Initially push two items
    assert(queue.waitPush(10) == EQueuePushResult::ACCEPTED);
    assert(queue.waitPush(20) == EQueuePushResult::ACCEPTED);
    assert(queue.size() == 2);

    // Try pushing another item with a small timeout 
    // (should fail if we can't push within the given time since capacity=2)
    assert(
        queue.waitPush(30, std::chrono::milliseconds(50))
        == EQueuePushResult::TIMEOUT
    );

    // Pop one item with a small timeout (should succeed immediately)
    int val = 0;
    assert(
        queue.waitPop(val, std::chrono::milliseconds(50))
        == EQueuePopResult::VALUE
    );
    std::cout << "Popped: " << val << std::endl;

    // Now we should be able to push again successfully
    assert(
        queue.waitPush(30, std::chrono::milliseconds(200))
        == EQueuePushResult::ACCEPTED
    );

    std::cout << "Timeout test completed." << std::endl;
}

/**
 * @brief Test non-blocking tryPush and tryPop.
 */
void testTryPushPop()
{
    std::cout << "=== testTryPushPop ===" << std::endl;
    queue_t queue(2);

    // Initially empty, tryPop reports EMPTY.
    int val = 0;
    assert(queue.tryPop(val) == EQueuePopResult::EMPTY);

    // tryPush first item
    assert(queue.tryPush(1) == EQueuePushResult::ACCEPTED);

    // tryPush second item
    assert(queue.tryPush(2) == EQueuePushResult::ACCEPTED);

    // Now queue is at capacity (2). Another tryPush should fail
    assert(queue.tryPush(3) == EQueuePushResult::FULL);

    // tryPop one item
    assert(queue.tryPop(val) == EQueuePopResult::VALUE);
    assert(val == 1);

    // Now we can tryPush again
    assert(queue.tryPush(3) == EQueuePushResult::ACCEPTED);

    // Pop remaining items
    assert(queue.tryPop(val) == EQueuePopResult::VALUE);
    assert(val == 2);

    assert(queue.tryPop(val) == EQueuePopResult::VALUE);
    assert(val == 3);

    // Now empty
    assert(queue.tryPop(val) == EQueuePopResult::EMPTY);

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
    EQueuePopResult popResult = EQueuePopResult::VALUE;
    std::thread t([&queue, &popResult]() {
        int val;
        popResult = queue.waitPop(val);
        // This will block until either an item arrives or the queue is closed.
    });

    // Give the thread some time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Close the queue
    queue.close();

    // The pop should immediately fail (return false) because the queue is empty and closed
    t.join();
    assert(popResult == EQueuePopResult::CLOSED_AND_DRAINED);

    std::cout << "Close behavior test passed." << std::endl;
}

void testDetailedTryPushStatus()
{
    BlockingRingQueue<int> ring(1);
    assert(ring.tryPush(1) == EQueuePushResult::ACCEPTED);
    assert(ring.tryPush(2) == EQueuePushResult::FULL);
    ring.close();
    assert(ring.tryPush(3) == EQueuePushResult::CLOSED);

    BlockingQueue<int> queue(1);
    assert(queue.tryPush(1) == EQueuePushResult::ACCEPTED);
    assert(queue.tryPush(2) == EQueuePushResult::FULL);
    queue.close();
    assert(queue.tryPush(3) == EQueuePushResult::CLOSED);
}

void testRingTryPop()
{
    BlockingRingQueue<int> queue(2);
    int value = -1;
    assert(queue.tryPop(value) == EQueuePopResult::EMPTY);
    assert(queue.tryPush(7) == EQueuePushResult::ACCEPTED);
    assert(queue.tryPush(9) == EQueuePushResult::ACCEPTED);
    assert(queue.tryPop(value) == EQueuePopResult::VALUE && value == 7);
    assert(queue.tryPop(value) == EQueuePopResult::VALUE && value == 9);
    assert(queue.tryPop(value) == EQueuePopResult::EMPTY);
    queue.close();
    assert(queue.tryPop(value) == EQueuePopResult::CLOSED_AND_DRAINED);
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
    testDetailedTryPushStatus();
    testRingTryPop();

    std::cout << "All tests passed successfully." << std::endl;
    return 0;
}
