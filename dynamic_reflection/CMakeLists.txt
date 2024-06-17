# import target libclang
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/find_libclang_unix.cmake)

# see https://mustache.github.io/mustache.5.html
find_package(mstch CONFIG)
if(mstch_FOUND)
    get_target_property(MSTCH_INCLUDE_DIRS mstch::mstch INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT MSTCH_INCLUDE_DIRS)
        message("mstch::mstch INTERFACE_INCLUDE_DIRECTORIES not found")
    endif()
    find_path(MSTCH_INCLUDE_DIRS mstch/mstch.hpp REQUIRED)
    set_target_properties(mstch::mstch PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${MSTCH_INCLUDE_DIRS}")
elseif()
    find_path(MSTCH_INCLUDE_DIRS mstch/mstch.hpp REQUIRED)
    find_library(MSTCH_LIBRARIES mstch REQUIRED)

    add_library(mstch INTERFACE)
    target_include_directories(mstch INTERFACE ${MSTCH_INCLUDE_DIRS})
    target_link_libraries(mstch INTERFACE ${MSTCH_LIBRARIES})
    add_library(mstch::mstch ALIAS mstch)
endif()

set(DREF_SRCS 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/MetaUnit.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/CxxParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/CxxParserImpl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/parse_class_declaration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/parse_function_declaration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/parse_enumeration_declaration.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/parse_type_uncertain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/parse_class_type.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/parse_function_type.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/parse_enumeration_type.cpp
)

# start constructing component dynamic_reflection
add_component(
    COMPONENT_NAME                  dynamic_reflection
    NAMESPACE                       lux::cxx
    SOURCE_FILES                    ${DREF_SRCS}
)

component_include_directories(
    dynamic_reflection
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
    dynamic_reflection
    PUBLIC
        libclang
        mstch::mstch
)

component_add_internal_dependencies(
    dynamic_reflection
        lux::cxx::lan_model
        lux::cxx::algorithm
)

target_compile_definitions(
    dynamic_reflection
    PRIVATE
    LUX_CXX_LIBRARY
)

component_add_transitive_commands(
    dynamic_reflection
    "find_package(Clang REQUIRED)"
    "find_package(mstch CONFIG   REQUIRED)"
)

# stop constructing component dynamic_reflection

add_executable(
	lux_reflection_gen
	${CMAKE_CURRENT_SOURCE_DIR}/src/generator/lux_df_gen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generator/MetaUnitSerializer.cpp
)

target_link_libraries(
	lux_reflection_gen
	PRIVATE
	lux::cxx::dynamic_reflection
)

target_include_directories(
    lux_reflection_gen
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    pinclude
)

set(ENABLE_DYNAMIC_REFLECTION_TEST true BOOL "Enable dynamic reflection test")
if(ENABLE_DYNAMIC_REFLECTION_TEST)
    add_subdirectory(test)    
endif()
