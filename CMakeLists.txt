cmake_minimum_required(VERSION 3.18)
project(lux-cxx)

find_package(LUX-CMAKE-TOOLSET CONFIG REQUIRED)
include(${LUX_CMAKE_COMPONENT_TOOLSET_DIR}/add_component.cmake)
include(${LUX_CMAKE_COMPONENT_TOOLSET_DIR}/install_project.cmake)

add_subdirectory(compile_time)
add_subdirectory(subprogram)
add_subdirectory(visibility)

install_projects(
    PROJECT_NAME    lux-cxx
    VERSION         0.0.2
    NAMESPACE       lux::cxx
    COMPONENTS      visibility
                    subprogram
                    compile_time
)
