#pragma once
// Generator scan target for the XML external round-trip. Same content as
// ext_json_register.hpp but a distinct basename so the generated
// ext_xml_register.serialize.hpp does not collide with the JSON one.
#include <lux/cxx/reflection/runtime/Marker.hpp>
#include "ext_serialize_types.hpp"

LUX_REFLECT_EXTERNAL(serializable, ::extlib::Widget)
LUX_REFLECT_EXTERNAL_ENUM(serializable, ::extlib::Status)
