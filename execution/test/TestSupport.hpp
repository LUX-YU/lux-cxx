#pragma once
#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <chrono>
#include <thread>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    std::abort(); } } while (false)

namespace test
{
    using Clock = std::chrono::steady_clock;
    template <class Predicate>
    void eventually(Predicate predicate)
    {
        const auto deadline = Clock::now() + std::chrono::seconds(30);
        while (!predicate())
        {
            CHECK(Clock::now() < deadline);
            std::this_thread::yield();
        }
    }

    struct Case { const char* name; void (*run)(); };
    template <std::size_t N>
    int run(const Case (&cases)[N])
    {
        for (const auto& item : cases)
        {
            std::printf("RUN %s\n", item.name);
            item.run();
            std::printf("PASS %s\n", item.name);
        }
        std::printf("%zu/%zu passed\n", N, N);
        return 0;
    }
}
