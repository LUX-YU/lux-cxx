#include "ext_serialize_types.hpp"
#include "ext_yaml_register.serialize.hpp"

#include <lux/cxx/serialization/yaml.hpp>

#include <iostream>

using namespace lux::cxx::ser;
using namespace extlib;

int main()
{
    Widget original;
    original.name = "external";
    original.count = 42;
    original.enabled = true;
    original.status = Status::Running;
    original.values = { 1, 2, 3 };

    const auto parsed = from_yaml<Widget>(to_yaml(original));
    const bool good = parsed && parsed->name == original.name &&
                      parsed->count == original.count && parsed->enabled &&
                      parsed->status == Status::Running &&
                      parsed->values == original.values;
    if (!good)
    {
        std::cerr << "[FAIL] external YAML reflection round-trip\n";
        return 1;
    }
    std::cout << "[PASS] external YAML reflection round-trip\n";
    return 0;
}
