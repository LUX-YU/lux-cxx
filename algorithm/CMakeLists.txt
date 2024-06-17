add_interface_component(
    COMPONENT_NAME                  algorithm
    NAMESPACE                       lux::cxx
)

component_include_directories(
    algorithm
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)