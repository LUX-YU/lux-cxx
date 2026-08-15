#include <lux/cxx/compile_time/TypeToken.hpp>

#include <cassert>

namespace
{
    struct First;
    struct Second;

    constexpr bool testTypeToken()
    {
        constexpr auto first = lux::cxx::typeToken<First>();
        constexpr auto same = lux::cxx::typeToken<First>();
        constexpr auto second = lux::cxx::typeToken<Second>();
        return first.isValid() && first == same && first != second;
    }
} // namespace

int main()
{
    static_assert(testTypeToken());
    assert(lux::cxx::typeToken<int>() == lux::cxx::typeToken<int>());
    assert(lux::cxx::typeToken<int>() != lux::cxx::typeToken<float>());
    return 0;
}
