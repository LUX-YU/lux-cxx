cmake_minimum_required(VERSION 3.18)
project(lux-cxx)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
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

add_subdirectory(third_party)
add_subdirectory(container)
add_subdirectory(memory)
add_subdirectory(concurrent)
add_subdirectory(compile_time)
add_subdirectory(subprogram)
add_subdirectory(arguments)
add_subdirectory(algorithm)
add_subdirectory(archtype)
add_subdirectory(event)

# stacktrace: std::stacktrace on C++23, a cpptrace-backed shim otherwise. Opt-in
# because the backport needs cpptrace (system or fetched via LUX_FETCH_DEPS).
option(ENABLE_STACKTRACE "Build the lux::cxx::stacktrace module" OFF)
if(ENABLE_STACKTRACE)
    add_subdirectory(stacktrace)
endif()

set(ENABLE_COMPONENTS 
    container
    memory
    concurrent
    subprogram 
    arguments
    compile_time
    algorithm
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
    list(APPEND ENABLE_COMPONENTS reflection_parser)
    list(APPEND ENABLE_COMPONENTS reflection_runtime)
    list(APPEND ENABLE_COMPONENTS reflection_generator)
endif()

add_subdirectory(serialization)

install_components(
    PROJECT_NAME    lux-cxx
    VERSION         0.0.2
    NAMESPACE       lux::cxx
    COMPONENTS      ${ENABLE_COMPONENTS}
)

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
    VERSION  0.0.2
    SUMMARY  "LUX core C++20 library (containers, compile-time, serialization, reflection)"
    VENDOR   "LUX-YU"
    CONTACT  "LUX-YU <chenhui.lux.yu@outlook.com>"
    HOMEPAGE "https://github.com/LUX-YU/lux-cxx"
    DEB_DEPENDS  "lux-cmake-toolset (>= 0.5.0)"
    RPM_REQUIRES "lux-cmake-toolset >= 0.5.0"
)
