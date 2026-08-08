#pragma once

#include <cstdint>

namespace lux::cxx
{
    /// Result of a non-blocking queue submission.
    enum class EQueuePushResult : std::uint8_t
    {
        ACCEPTED,
        FULL,
        CLOSED,
    };
} // namespace lux::cxx
