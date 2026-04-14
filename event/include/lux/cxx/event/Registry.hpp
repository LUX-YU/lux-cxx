#pragma once

#include <vector>
#include <algorithm>
#include <span>
#include <string_view>
#include "EventTraits.hpp"

namespace lux::cxx::event
{
    // ── Event Meta ───────────────────────────────────────────────
    // Runtime reflection info for a registered event type.
    // Used by diagnostics, editor panels, and logging.
    struct EventMeta
    {
        EventTypeId type_id;
        std::string_view name;
        uint16_t size;
        uint16_t alignment;
        bool trivially_copyable;
    };

    // ── Event Type Registry ──────────────────────────────────────
    // Global sorted-vector registry. Register at static-init time,
    // query at runtime for diagnostics/reflection.

    class EventTypeRegistry
    {
    public:
        template <Event E>
        static void register_type(std::string_view name)
        {
            auto& reg = registry();
            EventMeta meta{
                .type_id = kEventTypeId<E>,
                .name = name,
                .size = static_cast<uint16_t>(sizeof(E)),
                .alignment = static_cast<uint16_t>(alignof(E)),
                .trivially_copyable = std::is_trivially_copyable_v<E>,
            };

            auto it = std::lower_bound(
                reg.begin(), reg.end(), meta.type_id,
                [](const EventMeta &m, EventTypeId id)
                { return m.type_id < id; }
            );

            if (it != reg.end() && it->type_id == meta.type_id)
                return; // already registered

            reg.insert(it, meta);
        }

        static const EventMeta *find(EventTypeId id)
        {
            auto& reg = registry();
            auto it = std::lower_bound(
                reg.begin(), reg.end(), id,
                [](const EventMeta &m, EventTypeId tid)
                { return m.type_id < tid; });

            if (it != reg.end() && it->type_id == id)
                return &*it;

            return nullptr;
        }

        static std::span<const EventMeta> all()
        {
            return registry();
        }

    private:
        static std::vector<EventMeta>& registry()
        {
            static std::vector<EventMeta> instance;
            return instance;
        }
    };

    // ── Registration Macro ───────────────────────────────────────
    // Use in a .cpp file to register an event type at static-init time.
    //
    //   LUX_REGISTER_EVENT(MyEvent);
#define LUX_REGISTER_EVENT(Type) \
    static const bool _lux_event_reg_##Type = [] {                             \
        ::lux::cxx::event::EventTypeRegistry::register_type<Type>(#Type);      \
        return true; }()
} // namespace lux::cxx::event
