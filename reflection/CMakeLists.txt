# Add cmake module path for custom Find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find required packages.
# LibClang is HOST-ONLY tooling: it feeds the parser and the meta-generator
# executable. Cross builds (e.g. arm64-android) ship only reflection_runtime +
# reflection_generator (neither links libclang) and consume the generator EXE
# from a host install via meta.cmake's find_program path — so the parser, the
# executable and this find are all gated off when cross-compiling (2026-07-20).
if(NOT CMAKE_CROSSCOMPILING)
    find_package(LibClang REQUIRED)
endif()

# inja drives the generator's templating (header-only; needs nlohmann/json.hpp,
# which lux::cxx::nlohmann_json provides). Prefer a system/vcpkg package; with
# -DLUX_FETCH_DEPS=ON, fetch its single header when absent. (libclang above is NOT
# fetchable — it always needs the system.)
find_package(inja CONFIG QUIET)
if(NOT TARGET pantor::inja)
    if(LUX_FETCH_DEPS)
        set(LUX_INJA_VERSION "3.4.0" CACHE STRING "inja single-header version to fetch")
        set(LUX_INJA_SHA256  "8022ce5a782e911598accc42311ac44a0b9ccd9ad5995f439c828fb534974543"
            CACHE STRING "SHA-256 of the fetched inja.hpp (empty to skip verification)")
        set(_inja_inc "${CMAKE_BINARY_DIR}/_deps/inja-${LUX_INJA_VERSION}/include")
        if(NOT EXISTS "${_inja_inc}/inja/inja.hpp")
            set(_inja_hash "")
            if(LUX_INJA_SHA256)
                set(_inja_hash EXPECTED_HASH "SHA256=${LUX_INJA_SHA256}")
            endif()
            message(STATUS "[reflection] fetching inja ${LUX_INJA_VERSION} (single header)")
            file(DOWNLOAD
                "https://raw.githubusercontent.com/pantor/inja/v${LUX_INJA_VERSION}/single_include/inja/inja.hpp"
                "${_inja_inc}/inja/inja.hpp"
                ${_inja_hash}
                TLS_VERIFY ON
                STATUS _inja_dl)
            list(GET _inja_dl 0 _inja_code)
            if(NOT _inja_code EQUAL 0)
                list(GET _inja_dl 1 _inja_msg)
                file(REMOVE "${_inja_inc}/inja/inja.hpp")
                message(FATAL_ERROR "[reflection] failed to fetch inja: ${_inja_msg}")
            endif()
        endif()
        add_library(lux_inja_fetched INTERFACE)
        target_include_directories(lux_inja_fetched INTERFACE "${_inja_inc}")
        target_link_libraries(lux_inja_fetched INTERFACE lux::cxx::nlohmann_json)
        add_library(pantor::inja ALIAS lux_inja_fetched)
        message(STATUS "[reflection] inja: fetched single header ${LUX_INJA_VERSION}")
    else()
        message(FATAL_ERROR
            "[reflection] 'inja' not found. Install it (e.g. vcpkg) or configure with "
            "-DLUX_FETCH_DEPS=ON to download its single header automatically.")
    endif()
else()
    message(STATUS "[reflection] inja: found system package")
endif()

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

# nlohmann_json is header-only and used only in MetaUnitSerializer.cpp, so it is
# fully compiled in. $<BUILD_INTERFACE:> keeps it out of the installed export.
target_link_libraries(reflection_runtime PRIVATE $<BUILD_INTERFACE:lux::cxx::nlohmann_json>)

#[[==========================
    Reflection Parser (host-only: hard libclang dependency — see gate above)
=============================]]
if(NOT CMAKE_CROSSCOMPILING)
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
endif() # NOT CMAKE_CROSSCOMPILING — reflection_parser

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

target_link_libraries(reflection_generator PRIVATE $<BUILD_INTERFACE:lux::cxx::nlohmann_json>)

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
    Executable: lux_meta_generator (host-only: links the parser → libclang)
=============================]]
if(NOT CMAKE_CROSSCOMPILING)
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

# In-tree alias so this build can use the same namespaced name consumers get.
# Downstream, meta.cmake synthesises an IMPORTED lux::cxx::lux_meta_generator that
# points at the installed bin/ executable, so $<TARGET_FILE:lux::cxx::lux_meta_generator>
# works identically in-tree and after find_package().
add_executable(lux::cxx::lux_meta_generator ALIAS lux_meta_generator)
endif() # NOT CMAKE_CROSSCOMPILING — lux_meta_generator

option(ENABLE_REFLECTION_TEST "Enable reflection test" ON)
if(ENABLE_REFLECTION_TEST AND NOT CMAKE_CROSSCOMPILING)
    set(TEST_TEMPLATE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/templates)
    add_subdirectory(test)    
endif()
