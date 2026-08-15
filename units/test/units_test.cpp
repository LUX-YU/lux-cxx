#include <lux/cxx/units/Units.hpp>

#include <cassert>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace
{
    constexpr bool testUnits()
    {
        constexpr lux::cxx::KibibyteCount<> kibibytes(2);
        constexpr auto bytes = lux::cxx::quantityCast<
            lux::cxx::ByteCount<>
        >(kibibytes);
        if (!bytes || (*bytes).value() != 2048) return false;

        constexpr lux::cxx::Kilohertz<> kilohertz(2.5);
        constexpr auto hertz = lux::cxx::quantityCast<
            lux::cxx::Frequency<>
        >(kilohertz);
        if (!hertz || (*hertz).value() != 2500.0) return false;

        constexpr auto radians = lux::cxx::degreesToRadians(180.0);
        return lux::cxx::radiansToDegrees(radians) > 179.999 &&
               lux::cxx::radiansToDegrees(radians) < 180.001;
    }
} // namespace

int main()
{
    static_assert(testUnits());
    static_assert(sizeof(lux::cxx::ByteCount<>) == sizeof(std::uint64_t));
    static_assert(!std::is_convertible_v<std::uint64_t, lux::cxx::ByteCount<>>);

    constexpr lux::cxx::ByteCount<std::uint64_t> too_many(
        (std::numeric_limits<std::uint64_t>::max)()
    );
    const auto narrowed = lux::cxx::quantityCast<
        lux::cxx::ByteCount<std::uint8_t>
    >(too_many);
    assert(!narrowed);
    return 0;
}
