#include <lux/cxx/abi/BuildInfo.hpp>
#include <lux/cxx/algorithm/sha256.hpp>
#include <lux/cxx/binary/Binary.hpp>
#include <lux/cxx/concurrent/AdmissionGate.hpp>
#include <lux/cxx/container/StableSlotMap.hpp>
#include <lux/cxx/core/Core.hpp>
#include <lux/cxx/diagnostic/Diagnostic.hpp>
#include <lux/cxx/memory/SharedBytes.hpp>
#include <lux/cxx/time/Timestamp.hpp>
#include <lux/cxx/units/Units.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

namespace
{
    struct ValueTag final
    {
    };
}

int main()
{
    constexpr auto digest = lux::cxx::algorithm::Sha256::hash("lux-cxx");
    static_assert(digest.size() == 32);

    std::array<std::byte, 16> storage{};
    lux::cxx::BinarySpanWriter writer(storage);
    assert(writer.writeVarUint(42));
    lux::cxx::BinaryReader reader(writer.written());
    std::uint64_t value = 0;
    assert(reader.readVarUint(value));
    assert(value == 42);

    lux::cxx::StableSlotMap<int, ValueTag> values;
    const auto key = values.emplace(7);
    assert(values.isValid(key));
    assert(*values.find(key) == 7);

    lux::cxx::AdmissionGate<> gate;
    auto ticket = gate.tryAcquire();
    assert(ticket);
    assert(lux::cxx::AbiBuildInfo::fingerprint().digest().size() == 32);
}
