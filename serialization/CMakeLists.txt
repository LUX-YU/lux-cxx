# Serialization module (header-only core).
#
# The compile-time contract lives in include/lux/cxx/serialization/meta_info.hpp.
# Concrete meta_info<T> specialisations are produced by lux_meta_generator using
# the bundled templates/serializable.template.inja; downstream users drive it
# through cmake/lux_serialization.cmake (see component_add_cmake_scripts below).

add_interface_component(
    COMPONENT_NAME serialization
    NAMESPACE      lux::cxx
)

component_include_directories(
    serialization
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

component_add_internal_dependencies(
    serialization
        lux::cxx::compile_time
)

# Ship the codegen template + the turn-key CMake helpers so that consumers get
# lux_target_add_serialization() after find_package + include_component_cmake_scripts.
component_add_cmake_scripts(
    serialization
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/lux_serialization.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/serializable.template.inja
)

# JSON backend — COMPILED so the vendored nlohmann_json stays a PRIVATE,
# implementation-only dependency. The public header (json.hpp) exposes no
# nlohmann type, so consumers can use their own nlohmann_json without clashing.
add_component(
    COMPONENT_NAME serialization_json
    NAMESPACE      lux::cxx
    STATIC
    SOURCE_FILES   ${CMAKE_CURRENT_SOURCE_DIR}/src/json.cpp
)

component_include_directories(
    serialization_json
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

component_add_internal_dependencies(
    serialization_json
        lux::cxx::serialization
)

# nlohmann_json is an internal detail of json.cpp only — PRIVATE, never exported.
target_link_libraries(serialization_json PRIVATE lux::cxx::nlohmann_json)

# XML backend — same pattern: tinyxml2 hidden in xml.cpp, PRIVATE-linked.
add_component(
    COMPONENT_NAME serialization_xml
    NAMESPACE      lux::cxx
    STATIC
    SOURCE_FILES   ${CMAKE_CURRENT_SOURCE_DIR}/src/xml.cpp
)

component_include_directories(
    serialization_xml
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

component_add_internal_dependencies(
    serialization_xml
        lux::cxx::serialization
)

target_link_libraries(serialization_xml PRIVATE lux::cxx::tinyxml2)

# Arguments backend — header-only; depends on the project's own arguments module
# (no third-party type to hide), so it stays an INTERFACE component.
add_interface_component(
    COMPONENT_NAME serialization_arguments
    NAMESPACE      lux::cxx
)

component_include_directories(
    serialization_arguments
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

component_add_internal_dependencies(
    serialization_arguments
        lux::cxx::serialization
        lux::cxx::arguments
)

set(ENABLE_SERIALIZATION_TEST true CACHE BOOL "Enable serialization tests")
if(ENABLE_SERIALIZATION_TEST)
    add_subdirectory(test)
endif()
