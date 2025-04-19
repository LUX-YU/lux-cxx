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

set(ENABLE_COMPILE_TIME_TEST OFF CACHE BOOL "Enable compile time test")
if(ENABLE_COMPILE_TIME_TEST)
    add_subdirectory(test)
endif()