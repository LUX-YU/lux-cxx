add_interface_component(
    COMPONENT_NAME                  subprogram
    NAMESPACE                       lux::cxx
)

component_include_directories(
    subprogram
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)