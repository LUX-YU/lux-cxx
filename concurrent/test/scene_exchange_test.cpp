#include <lux/cxx/concurrent/BoundedSpscFrameRing.hpp>
#include <lux/cxx/concurrent/LatestSpscExchange.hpp>

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <thread>

namespace
{
    struct Observation final
    {
        std::uint64_t sequence{};
        std::uint64_t check{};
    };

    void latestObservation()
    {
        lux::cxx::LatestSpscExchange<Observation> exchange;
        assert(!exchange.acquireLatest());
        exchange.write() = {1, 11};
        exchange.publish();
        exchange.write() = {2, 22};
        exchange.publish();
        assert(exchange.acquireLatest());
        assert(exchange.read().sequence == 2 && exchange.read().check == 22);
        assert(!exchange.acquireLatest());

        constexpr std::uint64_t count = 100000;
        std::jthread producer(
            [&]
            {
                for (std::uint64_t value = 3; value <= count; ++value)
                {
                    exchange.write() = {value, value * 11};
                    exchange.publish();
                }
            });
        std::uint64_t previous = 2;
        while (previous != count)
        {
            if (exchange.acquireLatest())
            {
                const auto &value = exchange.read();
                assert(value.sequence > previous);
                assert(value.check == value.sequence * 11);
                previous = value.sequence;
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }

    void reliableFrames()
    {
        lux::cxx::BoundedSpscFrameRing<Observation, 4> ring{2};
        auto *first = ring.tryBeginWrite();
        assert(first == ring.tryBeginWrite());
        *first = {1, 11};
        assert(ring.publishWrite());
        *ring.tryBeginWrite() = {2, 22};
        assert(ring.publishWrite());
        auto *held = ring.tryBeginWrite();
        assert(held);
        *held = {3, 33};
        assert(!ring.publishWrite());
        assert(ring.tryBeginWrite() == held);
        assert(ring.tryAcquireRead() && ring.currentRead().sequence == 1);
        assert(ring.publishWrite());
        assert(ring.tryAcquireRead() && ring.currentRead().sequence == 2);
        assert(ring.tryAcquireRead() && ring.currentRead().sequence == 3);
        assert(!ring.tryAcquireRead());

        constexpr std::uint64_t count = 100000;
        std::jthread producer(
            [&]
            {
                for (std::uint64_t value = 4; value <= count; ++value)
                {
                    Observation *slot{};
                    while (!(slot = ring.tryBeginWrite()))
                    {
                        std::this_thread::yield();
                    }
                    *slot = {value, value * 11};
                    while (!ring.publishWrite())
                    {
                        assert(slot == ring.tryBeginWrite());
                        std::this_thread::yield();
                    }
                }
            });
        for (std::uint64_t expected = 4; expected <= count; ++expected)
        {
            while (!ring.tryAcquireRead())
            {
                std::this_thread::yield();
            }
            assert(ring.currentRead().sequence == expected);
            assert(ring.currentRead().check == expected * 11);
        }
    }

    void retainedSlots()
    {
        auto owner = std::make_shared<int>(1);
        std::weak_ptr<int> retained = owner;
        {
            lux::cxx::BoundedSpscFrameRing<std::shared_ptr<int>, 3> ring{1};
            *ring.tryBeginWrite() = std::move(owner);
            assert(ring.publishWrite());
            assert(ring.tryAcquireRead());
            assert(!retained.expired());
            // Acquisition does not destroy the persistent packet or its resources.
            ring.currentRead().reset();
            assert(retained.expired());
        }
    }
} // namespace

int main()
{
    latestObservation();
    reliableFrames();
    retainedSlots();
}
