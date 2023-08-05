find_package(nameof CONFIG REQUIRED)

add_interface_component(
    COMPONENT_NAME                  compile_time
    NAMESPACE                       lux::cxx
    BUILD_TIME_EXPORT_INCLUDE_DIRS  ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME_INCLUDE_PREFIX     include
    PUBLIC_LIBRARIES                nameof::nameof
    TRANSITIVE_PACKAGES_COMMANDS    "find_package(nameof CONFIG REQUIRED)"
)