add_interface_component(
    COMPONENT_NAME                  container
    NAMESPACE                       lux::cxx
)

component_include_directories(
    container
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)