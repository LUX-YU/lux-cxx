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
        lux::cxx::lan_model
        lux::cxx::compile_time
)
