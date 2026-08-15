#pragma once

#include <cstdint>
#include <optional>

namespace lux::cxx
{
    /// Result of a non-blocking queue submission.
    enum class EQueuePushResult : std::uint8_t
    {
        ACCEPTED,
        FULL,
        CLOSED,
        TIMEOUT,
        CANCELLED,
    };

    enum class EQueuePopResult : std::uint8_t
    {
        VALUE,
        EMPTY,
        CLOSED_AND_DRAINED,
        TIMEOUT,
        CANCELLED,
    };

    enum class EQueueState : std::uint8_t
    {
        OPEN,
        CLOSED,
        DRAINED,
    };

    template <typename Value>
    struct QueuePopValue final
    {
        EQueuePopResult result = EQueuePopResult::EMPTY;
        std::optional<Value> value;
    };
} // namespace lux::cxx
