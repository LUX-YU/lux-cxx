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

set(ENABLE_MEMORY_TEST ${LUX_CXX_BUILD_TESTS} CACHE BOOL "Enable memory test")
if(ENABLE_MEMORY_TEST AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()

lux_add_header_self_containment(
    memory_new_header_self_containment
    lux::cxx::memory
    lux/cxx/memory/PmrResources.hpp
    lux/cxx/memory/SharedBytes.hpp
)
