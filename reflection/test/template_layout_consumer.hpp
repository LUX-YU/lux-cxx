#pragma once

#include "template_layout_dependency.hpp"

namespace lux::cxx::reflection::test
{
    template<class Self, class Base>
    class Lineage : public Base
    {
    };

    class LUX_META(marked) LayoutConsumer final
        : public Lineage<LayoutConsumer, IncludedLayout>
    {
    public:
        static Slot<float> local_value;
    };
}
