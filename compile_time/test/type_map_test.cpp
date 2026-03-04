#include <lux/cxx/compile_time/type_map.hpp>
#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <type_traits>

using namespace lux::cxx;

#define TEST_ASSERT(cond) \
    if (!(cond)) { \
        std::cerr << "FAIL: " << #cond << " at line " << __LINE__ << std::endl; \
        assert(cond); \
    }

// ---- tag types used as keys -------------------------------------------------

struct PhysicsTag {};
struct RenderTag  {};
struct AudioTag   {};
struct EmptyTag   {};

// ---- compile-time tests -----------------------------------------------------

// Basic contains check
using TestMap = TypeMap<
    entry<PhysicsTag, int>,
    entry<RenderTag,  double>,
    entry<AudioTag,   float>
>;

static_assert(TestMap::entry_count == 3);
static_assert(TestMap::contains<PhysicsTag>());
static_assert(TestMap::contains<RenderTag>());
static_assert(TestMap::contains<AudioTag>());
static_assert(!TestMap::contains<EmptyTag>());

// constexpr usage
constexpr auto make_map()
{
    TypeMap<entry<int, int>, entry<float, int>> m;
    m.get<int>() = 42;
    m.get<float>() = 99;
    return m;
}

constexpr auto cm = make_map();
static_assert(cm.get<int>() == 42);
static_assert(cm.get<float>() == 99);

// ---- runtime tests ----------------------------------------------------------

void test_basic_get_set()
{
    TypeMap<
        entry<PhysicsTag, std::string>,
        entry<RenderTag,  double>,
        entry<AudioTag,   int>
    > map;

    map.get<PhysicsTag>() = "gravity";
    map.get<RenderTag>() = 3.14;
    map.get<AudioTag>() = 44100;

    TEST_ASSERT(map.get<PhysicsTag>() == "gravity");
    TEST_ASSERT(map.get<RenderTag>() == 3.14);
    TEST_ASSERT(map.get<AudioTag>() == 44100);

    std::cout << "  basic get/set tests passed" << std::endl;
}

void test_const_access()
{
    TypeMap<
        entry<int, std::string>,
        entry<float, double>
    > map;
    map.get<int>() = "hello";
    map.get<float>() = 2.71;

    const auto& cmap = map;
    TEST_ASSERT(cmap.get<int>() == "hello");
    TEST_ASSERT(cmap.get<float>() == 2.71);

    std::cout << "  const access tests passed" << std::endl;
}

void test_move_semantics()
{
    TypeMap<entry<int, std::vector<int>>> map;
    map.get<int>() = {1, 2, 3, 4, 5};
    TEST_ASSERT(map.get<int>().size() == 5);
    TEST_ASSERT(map.get<int>()[2] == 3);

    std::cout << "  move semantics tests passed" << std::endl;
}

void test_for_each()
{
    TypeMap<
        entry<int,   int>,
        entry<float, int>,
        entry<char,  int>
    > map;
    map.get<int>()   = 10;
    map.get<float>() = 20;
    map.get<char>()  = 30;

    int sum = 0;
    map.for_each([&sum]<typename Key, typename Value, std::size_t I>(Value& v) {
        sum += v;
    });
    TEST_ASSERT(sum == 60);

    // Const for_each
    const auto& cmap = map;
    int csum = 0;
    cmap.for_each([&csum]<typename Key, typename Value, std::size_t I>(const Value& v) {
        csum += v;
    });
    TEST_ASSERT(csum == 60);

    std::cout << "  for_each tests passed" << std::endl;
}

void test_for_each_type_dispatch()
{
    TypeMap<
        entry<int,    std::string>,
        entry<float,  int>,
        entry<double, double>
    > map;
    map.get<int>() = "type_int";
    map.get<float>() = 42;
    map.get<double>() = 1.5;

    // Verify type dispatch works — each callback gets the correct Key type
    bool found_int = false, found_float = false, found_double = false;
    map.for_each([&]<typename Key, typename Value, std::size_t I>(Value& v) {
        if constexpr (std::is_same_v<Key, int>)    { found_int = true;    TEST_ASSERT(v == "type_int"); }
        if constexpr (std::is_same_v<Key, float>)  { found_float = true;  TEST_ASSERT(v == 42); }
        if constexpr (std::is_same_v<Key, double>) { found_double = true; TEST_ASSERT(v == 1.5); }
    });
    TEST_ASSERT(found_int && found_float && found_double);

    std::cout << "  for_each type dispatch tests passed" << std::endl;
}

void test_get_entry()
{
    TypeMap<
        entry<int,    std::string>,
        entry<float,  double>
    > map;
    map.get<int>() = "entry_test";

    auto& e = map.get_entry<0>();
    static_assert(std::is_same_v<typename std::remove_reference_t<decltype(e)>::key_type, int>);
    TEST_ASSERT(e.value == "entry_test");

    std::cout << "  get_entry tests passed" << std::endl;
}

void test_construct_with_values()
{
    auto map = TypeMap(
        entry<int, int>(entry<int, int>(42)),
        entry<float, double>(entry<float, double>(3.14))
    );
    TEST_ASSERT(map.get<int>() == 42);
    TEST_ASSERT(map.get<float>() == 3.14);

    std::cout << "  construct with values tests passed" << std::endl;
}

int main()
{
    std::cout << "type_map tests:" << std::endl;
    test_basic_get_set();
    test_const_access();
    test_move_semantics();
    test_for_each();
    test_for_each_type_dispatch();
    test_get_entry();
    test_construct_with_values();
    std::cout << "All type_map tests passed!" << std::endl;
    return 0;
}
