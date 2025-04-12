get_filename_component(PARENT_DIR ../ ABSOLUTE)
set(DREF_PUBLIC_DIR ${PARENT_DIR}/include)

include_component_cmake_scripts(dynamic_reflection_runtime)

add_executable(
	meta_parse_test
	${CMAKE_CURRENT_SOURCE_DIR}/meta_parse_test.cpp
)

target_link_libraries(
	meta_parse_test
	PRIVATE
		lux::cxx::dynamic_reflection_parser
)

add_executable(
	meta_deserializer_test
	${CMAKE_CURRENT_SOURCE_DIR}/meta_deserializer_test.cpp
)

target_link_libraries(
	meta_deserializer_test
	PRIVATE
		lux::cxx::dynamic_reflection_runtime
)

get_filename_component(DYNAMIC_REFLECTION_BINARAY_DIR ${CMAKE_CURRENT_BINARY_DIR}/.. ABSOLUTE)
message(STATUS "DYNAMIC_REFLECTION_BINARAY_DIR: ${DYNAMIC_REFLECTION_BINARAY_DIR}")
find_program(
	LUX_GENERATOR
	lux_meta_generator
	PATHS ${DYNAMIC_REFLECTION_BINARAY_DIR}
	NO_CACHE
	NO_DEFAULT_PATH
	NO_CMAKE_INSTALL_PREFIX
	NO_PACKAGE_ROOT_PATH
	NO_CMAKE_PATH
)
if(LUX_GENERATOR)
	add_executable(
		generator_test
		${CMAKE_CURRENT_SOURCE_DIR}/generator_test.cpp
	)

	target_link_libraries(
		generator_test
		PRIVATE
			lux::cxx::dynamic_reflection_runtime
	)

	add_meta(
		NAME			test_meta
		GENERATOR		${LUX_GENERATOR}
		OUT_DIR			${CMAKE_BINARY_DIR}/metagen
		META_SUFFIX     "meta.hpp"
		MARKER          "marked"
		META_GENERATOR  ${GENERATOR}
		TEMPLATE		${TEST_TEMPLATE_DIR}/static_meta.template.inja
		TARGET_FILES	${CMAKE_CURRENT_SOURCE_DIR}/test_header.hpp
		ECHO
	)

	target_add_meta(
		TARGET			generator_test
		NAME			test_meta
		ALWAYS_REGENERATE
		ARGS_DONT_ADD_TO_SOURCE
		ECHO
	)

	add_dependencies(
		test_meta_gen
		lux_meta_generator
	)
endif()