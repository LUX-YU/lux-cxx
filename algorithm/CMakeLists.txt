add_interface_component(
    COMPONENT_NAME                  algorithm
    NAMESPACE                       lux::cxx
)

component_include_directories(
    algorithm
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

component_add_internal_dependencies(
    algorithm
        lux::cxx::compile_time
)

set(ENABLE_ALGORITHM_TEST ${LUX_CXX_BUILD_TESTS} CACHE BOOL "Enable algorithm tests")
if(ENABLE_ALGORITHM_TEST AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()

lux_add_header_self_containment(
    algorithm_new_header_self_containment
    lux::cxx::algorithm
    lux/cxx/algorithm/ContentId.hpp
    lux/cxx/algorithm/sha256.hpp
)
