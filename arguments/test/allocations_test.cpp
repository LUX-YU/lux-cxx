#include <lux/cxx/arguments/ContainerTraits.hpp>
#include <lux/cxx/arguments/TypedOptions.hpp>
#include <lux/cxx/arguments/DirectBinding.hpp>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

namespace args = lux::cxx::args;
namespace
{
    std::atomic<std::size_t> allocations{0};
    std::atomic<bool> tracking{false};
} // namespace
void* operator new(std::size_t n)
{
    if (tracking.load(std::memory_order_relaxed))
        allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(n ? n : 1))
        return p;
    std::abort();
}
void* operator new[](std::size_t n)
{
    if (tracking.load(std::memory_order_relaxed))
        allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(n ? n : 1))
        return p;
    std::abort();
}
void operator delete(void* p) noexcept
{
    std::free(p);
}
void operator delete[](void* p) noexcept
{
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept
{
    std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept
{
    std::free(p);
}

#define REQUIRE(e)                                                                                         \
    do                                                                                                     \
    {                                                                                                      \
        if (!(e))                                                                                          \
        {                                                                                                  \
            std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__);                                      \
            std::abort();                                                                                  \
        }                                                                                                  \
    } while (false)

int main()
{
    constexpr args::Option<int, "width", "w"> width;
    constexpr args::Option<double, "scale", "s"> scale;
    constexpr args::Option<bool, "enabled", "e"> enabled;
    int w = 0;
    double s = 0;
    bool e = false;
    const auto bw = args::bind(width, w);
    const auto bs = args::bind(scale, s);
    const auto be = args::bind(enabled, e);
    args::Workspace workspace;
    for (int i = 0; i < 100; ++i)
    {
        args::TextSource source{"width=1200 scale=2.5 --enabled"};
        REQUIRE(!args::parseIntoFrom(source, workspace, bw, bs, be));
    }
    allocations.store(0);
    tracking.store(true);
    std::size_t checksum = 0;
    for (int i = 0; i < 20000; ++i)
    {
        args::TextSource source{"width=1200 scale=2.5 --enabled"};
        REQUIRE(!args::parseIntoFrom(source, workspace, bw, bs, be));
        checksum += static_cast<std::size_t>(w) + static_cast<std::size_t>(s) + static_cast<std::size_t>(e);
        auto result = args::parse("width=1200 scale=2.5 --enabled", width, scale, enabled);
        REQUIRE(result && result.get<"width">().value() == 1200);
    }
    tracking.store(false);
    const auto count = allocations.load();
    REQUIRE(checksum != 0 && count == 0);

    // Preallocated destinations are permitted to reuse their own capacity.
    auto words = args::Option<std::vector<int>, "list">{}.multi();
    std::vector<int> list;
    list.reserve(16);
    auto bound = args::bind(words, list);
    allocations.store(0);
    tracking.store(true);
    for (int i = 0; i < 10000; ++i)
    {
        args::TextSource source{"list=1 list=2 list=3"};
        REQUIRE(!args::parseIntoFrom(source, workspace, bound));
        REQUIRE(list.size() == 3 && list[0] == 1 && list[2] == 3);
    }
    tracking.store(false);
    REQUIRE(allocations.load() == 0);
    std::printf("zero-allocation tests PASS (%zu per-loop allocations)\n", count);
}
