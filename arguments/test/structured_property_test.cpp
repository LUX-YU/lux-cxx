#include <lux/cxx/arguments/StructuredValue.hpp>
#include <lux/cxx/arguments/TypedOptions.hpp>
#include <lux/cxx/arguments/DirectBinding.hpp>
#include <thread>
#include <cstdio>
#include <cstdlib>
namespace a = lux::cxx::args;
#define CHECK(...)                                                                                         \
    do                                                                                                     \
    {                                                                                                      \
        if (!(__VA_ARGS__))                                                                                \
        {                                                                                                  \
            std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #__VA_ARGS__);                                 \
            std::abort();                                                                                  \
        }                                                                                                  \
    } while (false)
int main()
{
    using Matrix = std::vector<std::vector<int>>;
    constexpr a::Option<Matrix, "m"> option;
    std::uint64_t state = 0x123456789abcdefULL;
    auto random = [&] {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    };
    // Expected values come from the generator, NOT from another parser invocation.
    for (int iteration = 0; iteration < 25000; ++iteration)
    {
        Matrix expected(random() % 7);
        std::string encoded = "[";
        for (std::size_t row = 0; row < expected.size(); ++row)
        {
            if (row)
                encoded += ", ";
            encoded += '[';
            expected[row].resize(random() % 9);
            for (std::size_t col = 0; col < expected[row].size(); ++col)
            {
                if (col)
                    encoded += ",";
                int value = static_cast<int>(random() % 200001) - 100000;
                expected[row][col] = value;
                encoded += std::to_string(value);
            }
            encoded += ']';
        }
        encoded += ']';
        Matrix direct;
        CHECK(!a::parseInto("m=" + encoded, a::bind(option, direct)));
        CHECK(direct == expected);
        const std::string token = "--m=" + encoded;
        const char* argv[] = {"app", token.c_str()};
        auto r = a::parse(2, argv, option);
        CHECK(r && *r.get<"m">() == expected);
        // Truncation can never be a successful complete structured value.
        encoded.pop_back();
        CHECK(a::parseStructured(encoded, direct));
    }
    std::array<std::thread, 4> workers;
    for (auto& worker : workers)
        worker = std::thread([&] {
            Matrix target(2, std::vector<int>(3));
            a::Workspace workspace;
            for (int i = 0; i < 5000; ++i)
            {
                a::StructuredTextSource source("m=[[1,2,3],[4,5,6]]");
                CHECK(!a::parseIntoFrom(source, workspace, a::bind(option, target)));
                CHECK(target[1][2] == 6);
            }
        });
    for (auto& worker : workers)
        worker.join();
    std::puts("structured property PASS: 25000 independent expected matrices; 20000 concurrent parses");
}
