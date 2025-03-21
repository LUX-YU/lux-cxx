# import target libclang
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/find_libclang.cmake)
find_package(inja      CONFIG REQUIRED)
set(DREF_PARSER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/CxxParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/CxxParserImpl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/parse_class_declaration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/parse_enumeration_declaration.cpp
)

# start constructing component dynamic_reflection_runtime
set(DREF_RUNTIME_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/MetaUnit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/RuntimeMeta.cpp
)

add_component(
    COMPONENT_NAME                  dynamic_reflection_runtime
    NAMESPACE                       lux::cxx
    SOURCE_FILES                    ${DREF_RUNTIME_SRCS}
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
        ${CLANG_INCLUDE_DIRS}
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

component_add_cmake_scripts(
	dynamic_reflection_runtime
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/target_add_meta.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/reflection_header_template.hpp.in
)

# start constructing component dynamic_reflection_parser
add_component(
    COMPONENT_NAME                  dynamic_reflection_parser
    NAMESPACE                       lux::cxx
    SOURCE_FILES                    ${DREF_PARSER_SRCS}
)

component_include_directories(
    dynamic_reflection_parser
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LUX_GENERATE_HEADER_DIR}
    INSTALL_TIME
        include
    PRIVATE
        pinclude
        ${CLANG_INCLUDE_DIRS}
)

target_link_libraries(
    dynamic_reflection_parser
    PUBLIC
        libclang
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
    "find_package(Clang REQUIRED)"
)

# start constructing component dynamic_reflection_generator
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_executable(
	lux_meta_generator
	${CMAKE_CURRENT_SOURCE_DIR}/src/generator/meta_generator.cpp
)

target_link_libraries(
	lux_meta_generator
	PRIVATE
	lux::cxx::dynamic_reflection_parser
	lux::cxx::dynamic_reflection_runtime
	lux::cxx::static_reflection
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
    add_subdirectory(test)    
endif()
