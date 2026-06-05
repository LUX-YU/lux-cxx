#include <lux/cxx/container/BasicSparseSet.hpp>
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

using namespace lux::cxx;

#define TEST_ASSERT(cond) \
    if (!(cond)) { \
        std::cerr << "FAIL: " << #cond << " at line " << __LINE__ << std::endl; \
        assert(cond); \
    }

// -- tag types for SlotKey discrimination ------------------------------------
struct TagA {};
struct TagB {};

using KeyA = SlotKey<TagA>;
using KeyB = SlotKey<TagB>;

// ============================================================================
//  BasicSparseSet<SlotKey> tests
// ============================================================================

void test_slotkey_insert_and_contains()
{
    SlotKeySparseSet<KeyA, std::string> set;

    KeyA k1{0, 1};
    KeyA k2{1, 1};
    KeyA k3{2, 1};

    set.insert(k1, "hello");
    set.insert(k2, "world");

    TEST_ASSERT(set.size() == 2);
    TEST_ASSERT(set.contains(k1));
    TEST_ASSERT(set.contains(k2));
    TEST_ASSERT(!set.contains(k3));
}

void test_slotkey_stale_handle_rejected()
{
    SlotKeySparseSet<KeyA, int> set;

    KeyA old_key{5, 1};
    KeyA new_key{5, 2};  // same index, different generation

    set.insert(old_key, 42);

    TEST_ASSERT(set.contains(old_key));
    TEST_ASSERT(!set.contains(new_key));  // generation mismatch

    // tryGet must also respect generation
    TEST_ASSERT(set.tryGet(old_key) != nullptr);
    TEST_ASSERT(set.tryGet(new_key) == nullptr);
}

void test_slotkey_erase_and_stale()
{
    SlotKeySparseSet<KeyA, int> set;

    KeyA k1{0, 1};
    KeyA k2{1, 1};
    KeyA k3{2, 1};

    set.insert(k1, 10);
    set.insert(k2, 20);
    set.insert(k3, 30);

    TEST_ASSERT(set.erase(k2));
    TEST_ASSERT(!set.contains(k2));
    TEST_ASSERT(set.size() == 2);

    // After erase, re-inserting with bumped generation should work
    KeyA k2_new{1, 2};
    set.insert(k2_new, 200);
    TEST_ASSERT(set.contains(k2_new));
    TEST_ASSERT(!set.contains(k2));  // old generation still rejected
    TEST_ASSERT(*set.tryGet(k2_new) == 200);
}

void test_slotkey_null_key_rejected()
{
    SlotKeySparseSet<KeyA, int> set;

    KeyA null_key = KeyA::invalid();
    TEST_ASSERT(!set.contains(null_key));

    // insert with null key should not crash (is_addressable returns false for null)
    // The traits say null is not addressable, but insert calls ensure_sparse
    // which uses sparse_index. We should NOT insert null keys. Test contains only.
    TEST_ASSERT(set.tryGet(null_key) == nullptr);
}

void test_slotkey_at_and_operator()
{
    SlotKeySparseSet<KeyA, std::string> set;

    KeyA k1{0, 1};
    set.insert(k1, "foo");

    TEST_ASSERT(set.at(k1) == "foo");

    set[k1] = "bar";  // operator[] on existing key
    TEST_ASSERT(set.at(k1) == "bar");

    KeyA k2{3, 1};
    set[k2] = "baz";  // operator[] creates new entry
    TEST_ASSERT(set.contains(k2));
    TEST_ASSERT(set.at(k2) == "baz");
}

void test_slotkey_emplace()
{
    SlotKeySparseSet<KeyA, std::string> set;

    KeyA k1{10, 3};
    auto& ref = set.emplace(k1, "constructed");
    TEST_ASSERT(ref == "constructed");
    TEST_ASSERT(set.contains(k1));
    TEST_ASSERT(set.at(k1) == "constructed");
}

void test_slotkey_extract()
{
    SlotKeySparseSet<KeyA, std::string> set;

    KeyA k1{0, 1};
    set.insert(k1, "extractme");

    std::string out;
    TEST_ASSERT(set.extract(k1, out));
    TEST_ASSERT(out == "extractme");
    TEST_ASSERT(!set.contains(k1));
    TEST_ASSERT(set.empty());
}

void test_slotkey_dense_iteration()
{
    SlotKeySparseSet<KeyA, int> set;

    KeyA keys[] = {{0,1}, {3,1}, {7,2}, {1,5}};
    int vals[]  = {10, 30, 70, 15};

    for (int i = 0; i < 4; ++i)
        set.insert(keys[i], vals[i]);

    TEST_ASSERT(set.size() == 4);
    TEST_ASSERT(set.keys().size() == 4);
    TEST_ASSERT(set.values().size() == 4);

    // Check all values are present
    int sum = 0;
    for (auto& v : set.values())
        sum += v;
    TEST_ASSERT(sum == 10 + 30 + 70 + 15);
}

void test_slotkey_clear()
{
    SlotKeySparseSet<KeyA, int> set;

    for (uint32_t i = 0; i < 100; ++i)
        set.insert(KeyA{i, 1}, static_cast<int>(i));

    TEST_ASSERT(set.size() == 100);
    set.clear();
    TEST_ASSERT(set.empty());
    TEST_ASSERT(set.size() == 0);
}

void test_slotkey_tag_discrimination()
{
    // KeyA and KeyB are different types — they should not mix
    SlotKeySparseSet<KeyA, int> setA;
    SlotKeySparseSet<KeyB, int> setB;

    KeyA ka{0, 1};
    KeyB kb{0, 1};

    setA.insert(ka, 1);
    setB.insert(kb, 2);

    TEST_ASSERT(setA.at(ka) == 1);
    TEST_ASSERT(setB.at(kb) == 2);
    // These are completely independent containers — compile-time safety.
}

// ============================================================================
//  GenericAutoSparseSet<SlotKey> tests
// ============================================================================

void test_auto_slotkey_insert()
{
    SlotKeyAutoSparseSet<KeyA, std::string> set;

    auto k1 = set.insert("alpha");
    auto k2 = set.insert("beta");
    auto k3 = set.insert("gamma");

    TEST_ASSERT(set.size() == 3);
    TEST_ASSERT(set.contains(k1));
    TEST_ASSERT(set.contains(k2));
    TEST_ASSERT(set.contains(k3));
    TEST_ASSERT(set.at(k1) == "alpha");
    TEST_ASSERT(set.at(k2) == "beta");
    TEST_ASSERT(set.at(k3) == "gamma");

    // Keys should have sequential indices
    TEST_ASSERT(k1.index == 0);
    TEST_ASSERT(k2.index == 1);
    TEST_ASSERT(k3.index == 2);
    // Initial generation should be 1
    TEST_ASSERT(k1.gen == 1);
}

void test_auto_slotkey_erase_and_reuse()
{
    SlotKeyAutoSparseSet<KeyA, int> set;

    auto k1 = set.insert(10);
    auto k2 = set.insert(20);
    auto k3 = set.insert(30);

    TEST_ASSERT(set.erase(k2));
    TEST_ASSERT(!set.contains(k2));
    TEST_ASSERT(set.size() == 2);
    TEST_ASSERT(set.free_ids_count() == 1);

    // Insert again — should reuse index 1 but with bumped generation
    auto k4 = set.insert(40);
    TEST_ASSERT(k4.index == k2.index);       // same slot index
    TEST_ASSERT(k4.gen == k2.gen + 1);       // generation bumped
    TEST_ASSERT(!set.contains(k2));           // old handle still invalid
    TEST_ASSERT(set.contains(k4));
    TEST_ASSERT(set.at(k4) == 40);
}

void test_auto_slotkey_emplace()
{
    SlotKeyAutoSparseSet<KeyA, std::string> set;

    auto k = set.emplace("emplaced");
    TEST_ASSERT(set.at(k) == "emplaced");
}

void test_auto_slotkey_clear()
{
    SlotKeyAutoSparseSet<KeyA, int> set;

    for (int i = 0; i < 50; ++i)
        set.insert(i);

    TEST_ASSERT(set.size() == 50);
    set.clear();
    TEST_ASSERT(set.empty());
    TEST_ASSERT(set.free_ids_count() == 0);
}

void test_auto_slotkey_extract()
{
    SlotKeyAutoSparseSet<KeyA, std::string> set;

    auto k = set.insert("value");
    std::string out;
    TEST_ASSERT(set.extract(k, out));
    TEST_ASSERT(out == "value");
    TEST_ASSERT(!set.contains(k));
    TEST_ASSERT(set.free_ids_count() == 1);
}

// ============================================================================
//  BasicSparseSet<int> compatibility tests (integer keys)
// ============================================================================

void test_integer_basic()
{
    BasicSparseSet<int, std::string> set;

    set.insert(0, "zero");
    set.insert(5, "five");
    set.insert(10, "ten");

    TEST_ASSERT(set.size() == 3);
    TEST_ASSERT(set.contains(0));
    TEST_ASSERT(set.contains(5));
    TEST_ASSERT(set.contains(10));
    TEST_ASSERT(!set.contains(3));
    TEST_ASSERT(set.at(5) == "five");
}

void test_integer_erase()
{
    BasicSparseSet<size_t, int> set;

    set.insert(0, 100);
    set.insert(1, 200);
    set.insert(2, 300);

    TEST_ASSERT(set.erase(1));
    TEST_ASSERT(!set.contains(1));
    TEST_ASSERT(set.size() == 2);
    TEST_ASSERT(set.contains(0));
    TEST_ASSERT(set.contains(2));
}

// ============================================================================

int main()
{
    // SlotKey BasicSparseSet tests
    test_slotkey_insert_and_contains();
    test_slotkey_stale_handle_rejected();
    test_slotkey_erase_and_stale();
    test_slotkey_null_key_rejected();
    test_slotkey_at_and_operator();
    test_slotkey_emplace();
    test_slotkey_extract();
    test_slotkey_dense_iteration();
    test_slotkey_clear();
    test_slotkey_tag_discrimination();

    // SlotKey AutoSparseSet tests
    test_auto_slotkey_insert();
    test_auto_slotkey_erase_and_reuse();
    test_auto_slotkey_emplace();
    test_auto_slotkey_clear();
    test_auto_slotkey_extract();

    // Integer BasicSparseSet tests
    test_integer_basic();
    test_integer_erase();

    std::cout << "All basic_sparse_set tests passed." << std::endl;
    return 0;
}
