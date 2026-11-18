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
)

target_compile_features(arguments INTERFACE cxx_std_20)

component_add_assets(
    arguments
    licenses
    ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
)

lux_add_header_self_containment(
    arguments_header_self_containment
    lux::cxx::arguments
    lux/cxx/arguments/Arguments.hpp
    lux/cxx/arguments/ContainerTraits.hpp
    lux/cxx/arguments/DirectBinding.hpp
    lux/cxx/arguments/Help.hpp
    lux/cxx/arguments/Option.hpp
    lux/cxx/arguments/Sources.hpp
    lux/cxx/arguments/StructuredValue.hpp
    lux/cxx/arguments/TypedOptions.hpp
    lux/cxx/arguments/Value.hpp
)

set(ENABLE_ARGUMENTS_TEST ${LUX_CXX_BUILD_TESTS} CACHE BOOL "ENABLE_ARGUMENTS_TEST")
if(ENABLE_ARGUMENTS_TEST AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()
