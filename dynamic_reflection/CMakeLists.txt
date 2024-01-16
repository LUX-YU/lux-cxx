# import target libclang
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/find_libclang_unix.cmake)

# see https://mustache.github.io/mustache.5.html
find_package(mstch CONFIG   REQUIRED)

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

add_component(
    COMPONENT_NAME                  dynamic_reflection
    NAMESPACE                       lux::cxx
    SOURCE_FILES                    ${DREF_SRCS}
    BUILD_TIME_EXPORT_INCLUDE_DIRS  ${CMAKE_CURRENT_SOURCE_DIR}/include
                                    ${LUX_GENERATE_HEADER_DIR}
    INSTALL_TIME_INCLUDE_PREFIX     include
    PRIVATE_INCLUDE_DIRS            pinclude
                                    ${CLANG_INCLUDE_DIRS}
    PUBLIC_LIBRARIES                libclang
                                    mstch::mstch
    INTERNAL_DEPENDENCIES           lux::cxx::lan_model
                                    lux::cxx::algorithm
    PRIVATE_DEFINITIONS             LUX_CXX_LIBRARY
    TRANSITIVE_PACKAGES_COMMANDS    "find_package(mstch CONFIG   REQUIRED)"
                                    "find_package(Clang          REQUIRED)"
)

add_executable(
	lux_reflection_gen
	${CMAKE_CURRENT_SOURCE_DIR}/src/generator/lux_df_gen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generator/MetaUnitSerializer.cpp
)

target_link_libraries(
	lux_reflection_gen
	PRIVATE
	lux::cxx::dynamic_reflection
	mstch::mstch
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
