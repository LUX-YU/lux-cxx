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

set(ENABLE_EVENT_TEST OFF CACHE BOOL "Enable event test")
if(ENABLE_EVENT_TEST)
    add_subdirectory(test)
endif()
