add_interface_component(
    COMPONENT_NAME time
    NAMESPACE      lux::cxx
)

component_include_directories(
    time
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

component_add_internal_dependencies(time lux::cxx::core)

if(LUX_CXX_BUILD_TESTS AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()

lux_add_header_self_containment(
    time_header_self_containment
    lux::cxx::time
    lux/cxx/time/ClockMapping.hpp
    lux/cxx/time/ManualClock.hpp
    lux/cxx/time/Timestamp.hpp
)
