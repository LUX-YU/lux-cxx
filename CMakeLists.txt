cmake_minimum_required(VERSION 3.18)
project(lux-cxx VERSION 0.1.0 LANGUAGES CXX)

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(WIN32) 
    set(CMAKE_DEBUG_POSTFIX d)
endif()

find_package(LUX-CMAKE-TOOLSET CONFIG REQUIRED)
generate_visibility_header(
    ENABLE_MACRO_NAME   LUX_CXX_LIBRARY
    PUBLIC_MACRO_NAME   LUX_CXX_PUBLIC
    GENERATE_FILE_PATH  lux/cxx/visibility.h
)

# When ON, header-only dependencies not found via find_package (inja, and the
# optional simdjson backend's amalgamation source) are auto-downloaded into the
# build tree instead of failing. libclang is NOT fetchable — it always needs the
# system (vcpkg / LLVM install).
option(LUX_FETCH_DEPS "Auto-download missing header-only deps (inja, simdjson source)" OFF)
option(LUX_CXX_BUILD_TESTS "Build lux-cxx module tests" ${PROJECT_IS_TOP_LEVEL})
include(CTest)

function(lux_add_header_self_containment TARGET_NAME LINK_TARGET)
    if(NOT LUX_CXX_BUILD_TESTS OR NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING)
        return()
    endif()

    set(_lux_header_sources)
    foreach(_lux_header IN LISTS ARGN)
        string(MAKE_C_IDENTIFIER "${_lux_header}" _lux_header_id)
        set(_lux_header_source
            "${CMAKE_CURRENT_BINARY_DIR}/header_self_containment/${_lux_header_id}.cpp"
        )
        file(GENERATE
            OUTPUT "${_lux_header_source}"
            CONTENT "#include <${_lux_header}>\n"
        )
        list(APPEND _lux_header_sources "${_lux_header_source}")
    endforeach()

    add_library(${TARGET_NAME} OBJECT ${_lux_header_sources})
    target_link_libraries(${TARGET_NAME} PRIVATE ${LINK_TARGET})
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)
    set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "header-self-containment")
endfunction()

add_subdirectory(third_party)
add_subdirectory(compile_time)
add_subdirectory(core)
add_subdirectory(container)
add_subdirectory(memory)
add_subdirectory(concurrent)
add_subdirectory(subprogram)
add_subdirectory(arguments)
add_subdirectory(algorithm)
add_subdirectory(binary)
add_subdirectory(time)
add_subdirectory(units)
add_subdirectory(diagnostic)
add_subdirectory(abi)
add_subdirectory(archtype)
add_subdirectory(event)

# stacktrace: std::stacktrace on C++23, a cpptrace-backed shim otherwise. Opt-in
# because the backport needs cpptrace (system or fetched via LUX_FETCH_DEPS).
option(ENABLE_STACKTRACE "Build the lux::cxx::stacktrace module" OFF)
if(ENABLE_STACKTRACE)
    add_subdirectory(stacktrace)
endif()

set(ENABLE_COMPONENTS
    core
    container
    memory
    concurrent
    subprogram 
    arguments
    compile_time
    algorithm
    binary
    time
    units
    diagnostic
    abi
    archtype
    event
    serialization
    serialization_json
    serialization_xml
    serialization_arguments
)

# stacktrace hides cpptrace inside its DLL (PRIVATE dep on a SHARED lib), so the
# export carries no third-party reference — safe to ship/find_package.
if(ENABLE_STACKTRACE)
    list(APPEND ENABLE_COMPONENTS stacktrace)
endif()

set(ENABLE_REFLECTION OFF CACHE BOOL "ENABLE REFLECTION SUPPORT")
if(ENABLE_REFLECTION)
    add_subdirectory(reflection)
    # Parser is host-only (libclang); cross builds ship runtime + generator
    # libs only — see reflection/CMakeLists.txt gate (2026-07-20).
    if(NOT CMAKE_CROSSCOMPILING)
        list(APPEND ENABLE_COMPONENTS reflection_parser)
    endif()
    list(APPEND ENABLE_COMPONENTS reflection_runtime)
    list(APPEND ENABLE_COMPONENTS reflection_generator)
endif()

add_subdirectory(serialization)

install_components(
    PROJECT_NAME    lux-cxx
    VERSION         ${PROJECT_VERSION}
    NAMESPACE       lux::cxx
    COMPONENTS      ${ENABLE_COMPONENTS}
)

# Install a reproducible SDK identity for consumers that pin installed
# dependencies instead of reaching back into this source tree.
find_package(Git QUIET)
set(LUX_CXX_SDK_REVISION "unknown")
set(LUX_CXX_SDK_DIRTY false)
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE LUX_CXX_SDK_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} status --porcelain
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE _lux_cxx_sdk_status
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT _lux_cxx_sdk_status STREQUAL "")
        set(LUX_CXX_SDK_DIRTY true)
    endif()
endif()
if(ANDROID)
    set(LUX_CXX_SDK_TARGET_TRIPLE "android")
elseif(WIN32)
    set(LUX_CXX_SDK_TARGET_TRIPLE "windows-${CMAKE_SYSTEM_PROCESSOR}")
elseif(UNIX)
    set(LUX_CXX_SDK_TARGET_TRIPLE "linux-${CMAKE_SYSTEM_PROCESSOR}")
else()
    set(LUX_CXX_SDK_TARGET_TRIPLE "unknown")
endif()
math(EXPR LUX_CXX_SDK_POINTER_BITS "${CMAKE_SIZEOF_VOID_P} * 8")
include(TestBigEndian)
test_big_endian(_lux_cxx_sdk_big_endian)
if(_lux_cxx_sdk_big_endian)
    set(LUX_CXX_SDK_ENDIAN "big")
else()
    set(LUX_CXX_SDK_ENDIAN "little")
endif()
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/lux-cxx-sdk.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/lux-cxx-sdk.json
    @ONLY
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/lux-cxx-sdk.json
    DESTINATION share/lux-cxx
)

option(
    LUX_CXX_BUILD_INSTALL_CONSUMER_TEST
    "Verify an installed lux-cxx package with find_package"
    ${PROJECT_IS_TOP_LEVEL}
)
if(
    LUX_CXX_BUILD_INSTALL_CONSUMER_TEST
    AND LUX_CXX_BUILD_TESTS
    AND BUILD_TESTING
    AND NOT CMAKE_CROSSCOMPILING
)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/install-consumer-test.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/install-consumer-test.cmake
        @ONLY
    )
    add_test(
        NAME install_consumer_test
        COMMAND ${CMAKE_COMMAND}
            -P ${CMAKE_CURRENT_BINARY_DIR}/install-consumer-test.cmake
    )
    set_tests_properties(
        install_consumer_test
        PROPERTIES
            LABELS "install;consumer"
            TIMEOUT 600
    )
endif()

# ──────────────────────────────────────────────────────────────────────────────
# Packaging (CPack). One lux-cxx package: headers + static libs + CMake config
# (plus the reflection shared libs and lux_meta_generator when ENABLE_REFLECTION=ON).
#
# install_components() uses ABSOLUTE install destinations rooted at
# CMAKE_INSTALL_PREFIX, so configure with -DCMAKE_INSTALL_PREFIX=/usr, then
# `cpack -G DEB`. lux_configure_packaging comes from lux-cmake-toolset (found above).
# The build-time dependency on the toolset's CMake macros can't be inferred from
# binaries, so it is declared explicitly; SHLIBDEPS covers the rest (e.g. libclang).
# ──────────────────────────────────────────────────────────────────────────────
lux_configure_packaging(
    NAME     lux-cxx
    VERSION  ${PROJECT_VERSION}
    SUMMARY  "LUX core C++20 library (containers, compile-time, serialization, reflection)"
    VENDOR   "LUX-YU"
    CONTACT  "LUX-YU <chenhui.lux.yu@outlook.com>"
    HOMEPAGE "https://github.com/LUX-YU/lux-cxx"
    DEB_DEPENDS  "lux-cmake-toolset (>= 0.5.0)"
    RPM_REQUIRES "lux-cmake-toolset >= 0.5.0"
)
