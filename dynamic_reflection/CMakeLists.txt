# Add cmake module path for custom Find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find required packages
find_package(LibClang REQUIRED)
find_package(inja          CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

#[[==========================
    Dynamic Reflection Runtime
=============================]]
set(DREF_RUNTIME_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/MetaUnit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/MetaUnitSerializer.cpp
)

add_component(
    COMPONENT_NAME dynamic_reflection_runtime
    NAMESPACE      lux::cxx
    SOURCE_FILES   ${DREF_RUNTIME_SRCS}
)

component_include_directories(
    dynamic_reflection_runtime
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LUX_GENERATE_HEADER_DIR}
    INSTALL_TIME
        include
    PRIVATE
        pinclude
)

target_link_libraries(
    dynamic_reflection_runtime
    PUBLIC
        nlohmann_json::nlohmann_json
)

target_compile_definitions(
    dynamic_reflection_runtime
    PRIVATE
    LUX_CXX_LIBRARY
)

component_add_internal_dependencies(
    dynamic_reflection_runtime
        lux::cxx::algorithm
        lux::cxx::static_reflection
)

component_add_transitive_commands(
    dynamic_reflection_runtime
    "find_package(nlohmann_json REQUIRED)"
)

#[[==========================
    Dynamic Reflection Parser
=============================]]
set(DREF_PARSER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/CxxParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/CxxParserImpl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/parse_class_declaration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/parse_enumeration_declaration.cpp
)

add_component(
    COMPONENT_NAME dynamic_reflection_parser
    NAMESPACE      lux::cxx
    SOURCE_FILES   ${DREF_PARSER_SRCS}
)

component_include_directories(
    dynamic_reflection_parser
    INSTALL_TIME
        include
    PRIVATE
        pinclude
)

target_link_libraries(
    dynamic_reflection_parser
    PUBLIC
        LibClang::LibClang
)  

component_add_internal_dependencies(
    dynamic_reflection_parser
        lux::cxx::algorithm
        lux::cxx::dynamic_reflection_runtime
)

target_compile_definitions(
    dynamic_reflection_parser
    PRIVATE
    LUX_CXX_LIBRARY
)

component_add_transitive_commands(
    dynamic_reflection_parser
    "list(APPEND CMAKE_MODULE_PATH \"\${CMAKE_CURRENT_LIST_DIR}\")"
    "find_package(LibClang REQUIRED)"
)

#[[==========================
    Dynamic Reflection Generator
    
    This is just a demo program, you need to modify it according to the actual project, 
    but the corresponding cmake function is relatively common.
    
    You can consider using meta.cmake with your custom generator
=============================]]
set(DREF_GENERATOR_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generator/GeneratorHelper.cpp
)

add_component(
    COMPONENT_NAME dynamic_reflection_generator
    NAMESPACE      lux::cxx
    SOURCE_FILES   ${DREF_GENERATOR_SRCS}
)

component_include_directories(
    dynamic_reflection_generator
    INSTALL_TIME
        include
    PRIVATE
        pinclude
)

component_add_internal_dependencies(
    dynamic_reflection_generator
        lux::cxx::algorithm
        lux::cxx::dynamic_reflection_runtime
)

component_add_cmake_scripts(
	dynamic_reflection_generator
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/meta.cmake
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindLibClang.cmake
)

target_compile_definitions(
    dynamic_reflection_generator
    PRIVATE
    LUX_CXX_LIBRARY
)

#[[==========================
    Executable: lux_meta_generator
=============================]]
add_executable(
	lux_meta_generator
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generator/MetaGenerator.cpp
)

target_link_libraries(
	lux_meta_generator
	PRIVATE
	lux::cxx::dynamic_reflection_generator
    lux::cxx::dynamic_reflection_parser
    pantor::inja
)

target_include_directories(
    lux_meta_generator
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    pinclude
)

install(
	TARGETS lux_meta_generator
    RUNTIME DESTINATION bin
)

set(ENABLE_DYNAMIC_REFLECTION_TEST true BOOL "Enable dynamic reflection test")
if(ENABLE_DYNAMIC_REFLECTION_TEST)
    set(TEST_TEMPLATE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/templates)
    add_subdirectory(test)    
endif()
