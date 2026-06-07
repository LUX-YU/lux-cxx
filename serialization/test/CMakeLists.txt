# Serialization codegen test.
#
# Generating meta_info<T> requires the reflection generator toolchain
# (lux_meta_generator + add_meta/target_add_meta). When ENABLE_REFLECTION is OFF
# those are unavailable, so the codegen test is skipped.
if(NOT TARGET lux_meta_generator OR NOT TARGET lux::cxx::reflection_generator)
    message(STATUS "[serialization/test] reflection generator toolchain unavailable "
                   "(ENABLE_REFLECTION=OFF?); skipping serialization codegen test.")
    return()
endif()

# Bring add_meta()/target_add_meta() and lux_target_add_serialization() into scope.
include_component_cmake_scripts(reflection_generator)
include_component_cmake_scripts(serialization)

add_executable(serialization_test
    ${CMAKE_CURRENT_SOURCE_DIR}/serialization_test.cpp
)

target_link_libraries(serialization_test
    PRIVATE
        lux::cxx::serialization
        lux::cxx::reflection_runtime   # for <lux/cxx/reflection/runtime/Marker.hpp>
)

# Generate test_serialize.serialize.hpp from the annotated header and attach it.
lux_target_add_serialization(
    TARGET    serialization_test
    HEADERS   ${CMAKE_CURRENT_SOURCE_DIR}/test_serialize.hpp
    GENERATOR $<TARGET_FILE:lux_meta_generator>
    ALWAYS_REGENERATE
)

#[[==========================  JSON backend round-trip  ==========================]]
add_executable(json_test
    ${CMAKE_CURRENT_SOURCE_DIR}/json_test.cpp
)

target_link_libraries(json_test
    PRIVATE
        lux::cxx::serialization_json
        lux::cxx::reflection_runtime   # for <lux/cxx/reflection/runtime/Marker.hpp>
)

lux_target_add_serialization(
    TARGET    json_test
    HEADERS   ${CMAKE_CURRENT_SOURCE_DIR}/json_types.hpp
    GENERATOR $<TARGET_FILE:lux_meta_generator>
    ALWAYS_REGENERATE
)

#[[==========================  XML backend round-trip  ===========================]]
add_executable(xml_test
    ${CMAKE_CURRENT_SOURCE_DIR}/xml_test.cpp
)

target_link_libraries(xml_test
    PRIVATE
        lux::cxx::serialization_xml
        lux::cxx::reflection_runtime   # for <lux/cxx/reflection/runtime/Marker.hpp>
)

lux_target_add_serialization(
    TARGET    xml_test
    HEADERS   ${CMAKE_CURRENT_SOURCE_DIR}/xml_types.hpp
    GENERATOR $<TARGET_FILE:lux_meta_generator>
    ALWAYS_REGENERATE
)

#[[=================  External (non-intrusive) JSON round-trip  ==================]]
add_executable(external_json_test
    ${CMAKE_CURRENT_SOURCE_DIR}/external_json_test.cpp
)

target_link_libraries(external_json_test
    PRIVATE
        lux::cxx::serialization_json
        lux::cxx::reflection_runtime   # for <lux/cxx/reflection/runtime/Marker.hpp>
)

lux_target_add_serialization(
    TARGET    external_json_test
    HEADERS   ${CMAKE_CURRENT_SOURCE_DIR}/ext_json_register.hpp
    GENERATOR $<TARGET_FILE:lux_meta_generator>
    ALWAYS_REGENERATE
)

#[[==================  External (non-intrusive) XML round-trip  ==================]]
add_executable(external_xml_test
    ${CMAKE_CURRENT_SOURCE_DIR}/external_xml_test.cpp
)

target_link_libraries(external_xml_test
    PRIVATE
        lux::cxx::serialization_xml
        lux::cxx::reflection_runtime   # for <lux/cxx/reflection/runtime/Marker.hpp>
)

lux_target_add_serialization(
    TARGET    external_xml_test
    HEADERS   ${CMAKE_CURRENT_SOURCE_DIR}/ext_xml_register.hpp
    GENERATOR $<TARGET_FILE:lux_meta_generator>
    ALWAYS_REGENERATE
)

#[[========================  Arguments backend (CLI <-> struct)  ==================]]
add_executable(serialization_arguments_test
    ${CMAKE_CURRENT_SOURCE_DIR}/arguments_test.cpp
)

target_link_libraries(serialization_arguments_test
    PRIVATE
        lux::cxx::serialization_arguments
        lux::cxx::reflection_runtime   # for <lux/cxx/reflection/runtime/Marker.hpp>
)

lux_target_add_serialization(
    TARGET    serialization_arguments_test
    HEADERS   ${CMAKE_CURRENT_SOURCE_DIR}/args_types.hpp
    GENERATOR $<TARGET_FILE:lux_meta_generator>
    ALWAYS_REGENERATE
)
