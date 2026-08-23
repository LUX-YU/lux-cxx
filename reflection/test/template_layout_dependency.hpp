#pragma once

#include <lux/cxx/reflection/runtime/Marker.hpp>

namespace lux::cxx::reflection::test
{
    class LUX_META(marked) IncludedLayout
    {
    public:
        static int inherited_value;
    };
}
