add_interface_component(
    COMPONENT_NAME                  concurrent
    NAMESPACE                       lux::cxx
)

component_include_directories(
    concurrent
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

component_add_internal_dependencies(
    concurrent
        lux::cxx::compile_time
)

set(ENABLE_CONCURRENT_TEST OFF CACHE BOOL "Enable concurrent test")
if(ENABLE_CONCURRENT_TEST)
    add_subdirectory(test)
endif()
