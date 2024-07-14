add_interface_component(
    COMPONENT_NAME                  arguments
    NAMESPACE                       lux::cxx
)

component_include_directories(
    arguments
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

component_add_internal_dependencies(
    arguments
    lux::cxx::compile_time
    lux::cxx::container
)

set(ENABLE_ARGUMENTS_TEST ON CACHE BOOL "ENABLE_ARGUMENTS_TEST")
if(ENABLE_ARGUMENTS_TEST)
	add_subdirectory(test)
endif()
