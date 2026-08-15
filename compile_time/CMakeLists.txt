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

set(ENABLE_COMPILE_TIME_TEST ${LUX_CXX_BUILD_TESTS} CACHE BOOL "Enable compile time test")
if(ENABLE_COMPILE_TIME_TEST AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()

lux_add_header_self_containment(
    compile_time_new_header_self_containment
    lux::cxx::compile_time
    lux/cxx/compile_time/expected.hpp
    lux/cxx/compile_time/TypeToken.hpp
)
