#include <lux/cxx/container/StableSlotMap.hpp>

#include <cassert>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace
{
    struct AllocationState final
    {
        std::size_t allocations = 0;
        std::size_t fail_after = (std::numeric_limits<std::size_t>::max)();
    };

    template<typename Type>
    class CountingAllocator final
    {
      public:
        using value_type = Type;

        template<typename Other>
        friend class CountingAllocator;

        CountingAllocator()
            : state_(std::make_shared<AllocationState>())
        {
        }

        template<typename Other>
        CountingAllocator(const CountingAllocator<Other>& other) noexcept
            : state_(other.state_)
        {
        }

        [[nodiscard]] Type* allocate(std::size_t count)
        {
            if (state_->allocations >= state_->fail_after)
            {
                throw std::bad_alloc{};
            }
            ++state_->allocations;
            return std::allocator<Type>{}.allocate(count);
        }

        void deallocate(Type* pointer, std::size_t count) noexcept
        {
            std::allocator<Type>{}.deallocate(pointer, count);
        }

        [[nodiscard]] std::size_t allocations() const noexcept
        {
            return state_->allocations;
        }

        void failAfter(std::size_t successful_allocations) noexcept
        {
            state_->fail_after = successful_allocations;
        }

        template<typename Other>
        [[nodiscard]] bool operator==(
            const CountingAllocator<Other>& other
        ) const noexcept
        {
            return state_ == other.state_;
        }

      private:
        std::shared_ptr<AllocationState> state_;
    };

    struct EntityTag;

    struct Aux final
    {
        std::uint32_t flags = 0;
    };

    struct alignas(64) OverAligned final
    {
        int value = 0;
    };
} // namespace

int main()
{
    using Map = lux::cxx::StableSlotMap<int, EntityTag, Aux, 4>;
    Map map;
    map.reserve(4);
    auto first = map.emplace(10);
    int* stable_pointer = map.find(first);
    assert(stable_pointer != nullptr);

    std::vector<Map::key_type> keys;
    for (int value = 0; value < 100; ++value)
    {
        keys.push_back(map.emplace(value));
    }
    assert(map.find(first) == stable_pointer);
    assert(*stable_pointer == 10);

    map.aux(first)->flags = 7;
    assert(map.aux(first)->flags == 7);

    const auto erased = keys[20];
    assert(map.erase(erased));
    assert(!map.isValid(erased));
    assert(map.find(first) == stable_pointer);

    std::int64_t sum = 0;
    for (const int value : map) sum += value;
    assert(sum == 10 + 4950 - 20);

    const auto stale = first;
    map.clear();
    assert(map.empty());
    assert(!map.isValid(stale));
    auto replacement = map.emplace(99);
    assert(map.isValid(replacement));
    assert(!map.isValid(stale));


    lux::cxx::StableSlotMap<std::unique_ptr<int>, void, lux::cxx::NoAux, 2>
        move_only;
    auto pointer_key = move_only.emplace(std::make_unique<int>(42));
    assert(**move_only.find(pointer_key) == 42);

    lux::cxx::StableSlotMap<OverAligned, void, lux::cxx::NoAux, 2> aligned;
    auto aligned_key = aligned.emplace(OverAligned{123});
    assert(reinterpret_cast<std::uintptr_t>(aligned.find(aligned_key)) % 64 == 0);


    Map moved(std::move(map));
    assert(moved.isValid(replacement));
    assert(*moved.find(replacement) == 99);


    CountingAllocator<int> allocator;
    using AllocatedMap = lux::cxx::StableSlotMap<
        int,
        void,
        lux::cxx::NoAux,
        64,
        CountingAllocator<int>
    >;
    AllocatedMap allocated(allocator);
    allocated.reserve(1024);
    for (int value = 0; value < 1024; ++value)
    {
        allocated.emplace(value);
    }
    assert(allocator.allocations() < 64);

    CountingAllocator<int> prepared_allocator;
    AllocatedMap prepared(prepared_allocator);
    assert(!prepared.tryEmplacePrepared(1));
    prepared.reserve(64U);
    const auto preparation_allocations = prepared_allocator.allocations();
    const auto prepared_bytes = prepared.storageBytes();
    prepared_allocator.failAfter(preparation_allocations);
    std::array<AllocatedMap::key_type, 64U> prepared_keys;
    for (std::size_t index{}; index < prepared_keys.size(); ++index)
    {
        auto inserted = prepared.tryEmplacePrepared(static_cast<int>(index));
        assert(inserted);
        prepared_keys[index] = *inserted;
    }
    auto* pinned = prepared.find(prepared_keys[63]);
    assert(!prepared.tryEmplacePrepared(65) && prepared.capacity() == 64U);
    assert(prepared.erase(prepared_keys[0]) && prepared.find(prepared_keys[63]) == pinned);
    assert(prepared.tryEmplacePrepared(66));
    assert(*pinned == 63 && prepared_allocator.allocations() == preparation_allocations);
    assert(prepared.storageBytes() == prepared_bytes && prepared_bytes > 64U * sizeof(int));

    using RetiringMap = lux::cxx::StableSlotMap<int, void, lux::cxx::NoAux, 1U,
        std::allocator<int>, std::uint8_t, std::uint8_t>;
    RetiringMap retiring;
    retiring.reserve(1U);
    for (unsigned generation{1U}; generation < 255U; ++generation)
    {
        const auto key = retiring.tryEmplacePrepared(1);
        assert(key && key->gen == generation);
        assert(retiring.erase(*key));
        assert(!retiring.isValid(*key));
    }
    assert(!retiring.tryEmplacePrepared(2) && retiring.capacity() == 1U);


    CountingAllocator<int> failing_allocator;
    AllocatedMap failing(failing_allocator);
    // MSVC's checked standard library may allocate iterator-proxy state while
    // constructing an empty vector, so fail the first operation allocation.
    failing_allocator.failAfter(failing_allocator.allocations());
    bool failed = false;
    try
    {
        failing.emplace(1);
    }
    catch (const std::bad_alloc&)
    {
        failed = true;
    }
    assert(failed);
    assert(failing.empty());
    assert(failing.capacity() == 0);
    return 0;
}
