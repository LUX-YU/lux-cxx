#pragma once
// Generator scan target for the JSON external round-trip. The proxies live here
// (the main file fed to lux_meta_generator); the parser follows _lux_target to
// the real extlib::Widget / extlib::Status declarations in ext_serialize_types.hpp
// and emits meta_info<::extlib::Widget> + enum_meta<::extlib::Status> into the
// generated ext_json_register.serialize.hpp. In a normal build these macros
// expand to nothing.
#include <lux/cxx/reflection/runtime/Marker.hpp>
#include "ext_serialize_types.hpp"

LUX_REFLECT_EXTERNAL(serializable, ::extlib::Widget)
LUX_REFLECT_EXTERNAL_ENUM(serializable, ::extlib::Status)
