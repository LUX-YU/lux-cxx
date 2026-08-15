add_interface_component(
    COMPONENT_NAME                  event
    NAMESPACE                       lux::cxx
)

component_include_directories(
    event
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

component_add_internal_dependencies(
    event
        lux::cxx::compile_time
        lux::cxx::container
)

set(ENABLE_EVENT_TEST ${LUX_CXX_BUILD_TESTS} CACHE BOOL "Enable event test")
if(ENABLE_EVENT_TEST AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()
