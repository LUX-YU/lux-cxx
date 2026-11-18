#include <lux/cxx/arguments/ContainerTraits.hpp>
#include <lux/cxx/arguments/TypedOptions.hpp>
#include <lux/cxx/arguments/DirectBinding.hpp>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace a = lux::cxx::args;
#define REQUIRE(e)                                                                                         \
    do                                                                                                     \
    {                                                                                                      \
        if (!(e))                                                                                          \
        {                                                                                                  \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #e);                              \
            std::abort();                                                                                  \
        }                                                                                                  \
    } while (false)

int main()
{
    constexpr a::Option<int, "a", "a"> one;
    constexpr a::Option<double, "b", "b"> two;
    constexpr a::Option<bool, "c", "c"> three;
    const auto numbers = a::Option<std::vector<int>, "nums", "n">{}.multi();
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    const auto next = [&]() -> std::uint32_t {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return static_cast<std::uint32_t>((state * 0x2545f4914f6cdd1dull) >> 32);
    };
    for (int iteration = 0; iteration < 15000; ++iteration)
    {
        const int v1 = static_cast<int>(next() % 3001) - 1500;
        const int v2 = static_cast<int>(next() % 3001) - 1500;
        const bool v3 = (next() & 1) != 0;
        std::string command = "a=" + std::to_string(v1) + " -b 2.5 ";
        command += v3 ? "--c=true" : "--no-c";
        command += " nums=" + std::to_string(v1) + "," + std::to_string(v2);
        const auto d = numbers.delimiter(',');
        auto typed = a::parse(command, one, two, three, d);
        int x = -9;
        double y = -9;
        bool z = false;
        std::vector<int> list{-9};
        auto direct =
            a::parseInto(command, a::bind(one, x), a::bind(two, y), a::bind(three, z), a::bind(d, list));
        REQUIRE(typed && !direct);
        REQUIRE(x == typed.get<"a">().value());
        REQUIRE(y == typed.get<"b">().value());
        REQUIRE(z == typed.get<"c">().value());
        REQUIRE(list == typed.get<"nums">().value());
    }

    // Empty/malformed inputs must have identical failure classes, since both
    // modes share the single scanner rather than two copied grammar branches.
    for (std::string_view bad :
         {"--a=x", "a=12junk", "--a", "--no-a", "--unknown=3", "-z", "--nums", "--a=3 --b=bad"})
    {
        auto typed = a::parse(bad, one, two, three, numbers);
        int x{};
        double y{};
        bool z{};
        std::vector<int> list;
        auto direct =
            a::parseInto(bad, a::bind(one, x), a::bind(two, y), a::bind(three, z), a::bind(numbers, list));
        REQUIRE(typed.error().code == direct.code);
    }

    // Same descriptors can be parsed from different threads with independent
    // results and workspaces. No global mutable index or hidden static scratch.
    std::array<std::thread, 4> threads;
    for (auto& t : threads)
    {
        t = std::thread([&] {
            for (int i = 0; i < 6000; ++i)
            {
                auto typed = a::parse("a=42 b=1.25 c=true", one, two, three);
                REQUIRE(typed && typed.get<"a">().value() == 42);
                REQUIRE(typed.get<"b">().value() == 1.25);
            }
        });
    }
    for (auto& t : threads)
        t.join();
    std::puts("equivalence/property/concurrency PASS");
}
