#include <array>
#include <cassert>
#include <lux/cxx/core/function_ref.hpp>
#include <lux/cxx/core/move_only_function.hpp>
#include <memory>
#include <type_traits>

namespace
{
    int safeFunction(int n) noexcept
    {
        return n + 1;
    }
    int throwingFunction(int n)
    {
        return n + 2;
    }
    struct ThrowingConversion
    {
        operator int() const noexcept(false)
        {
            return 1;
        }
    };
    struct ReturnsConversion
    {
        ThrowingConversion operator()() const noexcept
        {
            return {};
        }
    };
    struct Large
    {
        std::array<int, 128> values{};
        int operator()(int n) noexcept
        {
            return ++values[0] + n;
        }
    };
} // namespace

int main()
{
    using Ref = lux::cxx::function_ref<int(int) noexcept>;
    using Owned = lux::cxx::move_only_function<int(int) noexcept>;
    static_assert(std::is_nothrow_invocable_r_v<int, Ref&, int>);
    static_assert(std::is_nothrow_invocable_r_v<int, Owned&, int>);
    static_assert(!std::is_constructible_v<Ref, decltype(&throwingFunction)>);
    static_assert(!std::is_constructible_v<Owned, decltype(&throwingFunction)>);
    static_assert(!std::is_constructible_v<lux::cxx::function_ref<int() noexcept>, ReturnsConversion&>);
    static_assert(!std::is_constructible_v<lux::cxx::move_only_function<int() noexcept>, ReturnsConversion>);
    static_assert(!std::is_copy_constructible_v<Owned>);
    Ref function(&safeFunction);
    assert(function(3) == 4);
    auto mutable_call = [state = 0](int n) mutable noexcept { return ++state + n; };
    Ref borrowed(mutable_call);
    assert(borrowed(1) == 2 && borrowed(1) == 3);
    static_assert(!std::is_constructible_v<Ref, decltype(mutable_call)>);
    const auto constant = [](int n) noexcept { return n * 2; };
    Ref const_borrow(constant);
    assert(const_borrow(3) == 6);
    Owned local([p = std::make_unique<int>(4)](int n) noexcept { return *p + n; });
    auto transferred = std::move(local);
    assert(!local && transferred(2) == 6);
    Owned heap(Large{});
    assert(heap(1) == 2);
    transferred = std::move(heap);
    assert(!heap && transferred(1) == 3);
    Owned other(&safeFunction);
    swap(other, transferred);
    assert(transferred(5) == 6 && other(1) == 4);
    other.reset();
    assert(!other);
    lux::cxx::move_only_function<int(int)> permissive(&throwingFunction);
    assert(permissive(1) == 3);
    static_assert(!std::is_constructible_v<Owned, decltype(permissive)>);
}
