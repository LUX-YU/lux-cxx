#include <lux/cxx/container/BasicSparseSet.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>

namespace
{
    bool count_allocations{};
    std::size_t allocations{};
    int failures{};

    void check(bool condition, const char* contract)
    {
        std::printf("%s: %s\n", condition ? "PASS" : "FAIL", contract);
        failures += !condition;
    }

    struct IdentityTag;
    using Key = lux::cxx::SlotKey<IdentityTag, std::uint8_t, std::uint8_t>;
    using Set = lux::cxx::SlotKeyAutoSparseSet<Key, int>;
}

void* operator new(std::size_t size)
{
    allocations += count_allocations;
    if (auto* memory = std::malloc(size == 0 ? 1 : size))
    {
        return memory;
    }
    std::abort();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

int main()
{
    Set cleared;
    const auto old = cleared.insert(7);
    cleared.clear();
    const auto replacement = cleared.insert(8);
    check(old != replacement && !cleared.contains(old), "clear never reissues a live key");

    Set recycled;
    const auto first = recycled.insert(1);
    auto current = first;
    for (unsigned generation = 1; generation <= 255; ++generation)
    {
        recycled.erase(current);
        current = recycled.insert(2);
    }
    check(current.index != first.index && !recycled.contains(first), "exhausted generation retires the slot");

    Set full;
    std::array<Key, 255> keys{};
    for (auto& key : keys)
    {
        key = full.insert(3);
    }
    const auto rejected = full.insert(4);
    check(rejected.isNull() && full.size() == keys.size(), "fresh index exhaustion rejects without mutation");
    bool originals_intact = true;
    for (auto key : keys)
    {
        originals_intact &= full.contains(key) && *full.tryGet(key) == 3;
    }
    check(originals_intact, "exhaustion preserves every live value");

    const auto exhausted_next = full.next_id();
    check(!full.prepareInsert(1) && full.next_id() == exhausted_next && full.free_ids_count() == 0,
        "failed batch preparation neither issues nor recycles identities");
    full.erase(keys[31]);
    check(!full.prepareInsert(2) && full.free_ids_count() == 1,
        "a partially available batch rejects without consuming its free slot");
    check(full.prepareInsert(1), "an exhausted index space can still reuse a safe generation");
    const auto reused = full.insert(9);
    check(reused.index == keys[31].index && reused.gen != keys[31].gen && !full.contains(keys[31]),
        "recycled identity remains distinct when no fresh indices remain");

    Set retired;
    bool clear_generations_match = true;
    for (unsigned generation = 1; generation <= 255; ++generation)
    {
        const auto key = retired.insert(11);
        clear_generations_match &= key.index == 0 && key.gen == generation;
        retired.clear();
    }
    check(clear_generations_match, "clear preserves issued generation");
    const auto after_exhausted_clear = retired.insert(12);
    check(after_exhausted_clear.index == 1 && after_exhausted_clear.gen == 1,
        "clear does not resurrect an exhausted slot");

    lux::cxx::SlotKeyAutoSparseSet<Key, std::unique_ptr<int>> prepared;
    prepared.reserve(16);
    std::array<std::unique_ptr<int>, 16> owners;
    for (auto& owner : owners)
    {
        owner = std::make_unique<int>(5);
    }
    std::array<Key, 16> prepared_keys{};
    allocations = 0;
    count_allocations = true;
    for (std::size_t i = 0; i < owners.size(); ++i)
    {
        prepared_keys[i] = prepared.insert(std::move(owners[i]));
    }
    for (auto key : prepared_keys)
    {
        std::unique_ptr<int> extracted;
        prepared.extract(key, extracted);
    }
    count_allocations = false;
    check(allocations == 0, "prepared unique-owner insertion and extraction do not allocate");

    const auto before_prepare = prepared.next_id();
    check(prepared.prepareInsert(16) && before_prepare == prepared.next_id(), "preparation does not issue keys");
    for (auto& owner : owners)
    {
        owner = std::make_unique<int>(6);
    }
    allocations = 0;
    count_allocations = true;
    for (std::size_t i = 0; i < owners.size(); ++i)
    {
        const auto key = prepared.insert(std::move(owners[i]));
        if (key == prepared_keys[i])
        {
            std::abort();
        }
    }
    prepared.clear();
    count_allocations = false;
    check(allocations == 0, "prepared recycled insertion and clear do not allocate");

    lux::cxx::SlotKeyAutoSparseSet<Key, std::unique_ptr<int>> owner_full;
    for (unsigned i = 0; i < 255; ++i)
    {
        owner_full.insert(std::make_unique<int>(7));
    }
    auto candidate = std::make_unique<int>(8);
    const auto rejected_owner = owner_full.insert(std::move(candidate));
    check(rejected_owner.isNull() && candidate && *candidate == 8 && owner_full.size() == 255,
        "exhaustion leaves a unique owner with its caller");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
