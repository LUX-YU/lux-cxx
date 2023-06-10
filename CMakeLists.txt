cmake_minimum_required(VERSION 3.18)
project(lux-cxx)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LUX-CMAKE-TOOLSET CONFIG REQUIRED)
include(${LUX_CMAKE_COMPONENT_TOOLSET_DIR}/add_component.cmake)
include(${LUX_CMAKE_COMPONENT_TOOLSET_DIR}/install_project.cmake)

add_subdirectory(compile_time)
add_subdirectory(subprogram)
add_subdirectory(visibility)

set(ENABLE_COMPONENTS visibility subprogram compile_time)

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
