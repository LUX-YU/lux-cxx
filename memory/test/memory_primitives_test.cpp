#include <lux/cxx/memory/PmrResources.hpp>
#include <lux/cxx/memory/SharedBytes.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <memory_resource>
#include <new>
#include <vector>

int main()
{
    const std::array source{
        std::byte{1},
        std::byte{2},
        std::byte{3},
        std::byte{4},
    };
    auto shared = lux::cxx::SharedBytes<>::copyOf(source);
    assert(shared.size() == source.size());
    assert(shared.view()[2] == std::byte{3});

    auto slice = shared.subspan(1, 2);
    assert(slice.size() == 2);
    assert(slice.view()[0] == std::byte{2});
    assert(slice.view()[1] == std::byte{3});
    assert(shared.use_count() == 2);
    assert(shared.subspan(99).empty());

    lux::cxx::CountingMemoryResource counting;
    {
        std::pmr::vector<int> values(&counting);
        values.resize(128);
        assert(counting.currentBytes() >= values.size() * sizeof(int));
        assert(counting.peakBytes() >= counting.currentBytes());
        assert(counting.allocationCount() > 0);
    }
    assert(counting.currentBytes() == 0);
    assert(counting.deallocationCount() == counting.allocationCount());

    lux::cxx::BudgetMemoryResource budget(128);
    void* first = budget.allocate(64, alignof(std::max_align_t));
    assert(budget.currentBytes() == 64);
    bool budget_failed = false;
    try
    {
        static_cast<void>(budget.allocate(80, alignof(std::max_align_t)));
    }
    catch (const std::bad_alloc&)
    {
        budget_failed = true;
    }
    assert(budget_failed);
    budget.deallocate(first, 64, alignof(std::max_align_t));
    assert(budget.currentBytes() == 0);

    lux::cxx::FailingMemoryResource failing(1);
    void* allowed = failing.allocate(8, alignof(std::max_align_t));
    bool injected_failure = false;
    try
    {
        static_cast<void>(failing.allocate(8, alignof(std::max_align_t)));
    }
    catch (const std::bad_alloc&)
    {
        injected_failure = true;
    }
    assert(injected_failure);
    failing.deallocate(allowed, 8, alignof(std::max_align_t));
    return 0;
}
