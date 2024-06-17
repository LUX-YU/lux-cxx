find_package(nameof CONFIG REQUIRED)

add_interface_component(
    COMPONENT_NAME                  compile_time
    NAMESPACE                       lux::cxx
)

component_include_directories(
    compile_time
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

target_link_libraries(
    compile_time
    INTERFACE
    nameof::nameof
)

component_add_transitive_commands(
    compile_time
    "find_package(nameof CONFIG REQUIRED)"
)
