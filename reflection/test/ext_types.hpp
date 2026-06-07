#pragma once
// Simulated third-party header. Intentionally carries NO reflection annotations
// and does not include Marker.hpp — it stands in for a foreign library type you
// cannot modify. It is reflected *non-intrusively* via LUX_REFLECT_EXTERNAL in
// ext_register.hpp.
namespace ext
{
    struct ExtPoint
    {
        int    x;
        double y;
    };

    enum class ExtColor
    {
        Red,
        Green,
        Blue
    };
}
