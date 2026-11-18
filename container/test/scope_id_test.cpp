#include <lux/cxx/container/ScopeId.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <thread>
#include <unordered_set>
#include <vector>

namespace
{
    struct FirstTag;
    struct SecondTag;
}

int main()
{
    using FirstId = lux::cxx::ScopeId<FirstTag>;

    const FirstId empty;
    assert(empty.isNull());
    assert(!empty.isValid());

    lux::cxx::ScopeIdSource<FirstTag> first_source;
    lux::cxx::ScopeIdSource<SecondTag> second_source;
    const FirstId first = first_source.acquire();
    const FirstId second = first_source.acquire();
    assert(first.isValid());
    assert(second.isValid());
    assert(first != second);
    assert(second_source.acquire().isValid());

    constexpr std::size_t kThreadCount = 8U;
    constexpr std::size_t kIdsPerThread = 1024U;
    std::array<std::vector<FirstId>, kThreadCount> values;
    std::array<std::thread, kThreadCount> threads;
    for (std::size_t thread = 0U; thread < kThreadCount; ++thread)
    {
        threads[thread] = std::thread([&, thread]
        {
            values[thread].reserve(kIdsPerThread);
            for (std::size_t index = 0U; index < kIdsPerThread; ++index)
                values[thread].push_back(first_source.acquire());
        });
    }
    for (auto& thread : threads)
        thread.join();

    std::unordered_set<FirstId, FirstId::Hash> unique;
    for (const auto& thread_values : values)
        unique.insert(thread_values.begin(), thread_values.end());
    assert(unique.size() == kThreadCount * kIdsPerThread);
}
