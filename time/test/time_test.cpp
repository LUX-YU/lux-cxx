#include <lux/cxx/time/ClockMapping.hpp>
#include <lux/cxx/time/ManualClock.hpp>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <limits>

namespace
{
    struct SensorTag;
    using SensorDomain = lux::cxx::SensorTimeDomain<SensorTag>;
    using SensorTimestamp = lux::cxx::Timestamp<SensorDomain>;
    using SteadyTimestamp = lux::cxx::Timestamp<lux::cxx::SteadyTimeDomain>;

    constexpr bool testTime()
    {
        SensorTimestamp sensor(100);
        lux::cxx::ClockMapping<SensorDomain, lux::cxx::SteadyTimeDomain> mapping(
            SensorTimestamp(100),
            SteadyTimestamp(1'000),
            2.0,
            std::chrono::nanoseconds(4),
            7
        );
        const auto mapped = mapping.map(SensorTimestamp(125));
        if (!mapped || (*mapped).ticks() != 1'050) return false;
        if (mapping.uncertainty().count() != 4 || mapping.revision() != 7)
        {
            return false;
        }

        lux::cxx::ManualClock<SensorDomain> clock(sensor);
        return clock.advance(std::chrono::nanoseconds(5)).ticks() == 105;
    }
} // namespace

int main()
{
    static_assert(testTime());
    static_assert(sizeof(SensorTimestamp) == sizeof(std::int64_t));
    static_assert(!std::is_convertible_v<SensorTimestamp, SteadyTimestamp>);

    lux::cxx::ClockMapping<SensorDomain, lux::cxx::SteadyTimeDomain> invalid(
        SensorTimestamp(0),
        SteadyTimestamp(0),
        0.0
    );
    assert(!invalid.map(SensorTimestamp(1)));

    lux::cxx::ClockMapping<SensorDomain, lux::cxx::SteadyTimeDomain> overflow(
        SensorTimestamp(0),
        SteadyTimestamp((std::numeric_limits<std::int64_t>::max)()),
        2.0
    );
    assert(!overflow.map(SensorTimestamp(1)));
    return 0;
}
