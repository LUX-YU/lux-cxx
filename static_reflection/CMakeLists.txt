add_interface_component(
    COMPONENT_NAME                  static_reflection
    NAMESPACE                       lux::cxx
)

component_include_directories(
    static_reflection
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

component_add_internal_dependencies(
    static_reflection
        lux::cxx::compile_time
)

set(ENABLE_STATIC_REFLECTION_TEST OFF CACHE BOOL "Enable static reflection test")
if(ENABLE_STATIC_REFLECTION_TEST)
    add_subdirectory(test)
endif()
