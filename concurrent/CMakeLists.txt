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