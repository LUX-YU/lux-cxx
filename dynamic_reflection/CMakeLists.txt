find_package(Clang          QUIET)
if(NOT Clang_FOUND)
    if(UNIX)
        include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/find_libclang_unix.cmake)
    endif(UNIX)
endif()

# see https://mustache.github.io/mustache.5.html
find_package(mstch CONFIG   REQUIRED)
find_package(nameof CONFIG  REQUIRED)

set(DREF_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/CxxParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/CxxParserImpl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/ParserResult.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/ParserResultImpl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/Class.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/Method.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/Function.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/Instance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/Enumeration.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/MetaUnit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/BasicMetaUnit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/MetaRegistry.cpp
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
                                    nameof::nameof
                                    mstch::mstch
    PRIVATE_DEFINITIONS             LUX_CXX_LIBRARY
    TRANSITIVE_PACKAGES_COMMANDS    "find_package(mstch CONFIG   REQUIRED)"
                                    "find_package(Clang          REQUIRED)"
                                    "find_package(nameof CONFIG  REQUIRED)"
)

add_executable(
	lux_reflection_gen
	${CMAKE_CURRENT_SOURCE_DIR}/src/generator/lux_df_gen.cpp
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
