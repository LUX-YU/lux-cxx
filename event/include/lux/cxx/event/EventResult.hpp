#pragma once

#include <cstdint>

namespace lux::cxx::event
{
    /// Result returned by interceptable signal callbacks.
    /// - Continue: pass the event to the next subscriber
    /// - Handled: event consumed, stop dispatching
    enum class EventResult : uint8_t
    {
        Continue, // Continue dispatching to lower-priority subscribers
        Handled   // Event consumed — stop dispatching
    };
} // namespace lux::cxx::event
