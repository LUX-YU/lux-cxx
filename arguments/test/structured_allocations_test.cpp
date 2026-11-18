#include <lux/cxx/arguments/StructuredValue.hpp>
#include <lux/cxx/arguments/TypedOptions.hpp>
#include <lux/cxx/arguments/DirectBinding.hpp>
#include "allocation_counter.hpp"
#include <cstdio>
#include <memory_resource>
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
    constexpr a::Option<std::array<int, 3>, "position"> position;
    allocation_test::start();
    for (int i = 0; i < 10000; ++i)
    {
        auto r = a::parse("position=[1, 2, 3]", position);
        CHECK(r && r.get<"position">()->at(2) == 3);
    }
    CHECK(allocation_test::stop() == 0);
    a::Option<std::vector<std::vector<int>>, "matrix"> matrix;
    a::Option<std::vector<std::string>, "names"> names;
    std::vector<std::vector<int>> m(2);
    for (auto& row : m)
        row.reserve(8);
    std::vector<std::string> n(2);
    for (auto& name : n)
        name.reserve(128);
    const auto bm = a::bind(matrix, m);
    const auto bn = a::bind(names, n);
    // A mixed schema uses the structured source; scalar-only schemas don't.
    a::Workspace workspace;
    constexpr auto input =
        R"(matrix=[[1,2,3],[4,5,6]] names=["first long name longer than small-string capacity","second long name longer than small-string capacity"] )";
    const auto* row_pointer = m[0].data();
    const auto* string_pointer = n[0].data();
    allocation_test::start();
    for (int i = 0; i < 10000; ++i)
    {
        a::StructuredTextSource source(input);
        CHECK(!a::parseIntoFrom(source, workspace, bm, bn));
        CHECK(m[0][2] == 3 && m[1][2] == 6 && n.size() == 2);
    }
    const auto allocs = allocation_test::stop();
    CHECK(allocs == 0 && m[0].data() == row_pointer && n[0].data() == string_pointer);

    alignas(std::max_align_t) std::byte memory[32768];
    std::pmr::monotonic_buffer_resource arena(memory, sizeof(memory), std::pmr::null_memory_resource());
    std::pmr::vector<std::pmr::string> strings(&arena);
    std::pmr::map<std::pmr::string, std::pmr::vector<int>> table(&arena);
    // Catch accidental allocation from the default resource, including temporaries.
    auto* previous = std::pmr::set_default_resource(std::pmr::null_memory_resource());
    allocation_test::start();
    CHECK(!a::parseStructured(R"(["a long string which is deliberately outside SSO"])", strings));
    CHECK(!a::parseStructured(R"({"a long key which is deliberately outside SSO":[1,2,3]})", table));
    const auto pmr_allocations = allocation_test::stop();
    std::pmr::set_default_resource(previous);
    CHECK(pmr_allocations == 0);
    CHECK(strings[0].get_allocator().resource() == &arena);
    CHECK(table.begin()->first.get_allocator().resource() == &arena);
    CHECK(table.begin()->second.get_allocator().resource() == &arena);
    std::puts("structured allocations PASS: 10000 fixed-array results; 10000 reused nested destinations; "
              "PMR isolation");
}
