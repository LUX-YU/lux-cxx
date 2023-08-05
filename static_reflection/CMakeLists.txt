add_interface_component(
    COMPONENT_NAME                  static_reflection
    NAMESPACE                       lux::cxx
    BUILD_TIME_EXPORT_INCLUDE_DIRS  ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME_INCLUDE_PREFIX     include
    INTERNAL_DEPENDENCIES           lux::cxx::lan_model
                                    lux::cxx::compile_time
)
