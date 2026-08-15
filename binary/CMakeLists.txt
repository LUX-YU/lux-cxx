add_interface_component(
    COMPONENT_NAME binary
    NAMESPACE      lux::cxx
)

component_include_directories(
    binary
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

component_add_internal_dependencies(
    binary
        lux::cxx::core
)

if(LUX_CXX_BUILD_TESTS AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()

lux_add_header_self_containment(
    binary_header_self_containment
    lux::cxx::binary
    lux/cxx/binary/Binary.hpp
    lux/cxx/binary/BinaryError.hpp
    lux/cxx/binary/BinaryReader.hpp
    lux/cxx/binary/BinaryWriter.hpp
    lux/cxx/binary/ScalarSchema.hpp
)
