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

add_subdirectory(container)
add_subdirectory(memory)
add_subdirectory(concurrent)
add_subdirectory(static_reflection)
add_subdirectory(compile_time)
add_subdirectory(subprogram)
add_subdirectory(arguments)
add_subdirectory(algorithm)
add_subdirectory(archtype)

set(ENABLE_COMPONENTS 
    container
    memory
    concurrent
    subprogram 
    arguments
    compile_time
    algorithm
    archtype
    static_reflection
)

set(ENABLE_DYNAMIC_REFLECTION OFF CACHE BOOL "ENABLE REFLECTION SUPPORT")
if(ENABLE_DYNAMIC_REFLECTION)
    add_subdirectory(dynamic_reflection)
    list(APPEND ENABLE_COMPONENTS dynamic_reflection_parser)
    list(APPEND ENABLE_COMPONENTS dynamic_reflection_runtime)
    list(APPEND ENABLE_COMPONENTS dynamic_reflection_generator)
endif()

install_components(
    PROJECT_NAME    lux-cxx
    VERSION         0.0.2
    NAMESPACE       lux::cxx
    COMPONENTS      ${ENABLE_COMPONENTS}
)
