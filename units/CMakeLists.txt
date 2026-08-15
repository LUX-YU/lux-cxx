add_interface_component(
    COMPONENT_NAME units
    NAMESPACE      lux::cxx
)

component_include_directories(
    units
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

component_add_internal_dependencies(units lux::cxx::core)

if(LUX_CXX_BUILD_TESTS AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()

lux_add_header_self_containment(
    units_header_self_containment
    lux::cxx::units
    lux/cxx/units/Quantity.hpp
    lux/cxx/units/Units.hpp
)
