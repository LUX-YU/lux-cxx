add_interface_component(
    COMPONENT_NAME                  archtype
    NAMESPACE                       lux::cxx
)

component_include_directories(
    archtype
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

# Context/TypeRegistry key resources by lux::cxx::type_hash/type_name
# (RTTI-free identity) from the compile_time component.
component_add_internal_dependencies(
    archtype
        lux::cxx::compile_time
)

set(ENABLE_ARCHTYPE_TEST ${LUX_CXX_BUILD_TESTS} CACHE BOOL "Enable archtype test")
if(ENABLE_ARCHTYPE_TEST AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()
