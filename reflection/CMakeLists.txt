# Add cmake module path for custom Find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find required packages
find_package(LibClang REQUIRED)
find_package(inja          CONFIG REQUIRED)

#[[==========================
    Reflection Runtime
=============================]]
set(REFLECTION_RUNTIME_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/MetaUnit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/MetaUnitSerializer.cpp
)

add_component(
    COMPONENT_NAME reflection_runtime
    NAMESPACE      lux::cxx
    SOURCE_FILES   ${REFLECTION_RUNTIME_SRCS}
)

component_include_directories(
    reflection_runtime
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LUX_GENERATE_HEADER_DIR}
    INSTALL_TIME
        include
    PRIVATE
        pinclude
)

target_compile_definitions(
    reflection_runtime
    PRIVATE
    LUX_CXX_LIBRARY
)

component_add_internal_dependencies(
    reflection_runtime
        lux::cxx::algorithm
        lux::cxx::compile_time
)

# nlohmann_json is an implementation detail (MetaUnitSerializer.cpp) — PRIVATE, never exported.
target_link_libraries(reflection_runtime PRIVATE lux::cxx::nlohmann_json)

#[[==========================
    Reflection Parser
=============================]]
set(REFLECTION_PARSER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/CxxParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/CxxParserImpl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/parse_class_declaration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/parse_enumeration_declaration.cpp
)

add_component(
    COMPONENT_NAME reflection_parser
    NAMESPACE      lux::cxx
    SOURCE_FILES   ${REFLECTION_PARSER_SRCS}
)

component_include_directories(
    reflection_parser
    INSTALL_TIME
        include
    PRIVATE
        pinclude
)

target_link_libraries(
    reflection_parser
    PUBLIC
        LibClang::LibClang
)  

component_add_internal_dependencies(
    reflection_parser
        lux::cxx::algorithm
        lux::cxx::reflection_runtime
)

target_compile_definitions(
    reflection_parser
    PRIVATE
    LUX_CXX_LIBRARY
)

# Consumers that link reflection_parser pull in LibClang::LibClang transitively,
# so the import must be able to find_package(LibClang). Ship FindLibClang.cmake
# with the parser and point CMAKE_MODULE_PATH at its installed cmake_scripts/ dir.
component_add_cmake_scripts(
    reflection_parser
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindLibClang.cmake
)

component_add_transitive_commands(
    reflection_parser
    "list(APPEND CMAKE_MODULE_PATH \"\${CMAKE_CURRENT_LIST_DIR}/cmake_scripts\")"
    "find_package(LibClang REQUIRED)"
)

#[[==========================
    Reflection Generator
    
    This is just a demo program, you need to modify it according to the actual project, 
    but the corresponding cmake function is relatively common.
    
    You can consider using meta.cmake with your custom generator
=============================]]
set(REFLECTION_GENERATOR_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generator/GeneratorHelper.cpp
)

add_component(
    COMPONENT_NAME reflection_generator
    NAMESPACE      lux::cxx
    SOURCE_FILES   ${REFLECTION_GENERATOR_SRCS}
)

component_include_directories(
    reflection_generator
    INSTALL_TIME
        include
    PRIVATE
        pinclude
)

component_add_internal_dependencies(
    reflection_generator
        lux::cxx::algorithm
        lux::cxx::reflection_runtime
)

target_link_libraries(reflection_generator PRIVATE lux::cxx::nlohmann_json)

component_add_cmake_scripts(
	reflection_generator
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/meta.cmake
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindLibClang.cmake
)

target_compile_definitions(
    reflection_generator
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
	lux::cxx::reflection_generator
    lux::cxx::reflection_parser
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

option(ENABLE_REFLECTION_TEST "Enable reflection test" ON)
if(ENABLE_REFLECTION_TEST)
    set(TEST_TEMPLATE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/templates)
    add_subdirectory(test)    
endif()
