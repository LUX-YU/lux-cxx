#include <lux/cxx/container/SlotMap.hpp>
#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>

using namespace lux::cxx;

#define TEST_ASSERT(cond) \
    if (!(cond)) { \
        std::cerr << "FAIL: " << #cond << " at line " << __LINE__ << std::endl; \
        assert(cond); \
    }

// ---- basic tests ------------------------------------------------------------

void test_insert_and_lookup()
{
    SlotMap<std::string> map;

    auto k1 = map.insert("hello");
    auto k2 = map.insert("world");
    auto k3 = map.insert("foo");

    TEST_ASSERT(map.size() == 3);
    TEST_ASSERT(!map.empty());
    TEST_ASSERT(map.is_valid(k1));
    TEST_ASSERT(map.is_valid(k2));
    TEST_ASSERT(map.is_valid(k3));

    TEST_ASSERT(map.at(k1) == "hello");
    TEST_ASSERT(map.at(k2) == "world");
    TEST_ASSERT(map.at(k3) == "foo");

    TEST_ASSERT(map[k1] == "hello");
    TEST_ASSERT(map[k2] == "world");

    std::cout << "  insert and lookup tests passed" << std::endl;
}

void test_find()
{
    SlotMap<int> map;
    auto k1 = map.insert(42);

    auto* ptr = map.find(k1);
    TEST_ASSERT(ptr != nullptr);
    TEST_ASSERT(*ptr == 42);

    // Null key
    SlotMap<int>::key_type null_key;
    TEST_ASSERT(null_key.is_null());
    TEST_ASSERT(map.find(null_key) == nullptr);

    std::cout << "  find tests passed" << std::endl;
}

void test_emplace()
{
    SlotMap<std::string> map;
    auto k = map.emplace(5, 'x');  // std::string(5, 'x') = "xxxxx"
    TEST_ASSERT(map.size() == 1);
    TEST_ASSERT(map.at(k) == "xxxxx");

    std::cout << "  emplace tests passed" << std::endl;
}

void test_erase_basic()
{
    SlotMap<int> map;
    auto k1 = map.insert(10);
    auto k2 = map.insert(20);
    auto k3 = map.insert(30);

    TEST_ASSERT(map.size() == 3);

    bool erased = map.erase(k2);
    TEST_ASSERT(erased);
    TEST_ASSERT(map.size() == 2);
    TEST_ASSERT(!map.is_valid(k2));
    TEST_ASSERT(map.find(k2) == nullptr);

    // k1 and k3 should still be valid
    TEST_ASSERT(map.is_valid(k1));
    TEST_ASSERT(map.is_valid(k3));
    TEST_ASSERT(map.at(k1) == 10);
    TEST_ASSERT(map.at(k3) == 30);

    // Double-erase should return false
    TEST_ASSERT(!map.erase(k2));

    std::cout << "  erase basic tests passed" << std::endl;
}

void test_generation_invalidation()
{
    SlotMap<std::string> map;

    auto k1 = map.insert("first");
    map.erase(k1);

    // k1 is now stale
    TEST_ASSERT(!map.is_valid(k1));
    TEST_ASSERT(map.find(k1) == nullptr);

    // Insert into the recycled slot — generation should differ
    auto k2 = map.insert("second");
    TEST_ASSERT(map.is_valid(k2));
    TEST_ASSERT(map.at(k2) == "second");

    // k1 still invalid even though slot index might be the same
    TEST_ASSERT(!map.is_valid(k1));
    TEST_ASSERT(k1 != k2);

    // Same slot index, different generation
    TEST_ASSERT(k1.index == k2.index);
    TEST_ASSERT(k1.generation != k2.generation);

    std::cout << "  generation invalidation tests passed" << std::endl;
}

void test_erase_all_and_reinsert()
{
    SlotMap<int> map;
    std::vector<SlotMap<int>::key_type> keys;

    for (int i = 0; i < 100; i++)
        keys.push_back(map.insert(i));

    TEST_ASSERT(map.size() == 100);

    // Erase all
    for (auto& k : keys)
        TEST_ASSERT(map.erase(k));

    TEST_ASSERT(map.size() == 0);
    TEST_ASSERT(map.empty());

    // All keys should be invalid
    for (auto& k : keys)
        TEST_ASSERT(!map.is_valid(k));

    // Reinsert — should reuse slots
    std::vector<SlotMap<int>::key_type> new_keys;
    for (int i = 0; i < 50; i++)
        new_keys.push_back(map.insert(i * 10));

    TEST_ASSERT(map.size() == 50);

    // Old keys still invalid
    for (auto& k : keys)
        TEST_ASSERT(!map.is_valid(k));

    // New keys valid
    for (size_t i = 0; i < new_keys.size(); i++)
    {
        TEST_ASSERT(map.is_valid(new_keys[i]));
        TEST_ASSERT(map.at(new_keys[i]) == static_cast<int>(i * 10));
    }

    std::cout << "  erase all and reinsert tests passed" << std::endl;
}

void test_dense_iteration()
{
    SlotMap<int> map;
    map.insert(10);
    map.insert(20);
    map.insert(30);

    int sum = 0;
    for (auto& v : map)
        sum += v;
    TEST_ASSERT(sum == 60);

    // const iteration
    const auto& cmap = map;
    int csum = 0;
    for (const auto& v : cmap)
        csum += v;
    TEST_ASSERT(csum == 60);

    // values() access
    TEST_ASSERT(map.values().size() == 3);

    std::cout << "  dense iteration tests passed" << std::endl;
}

void test_at_throws()
{
    SlotMap<int> map;
    auto k = map.insert(42);
    map.erase(k);

    bool threw = false;
    try {
        (void)map.at(k);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_ASSERT(threw);

    std::cout << "  at() throws tests passed" << std::endl;
}

void test_clear()
{
    SlotMap<int> map;
    auto k1 = map.insert(1);
    auto k2 = map.insert(2);

    map.clear();
    TEST_ASSERT(map.size() == 0);
    TEST_ASSERT(map.empty());
    TEST_ASSERT(!map.is_valid(k1));
    TEST_ASSERT(!map.is_valid(k2));

    // Can still insert after clear
    auto k3 = map.insert(3);
    TEST_ASSERT(map.size() == 1);
    TEST_ASSERT(map.at(k3) == 3);

    std::cout << "  clear tests passed" << std::endl;
}

void test_reserve()
{
    SlotMap<int> map(1000);
    // Should not crash, just pre-allocates
    for (int i = 0; i < 1000; i++)
        map.insert(i);
    TEST_ASSERT(map.size() == 1000);

    std::cout << "  reserve tests passed" << std::endl;
}

void test_move_only_values()
{
    SlotMap<std::unique_ptr<int>> map;
    auto k1 = map.insert(std::make_unique<int>(42));
    auto k2 = map.emplace(std::unique_ptr<int>(new int(99)));

    TEST_ASSERT(*map.at(k1) == 42);
    TEST_ASSERT(*map.at(k2) == 99);

    map.erase(k1);
    TEST_ASSERT(map.size() == 1);
    TEST_ASSERT(*map.at(k2) == 99);

    std::cout << "  move-only values tests passed" << std::endl;
}

void test_interleaved_insert_erase()
{
    SlotMap<int> map;

    // Build up
    auto k0 = map.insert(0);
    auto k1 = map.insert(1);
    auto k2 = map.insert(2);
    auto k3 = map.insert(3);

    // Erase middle
    map.erase(k1);
    map.erase(k2);
    TEST_ASSERT(map.size() == 2);

    // Insert new — should reuse slots
    auto k4 = map.insert(4);
    auto k5 = map.insert(5);
    TEST_ASSERT(map.size() == 4);

    // All stale handles invalid
    TEST_ASSERT(!map.is_valid(k1));
    TEST_ASSERT(!map.is_valid(k2));

    // Valid handles correct
    TEST_ASSERT(map.at(k0) == 0);
    TEST_ASSERT(map.at(k3) == 3);
    TEST_ASSERT(map.at(k4) == 4);
    TEST_ASSERT(map.at(k5) == 5);

    // Sum via iteration
    int sum = 0;
    for (auto& v : map) sum += v;
    TEST_ASSERT(sum == 0 + 3 + 4 + 5);

    std::cout << "  interleaved insert/erase tests passed" << std::endl;
}

int main()
{
    std::cout << "slot_map tests:" << std::endl;
    test_insert_and_lookup();
    test_find();
    test_emplace();
    test_erase_basic();
    test_generation_invalidation();
    test_erase_all_and_reinsert();
    test_dense_iteration();
    test_at_throws();
    test_clear();
    test_reserve();
    test_move_only_values();
    test_interleaved_insert_erase();
    std::cout << "All slot_map tests passed!" << std::endl;
    return 0;
}
