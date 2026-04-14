#pragma once

#include <cstdint>
#include <lux/cxx/compile_time/type_info.hpp>

namespace lux::cxx::event
{
    // ── Event Type ID ────────────────────────────────────────────
    // Compile-time type identifier using FNV-1a hash from lux-cxx.
    // No manual ID assignment needed.

    using EventTypeId = uint64_t;

    template <typename E>
    inline constexpr EventTypeId kEventTypeId = lux::cxx::type_hash<E>();
} // namespace lux::cxx::event
