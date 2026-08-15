#include <lux/cxx/container/SmallVector.hpp>

#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>

namespace
{
    template<typename Type>
    class FailingAllocator final
    {
      public:
        using value_type = Type;

        template<typename Other>
        friend class FailingAllocator;

        FailingAllocator()
            : remaining_(std::make_shared<std::size_t>(0))
        {
        }

        template<typename Other>
        FailingAllocator(const FailingAllocator<Other>& other) noexcept
            : remaining_(other.remaining_)
        {
        }

        [[nodiscard]] Type* allocate(std::size_t count)
        {
            if (*remaining_ == 0) throw std::bad_alloc{};
            --*remaining_;
            return std::allocator<Type>{}.allocate(count);
        }

        void deallocate(Type* pointer, std::size_t count) noexcept
        {
            std::allocator<Type>{}.deallocate(pointer, count);
        }

        template<typename Other>
        [[nodiscard]] bool operator==(
            const FailingAllocator<Other>& other
        ) const noexcept
        {
            return remaining_ == other.remaining_;
        }

      private:
        std::shared_ptr<std::size_t> remaining_;
    };

    template <typename Type>
    class CountingAllocator final
    {
      public:
        using value_type = Type;
        using propagate_on_container_copy_assignment = std::true_type;
        using propagate_on_container_move_assignment = std::true_type;
        using propagate_on_container_swap = std::true_type;

        template <typename Other>
        friend class CountingAllocator;

        CountingAllocator()
            : allocations_(std::make_shared<std::size_t>(0))
        {
        }

        template <typename Other>
        CountingAllocator(const CountingAllocator<Other>& other) noexcept
            : allocations_(other.allocations_)
        {
        }

        [[nodiscard]] Type* allocate(std::size_t count)
        {
            ++*allocations_;
            return std::allocator<Type>{}.allocate(count);
        }

        void deallocate(Type* pointer, std::size_t count) noexcept
        {
            std::allocator<Type>{}.deallocate(pointer, count);
        }

        [[nodiscard]] std::size_t allocations() const noexcept
        {
            return *allocations_;
        }

        template <typename Other>
        [[nodiscard]] bool operator==(
            const CountingAllocator<Other>& other
        ) const noexcept
        {
            return allocations_ == other.allocations_;
        }

      private:
        std::shared_ptr<std::size_t> allocations_;
    };
} // namespace

int main()
{
    CountingAllocator<int> allocator;
    lux::cxx::SmallVector<int, 4, CountingAllocator<int>> values(allocator);
    for (int value = 0; value < 4; ++value) values.push_back(value);
    assert(allocator.allocations() == 0);

    values.push_back(4);
    assert(allocator.allocations() == 1);
    assert(values.get_allocator() == allocator);

    auto* heap_data = values.data();
    auto moved = std::move(values);
    assert(moved.data() == heap_data);
    assert(moved.size() == 5);

    auto copied = moved;
    assert(copied == moved);
    assert(copied.get_allocator() == allocator);

    FailingAllocator<int> failing_allocator;
    lux::cxx::SmallVector<int, 2, FailingAllocator<int>> failing(
        failing_allocator
    );
    failing.push_back(1);
    failing.push_back(2);
    bool allocation_failed = false;
    try
    {
        failing.push_back(3);
    }
    catch (const std::bad_alloc&)
    {
        allocation_failed = true;
    }
    assert(allocation_failed);
    assert(failing.size() == 2);
    assert(failing[0] == 1 && failing[1] == 2);
    return 0;
}
