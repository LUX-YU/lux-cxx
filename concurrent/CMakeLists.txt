add_interface_component(
    COMPONENT_NAME                  concurrent
    NAMESPACE                       lux::cxx
    BUILD_TIME_EXPORT_INCLUDE_DIRS  ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME_INCLUDE_PREFIX     include
)