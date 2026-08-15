#include <lux/cxx/core/Core.hpp>

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace
{
    std::atomic<std::size_t> g_allocations{};

    struct IdTag;
    struct NameTag;

    enum class EPermission : std::uint8_t
    {
        NONE = 0,
        READ = 1,
        WRITE = 2,
        EXECUTE = 4,
    };

    struct Counter final
    {
        int value = 0;

        int add(int amount)
        {
            value += amount;
            return value;
        }
    };

    int addOne(int value)
    {
        return value + 1;
    }

    struct LargeCallable final
    {
        std::array<std::byte, 64> padding{};

        int operator()(int value) noexcept
        {
            return value + 2;
        }
    };

    struct alignas(32) OverAlignedCallable final
    {
        int operator()(int value) noexcept
        {
            return value + 3;
        }
    };

    struct alignas(std::max_align_t) BoundaryCallable final
    {
        std::array<std::byte, 32> state{};

        int operator()(int value) noexcept
        {
            return value + 4;
        }
    };

    struct ConstructionError final
    {
    };

    struct ThrowingCallable final
    {
        ThrowingCallable() = default;
        ThrowingCallable(const ThrowingCallable&)
        {
            throw ConstructionError{};
        }
        ThrowingCallable(ThrowingCallable&&) noexcept = default;

        void operator()() noexcept
        {
        }
    };

    struct LifetimeCallable final
    {
        int* alive = nullptr;

        explicit LifetimeCallable(int& count) noexcept
            : alive(&count)
        {
            ++*alive;
        }

        LifetimeCallable(const LifetimeCallable&) = delete;

        LifetimeCallable(LifetimeCallable&& other) noexcept
            : alive(std::exchange(other.alive, nullptr))
        {
        }

        ~LifetimeCallable()
        {
            if (alive != nullptr) --*alive;
        }

        void operator()() noexcept
        {
        }
    };

    constexpr bool testConstexprCore()
    {
        using Id = lux::cxx::StrongId<IdTag, std::uint32_t>;
        constexpr Id id(42);
        static_assert(sizeof(Id) == sizeof(std::uint32_t));
        if (!id.isValid() || id.value() != 42) return false;

        constexpr lux::cxx::FixedText text("lux");
        if (text.view() != "lux") return false;

        constexpr auto name = lux::cxx::StableNameIdView<NameTag>("lux.core");
        if (!name.isValid()) return false;
        if (name.hash() != 0x745f67891b8373d7ULL) return false;

        constexpr auto sum = lux::cxx::checkedAdd<std::uint8_t>(20, 22);
        if (!sum || *sum != 42) return false;
        constexpr auto overflow = lux::cxx::checkedAdd<std::uint8_t>(250, 10);
        if (overflow) return false;
        constexpr auto aligned = lux::cxx::alignUpChecked<std::uint32_t>(17, 8);
        if (!aligned || *aligned != 24) return false;
        return true;
    }
} // namespace

template <>
struct lux::cxx::enable_enum_flags<EPermission> : std::true_type
{
};

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

void* operator new(std::size_t size, std::align_val_t alignment)
{
    g_allocations.fetch_add(1, std::memory_order_relaxed);
#if defined(_MSC_VER)
    if (void* memory = _aligned_malloc(size, static_cast<std::size_t>(alignment)))
    {
        return memory;
    }
#else
    const auto boundary = static_cast<std::size_t>(alignment);
    const auto rounded = (size + boundary - 1) / boundary * boundary;
    if (void* memory = std::aligned_alloc(boundary, rounded)) return memory;
#endif
    throw std::bad_alloc{};
}

void operator delete(void* memory, std::align_val_t) noexcept
{
#if defined(_MSC_VER)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}

void operator delete(void* memory, std::size_t, std::align_val_t) noexcept
{
#if defined(_MSC_VER)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}

int main()
{
    using lux::cxx::operator|;

    static_assert(testConstexprCore());
    static_assert(sizeof(lux::cxx::function_ref<int(int)>) <= 2 * sizeof(void*));
    static_assert(sizeof(lux::cxx::Delegate<int(int)>) <= 2 * sizeof(void*));
    static_assert(
        lux::cxx::move_only_function<int(int)>::stores_inplace<decltype(&addOne)>
    );
    static_assert(
        !lux::cxx::move_only_function<int(int)>::stores_inplace<LargeCallable>
    );
    static_assert(
        !lux::cxx::move_only_function<int(int)>::stores_inplace<OverAlignedCallable>
    );
    static_assert(sizeof(BoundaryCallable)
        == lux::cxx::move_only_function<int(int)>::inplace_size);
    static_assert(alignof(BoundaryCallable)
        == lux::cxx::move_only_function<int(int)>::inplace_alignment);
    static_assert(
        lux::cxx::move_only_function<int(int)>::stores_inplace<BoundaryCallable>
    );

    const auto before_small = g_allocations.load(std::memory_order_relaxed);
    lux::cxx::move_only_function<int(int)> small(&addOne);
    assert(small(2) == 3);
    assert(g_allocations.load(std::memory_order_relaxed) == before_small);

    lux::cxx::move_only_function<int(int)> moved(std::move(small));
    assert(!small);
    assert(moved(4) == 5);
    moved = std::move(moved);
    assert(moved(5) == 6);
    moved.reset();
    assert(!moved);

    const auto before_large = g_allocations.load(std::memory_order_relaxed);
    lux::cxx::move_only_function<int(int)> large(LargeCallable{});
    assert(g_allocations.load(std::memory_order_relaxed) == before_large + 1);
    assert(large(1) == 3);

    const auto before_boundary = g_allocations.load(std::memory_order_relaxed);
    lux::cxx::move_only_function<int(int)> boundary(BoundaryCallable{});
    assert(boundary(1) == 5);
    assert(g_allocations.load(std::memory_order_relaxed) == before_boundary);

    const auto before_over_aligned = g_allocations.load(std::memory_order_relaxed);
    lux::cxx::move_only_function<int(int)> over_aligned(OverAlignedCallable{});
    assert(over_aligned(1) == 4);
    assert(g_allocations.load(std::memory_order_relaxed) == before_over_aligned + 1);

    ThrowingCallable throwing_source;
    bool construction_threw = false;
    try
    {
        lux::cxx::move_only_function<void()> throwing(throwing_source);
    }
    catch (const ConstructionError&)
    {
        construction_threw = true;
    }
    assert(construction_threw);

    auto owned_value = std::make_unique<int>(41);
    lux::cxx::move_only_function<int()> move_only(
        [value = std::move(owned_value)]() noexcept { return *value + 1; }
    );
    assert(!owned_value);
    assert(move_only() == 42);

    int alive = 0;
    {
        lux::cxx::move_only_function<void()> lifetime(LifetimeCallable{alive});
        assert(alive == 1);
        auto moved_lifetime = std::move(lifetime);
        assert(!lifetime);
        assert(alive == 1);
        moved_lifetime.reset();
        assert(alive == 0);
    }

    auto mutable_lambda = [offset = 4](int value) mutable
    {
        return value + offset++;
    };
    lux::cxx::function_ref<int(int)> reference(mutable_lambda);
    assert(reference(1) == 5);
    assert(reference(1) == 6);
    lux::cxx::function_ref<int(int)> function_reference(&addOne);
    assert(function_reference(4) == 5);

    Counter counter;
    auto member = lux::cxx::Delegate<int(int)>::bind<&Counter::add>(counter);
    assert(member(3) == 3);
    assert(member(4) == 7);
    auto free = lux::cxx::Delegate<int(int)>::bind<&addOne>();
    assert(free(8) == 9);
    auto runtime_free = lux::cxx::Delegate<int(int)>::fromFunction(&addOne);
    assert(runtime_free(9) == 10);

    auto permissions = EPermission::READ | EPermission::WRITE;
    assert(permissions.containsAll(EPermission::READ));
    assert(permissions.containsAny(EPermission::WRITE));
    assert(!permissions.containsAny(EPermission::EXECUTE));
    static_assert(sizeof(decltype(permissions)) == sizeof(std::uint8_t));

    lux::cxx::FixedText<4> truncated;
    assert(!truncated.assign("abcdef"));
    assert(truncated.view() == "abcd");

    int scope_value = 0;
    {
        lux::cxx::scope_exit guard([&scope_value]() noexcept
        {
            scope_value = 42;
        });
    }
    assert(scope_value == 42);

    lux::cxx::StableNameId<NameTag> owned("lux.core");
    assert(owned.view() == lux::cxx::StableNameIdView<NameTag>("lux.core"));
    assert(!lux::cxx::StableNameIdView<NameTag>::fromVerified("lux.core", 1).isValid());

    assert(lux::cxx::saturatingAdd<std::uint8_t>(250, 10) == 255);
    assert(!lux::cxx::alignUpChecked<std::uint32_t>(4, 3));
    assert(!lux::cxx::checkedNarrow<std::uint8_t>(300));
    return 0;
}
