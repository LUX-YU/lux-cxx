#include <lux/cxx/concurrent/AdmissionGate.hpp>
#include <lux/cxx/concurrent/BlockingQueue.hpp>
#include <lux/cxx/concurrent/BudgetGate.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <stop_token>
#include <thread>
#include <vector>

int main()
{
    static_assert(sizeof(lux::cxx::AdmissionTicket<>) == sizeof(void*));
    static_assert(lux::cxx::BudgetGate<>::fitsBudget(2, 20, 3, 30, 5, 50));
    static_assert(!lux::cxx::BudgetGate<>::fitsBudget(2, 20, 4, 30, 5, 50));

    lux::cxx::AdmissionGate<> admission;
    auto first = admission.tryAcquire();
    assert(first.has_value());
    assert(admission.inFlight() == 1);
    admission.close();
    assert(!admission.tryAcquire());
    first.reset();
    assert(admission.inFlight() == 0);

    lux::cxx::BudgetGate<> budget(4, 100);
    auto reservation = budget.tryAcquire(2, 60);
    assert(reservation.has_value());
    assert(!budget.tryAcquire(3, 1));
    assert(!budget.tryAcquire(1, 50));
    reservation.reset();
    assert(budget.currentItems() == 0);
    assert(budget.currentBytes() == 0);

    lux::cxx::AdmissionGate<> concurrent_gate;
    std::atomic<std::size_t> acquired{0};
    std::vector<std::jthread> workers;
    for (int index = 0; index < 8; ++index)
    {
        workers.emplace_back([&]
        {
            for (int iteration = 0; iteration < 1000; ++iteration)
            {
                if (auto ticket = concurrent_gate.tryAcquire())
                {
                    acquired.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    workers.clear();
    assert(acquired == 8000);
    assert(concurrent_gate.inFlight() == 0);

    lux::cxx::BlockingQueue<int> queue(1);
    std::stop_source source;
    source.request_stop();
    int output = 0;
    assert(
        queue.waitPop(output, source.get_token()) ==
        lux::cxx::EQueuePopResult::CANCELLED
    );
    assert(
        queue.waitPop(output, std::chrono::milliseconds(1)) ==
        lux::cxx::EQueuePopResult::TIMEOUT
    );
    assert(queue.waitPush(42) == lux::cxx::EQueuePushResult::ACCEPTED);
    assert(queue.tryPop(output) == lux::cxx::EQueuePopResult::VALUE);
    assert(output == 42);
    queue.close();
    assert(queue.state() == lux::cxx::EQueueState::DRAINED);
    return 0;
}
