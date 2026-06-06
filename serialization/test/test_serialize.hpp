#pragma once
#include <lux/cxx/reflection/runtime/Marker.hpp>
#include <string>

// A plain aggregate.
struct LUX_META(serializable) Point
{
    int x;
    int y;
};

// Field-level options exercised:
//   - name override   (full_name)
//   - required flag    (age)
//   - short option     (age -> "a")
//   - skip flag        (cached, still public so a member pointer exists)
//   - nested reflected field (position : Point)
struct LUX_META(serializable) Person
{
    std::string LUX_META(serializable, name = "full_name") name;
    int         LUX_META(serializable, required, short = "a") age;
    Point       position;
    int         LUX_META(serializable, skip) cached;
};

namespace app::config
{
    struct LUX_META(serializable) Server
    {
        std::string host;
        int         port;
    };
}
