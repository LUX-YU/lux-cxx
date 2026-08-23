#pragma once

#include <lux/cxx/reflection/runtime/Marker.hpp>

namespace lux::cxx::reflection::test
{
    template<class Value>
    struct Slot
    {
    };

    class LUX_META(marked) IncludedLayout
    {
    public:
        static Slot<int> inherited_value;
    };
}
