add_interface_component(
    COMPONENT_NAME                  memory
    NAMESPACE                       lux::cxx
)

component_include_directories(
    memory
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

set(ENABLE_MEMORY_TEST OFF CACHE BOOL "Enable memory test")
if(ENABLE_MEMORY_TEST)
    add_subdirectory(test)
endif()
