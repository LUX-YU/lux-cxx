#pragma once
#include <lux/cxx/reflection/runtime/Marker.hpp>
#include "ext_types.hpp"

// --- Non-intrusive registration of the foreign types (the feature under test).
//     The proxy lives here, in the main file; the parser follows _lux_target to
//     the real ext::ExtPoint / ext::ExtColor declarations in ext_types.hpp.
LUX_REFLECT_EXTERNAL(marked, ::ext::ExtPoint)
LUX_REFLECT_EXTERNAL_ENUM(marked, ::ext::ExtColor)

// --- Intrusive controls: structurally identical types annotated the normal way,
//     so the test can prove the external path yields the same meta shape.
struct LUX_META(marked) IntrPoint
{
    int    x;
    double y;
};

enum class LUX_META(marked) IntrColor
{
    Red,
    Green,
    Blue
};
