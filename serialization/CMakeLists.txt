# Serialization module (header-only core).
#
# The compile-time contract lives in include/lux/cxx/serialization/meta_info.hpp.
# Concrete meta_info<T> specialisations are produced by lux_meta_generator using
# the bundled templates/serializable.template.inja; downstream users drive it
# through cmake/lux_serialization.cmake (see component_add_cmake_scripts below).

# Selectable JSON parser backend for from_json (the streaming writer is always
# used for to_json and needs no library):
#   nlohmann (default) — header-only, already pulled in by inja; small/fast build.
#   simdjson           — SIMD parser, ~4-5x faster + far fewer allocations, but a
#                        large dependency (compiled straight in, hidden).
set(LUX_SERIALIZATION_JSON "nlohmann" CACHE STRING
    "from_json parser backend: 'nlohmann' (default) or 'simdjson'")
set_property(CACHE LUX_SERIALIZATION_JSON PROPERTY STRINGS nlohmann simdjson)
if(NOT LUX_SERIALIZATION_JSON MATCHES "^(nlohmann|simdjson)$")
    message(FATAL_ERROR "LUX_SERIALIZATION_JSON must be 'nlohmann' or 'simdjson', got '${LUX_SERIALIZATION_JSON}'")
endif()
message(STATUS "[serialization] from_json parser backend: ${LUX_SERIALIZATION_JSON}")

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
        lux::cxx::core
)

# Ship the codegen template + the turn-key CMake helpers so that consumers get
# lux_target_add_serialization() after find_package + include_component_cmake_scripts.
component_add_cmake_scripts(
    serialization
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/lux_serialization.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/serializable.template.inja
)

# JSON backend. to_json always uses a hand-rolled streaming writer (no library);
# the from_json PARSER is chosen by LUX_SERIALIZATION_JSON (see top of this file).
# Either dependency is hidden: nlohmann is header-only behind $<BUILD_INTERFACE:>;
# simdjson's amalgamation is compiled directly in (like tinyxml2). json.hpp exposes
# neither type, so the installed package and consumers stay dependency-free.
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

if(LUX_SERIALIZATION_JSON STREQUAL "simdjson")
    # simdjson is NOT vendored (it's ~10 MB). When this backend is selected we
    # resolve its single-file amalgamation (simdjson.h + simdjson.cpp):
    #   1. LUX_SIMDJSON_SOURCE_DIR  — a local copy, if you point at one;
    #   2. otherwise download the pinned release into the build tree (once, SHA-256
    #      verified). LUX_FETCH_DEPS=OFF forbids the download.
    # The .cpp is compiled straight into serialization_json (hidden, like tinyxml2)
    # so simdjson never reaches the public headers or the consumer.
    set(LUX_SIMDJSON_VERSION    "4.6.4" CACHE STRING "simdjson amalgamation version to fetch")
    set(LUX_SIMDJSON_SOURCE_DIR ""      CACHE PATH   "Local dir with simdjson.h + simdjson.cpp (skips download)")

    if(LUX_SIMDJSON_SOURCE_DIR)
        set(_sj_dir "${LUX_SIMDJSON_SOURCE_DIR}")
    else()
        set(_sj_dir "${CMAKE_BINARY_DIR}/_deps/simdjson-${LUX_SIMDJSON_VERSION}")
        foreach(_f simdjson.h simdjson.cpp)
            if(_f STREQUAL "simdjson.h")
                set(_sj_sha "fec0cb4559e25456789efb153b7e5eaa27b50ce2bc2c9d3b531ea2000928380a")
            else()
                set(_sj_sha "826660ed60cf467641ab2f93a0e1a2dacf603c444f76d835358c814bc2caba3a")
            endif()
            if(NOT EXISTS "${_sj_dir}/${_f}")
                if(DEFINED LUX_FETCH_DEPS AND NOT LUX_FETCH_DEPS)
                    message(FATAL_ERROR "[serialization] simdjson backend needs ${_f} but LUX_FETCH_DEPS=OFF.\n"
                        "  Point LUX_SIMDJSON_SOURCE_DIR at a local amalgamation, enable LUX_FETCH_DEPS, or use LUX_SERIALIZATION_JSON=nlohmann.")
                endif()
                message(STATUS "[serialization] fetching simdjson ${LUX_SIMDJSON_VERSION}: ${_f}")
                file(DOWNLOAD
                    "https://github.com/simdjson/simdjson/releases/download/v${LUX_SIMDJSON_VERSION}/${_f}"
                    "${_sj_dir}/${_f}"
                    EXPECTED_HASH "SHA256=${_sj_sha}"
                    TLS_VERIFY ON
                    STATUS _sj_dl)
                list(GET _sj_dl 0 _sj_code)
                if(NOT _sj_code EQUAL 0)
                    list(GET _sj_dl 1 _sj_msg)
                    file(REMOVE "${_sj_dir}/${_f}")
                    message(FATAL_ERROR "[serialization] failed to fetch simdjson ${_f}: ${_sj_msg}\n"
                        "  Point LUX_SIMDJSON_SOURCE_DIR at a local amalgamation, or use LUX_SERIALIZATION_JSON=nlohmann.")
                endif()
            endif()
        endforeach()
    endif()

    if(NOT EXISTS "${_sj_dir}/simdjson.cpp" OR NOT EXISTS "${_sj_dir}/simdjson.h")
        message(FATAL_ERROR "[serialization] simdjson amalgamation not found in '${_sj_dir}'.")
    endif()

    target_sources(serialization_json PRIVATE "${_sj_dir}/simdjson.cpp")
    target_include_directories(serialization_json PRIVATE "${_sj_dir}")
    target_compile_definitions(serialization_json PRIVATE LUX_SER_JSON_SIMDJSON)
else()
    # nlohmann is header-only; $<BUILD_INTERFACE:> keeps it out of the export.
    target_link_libraries(serialization_json PRIVATE $<BUILD_INTERFACE:lux::cxx::nlohmann_json>)
endif()

# XML backend. tinyxml2's single .cpp is compiled straight into serialization_xml,
# so the static archive is self-contained and the consumer needs no tinyxml2 target
# on its final link line (tinyxml2 is therefore never exported).
add_component(
    COMPONENT_NAME serialization_xml
    NAMESPACE      lux::cxx
    STATIC
    SOURCE_FILES   ${CMAKE_CURRENT_SOURCE_DIR}/src/xml.cpp
                   ${PROJECT_SOURCE_DIR}/third_party/tinyxml2/src/tinyxml2.cpp
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

# tinyxml2 headers are a build-only PRIVATE include (for xml.cpp and the bundled
# tinyxml2.cpp). PRIVATE => not exported, so nothing tinyxml2 reaches the consumer.
target_include_directories(serialization_xml PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party/tinyxml2/include)

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

set(ENABLE_SERIALIZATION_TEST ${LUX_CXX_BUILD_TESTS} CACHE BOOL "Enable serialization tests")
if(ENABLE_SERIALIZATION_TEST AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()
