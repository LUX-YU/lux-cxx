#include <lux/cxx/diagnostic/DiagnosticRecord.hpp>

#include <cassert>
#include <atomic>
#include <cstdlib>
#include <new>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace
{
    std::atomic<std::size_t> g_allocations{};
    struct TestDomain final
    {
    };

    struct TestClock final
    {
        using timestamp_type = lux::cxx::Timestamp<TestDomain>;

        [[nodiscard]] static constexpr timestamp_type now() noexcept
        {
            return timestamp_type{42};
        }
    };

    struct Category final
    {
        std::uint16_t value{};
    };

    enum class ECode : std::uint16_t
    {
        NONE,
        FAILED
    };

    using Record = lux::cxx::BasicDiagnosticRecord<8, TestClock, Category, ECode>;

    constexpr bool constexprRecordWorks()
    {
        const Record record{
            lux::cxx::EDiagnosticSeverity::WARNING,
            Category{7},
            ECode::FAILED,
            TestClock::now(),
            9,
            11,
            "123456789"
        };
        return record.timestamp.ticks() == 42
            && record.message.view() == "12345678"
            && record.message_truncated
            && record.category.value == 7;
    }

    static_assert(constexprRecordWorks());
    static_assert(std::is_trivially_destructible_v<Record>);
    static_assert(noexcept(Record{}));
}

void* operator new(std::size_t size)
{
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

int main()
{
    const auto before_capture = g_allocations.load(std::memory_order_relaxed);
    const auto first = lux::cxx::DiagnosticRecord::capture(
        lux::cxx::EDiagnosticSeverity::ERROR,
        lux::cxx::DiagnosticCategory{"binary"},
        17,
        3,
        "invalid varint"
    );
    const auto second = lux::cxx::DiagnosticRecord::capture(
        lux::cxx::EDiagnosticSeverity::INFO,
        lux::cxx::DiagnosticCategory{"binary"},
        0,
        4,
        "recovered"
    );

    assert(first.message.view() == "invalid varint");
    assert(!first.message_truncated);
    assert(first.category.name() == "binary");
    assert(first.thread_id != 0);
    assert(first.thread_id == second.thread_id);
    assert(first.timestamp.ticks() <= second.timestamp.ticks());
    assert(g_allocations.load(std::memory_order_relaxed) == before_capture);
}
