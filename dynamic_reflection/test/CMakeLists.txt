get_filename_component(PARENT_DIR ../ ABSOLUTE)
set(DREF_PUBLIC_DIR ${PARENT_DIR}/include)

include_component_cmake_scripts(dynamic_reflection_runtime)

# macro definition
set(LUX_DREF_TEST_GEN_FILE_PATH 
    ${CMAKE_BINARY_DIR}/gen/include/clang_config.h
)
set(LUX_CXX_INCLUDE_DIR_DEF
    ${DREF_PUBLIC_DIR}
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/clang_config.h.in
    ${LUX_DREF_TEST_GEN_FILE_PATH}
)

add_executable(
	dref_parse_test
	${CMAKE_CURRENT_SOURCE_DIR}/dref_parse_test.cpp
)

target_link_libraries(
	dref_parse_test
	PRIVATE
	lux::cxx::dynamic_reflection_parser
)

get_filename_component(DYNAMIC_REFLECTION_BINARAY_DIR ${CMAKE_CURRENT_BINARY_DIR}/.. ABSOLUTE)
find_program(
	GENERATOR
	lux_meta_generator
	HINTS ${DYNAMIC_REFLECTION_BINARAY_DIR}
)

if(GENERATOR)
generate_static_meta(
	METAS ${CMAKE_CURRENT_SOURCE_DIR}/test_header2.hpp
	META_GEN_OUT_DIR ${CMAKE_BINARY_DIR}/metagen
	META_GENERATOR ${GENERATOR}
)

add_executable(
	generate_static_meta_test
	generate_static_meta_test.cpp
)

target_link_libraries(
	generate_static_meta_test
	PRIVATE
	dynamic_reflection_runtime
)

target_include_directories(
	generate_static_meta_test
	PRIVATE
	${CMAKE_BINARY_DIR}/metagen
)
endif()

#[[
add_executable(
		dref_runtime_test
		${CMAKE_CURRENT_SOURCE_DIR}/dref_runtime_test.cpp
)

target_link_libraries(
		dref_runtime_test
		PRIVATE
		lux::cxx::dynamic_reflection_runtime
)
]]