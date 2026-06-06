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

set(ENABLE_COMPONENTS 
    nlohmann_json
    tinyxml2
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
