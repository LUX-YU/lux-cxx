add_interface_component(
    COMPONENT_NAME                  lan_model
    NAMESPACE                       lux::cxx
)

component_include_directories(
    lan_model
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)