cmake_minimum_required(VERSION 3.18)
project(lux-cxx)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LUX-CMAKE-TOOLSET CONFIG REQUIRED)
generate_visibility_header(
    ENABLE_MACRO_NAME   LUX_CXX_LIBRARY
    PUBLIC_MACRO_NAME   LUX_CXX_PUBLIC
    GENERATE_FILE_PATH  lux/cxx/visibility.h
)

add_subdirectory(compile_time)
add_subdirectory(subprogram)

set(ENABLE_COMPONENTS subprogram compile_time)

set(ENABLE_REFLECTION OFF CACHE BOOL "ENABLE REFLECTION SUPPORT")
if(ENABLE_REFLECTION)
add_subdirectory(reflection)
list(APPEND ENABLE_COMPONENTS reflection)
endif()

install_projects(
    PROJECT_NAME    lux-cxx
    VERSION         0.0.2
    NAMESPACE       lux::cxx
    COMPONENTS      ${ENABLE_COMPONENTS}
)
