#pragma once
// Simulated third-party header for the serialization end-to-end test. Carries NO
// reflection annotations and does not include Marker.hpp — it stands in for a
// foreign library type. It is registered non-intrusively in ext_json_register.hpp
// / ext_xml_register.hpp via LUX_REFLECT_EXTERNAL.
#include <string>
#include <vector>

namespace extlib
{
    enum class Status { Idle, Running, Done };

    struct Widget
    {
        std::string      name;
        int              count;
        bool             enabled;
        Status           status;   // external enum field -> exercises enum_meta
        std::vector<int> values;   // sequence field
    };
}
