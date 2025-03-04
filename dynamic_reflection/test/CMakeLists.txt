get_filename_component(PARENT_DIR ../ ABSOLUTE)
set(DREF_PUBLIC_DIR ${PARENT_DIR}/include)

include_component_cmake_scripts(dynamic_reflection_runtime)

get_filename_component(DYNAMIC_REFLECTION_BINARAY_DIR ${CMAKE_CURRENT_BINARY_DIR}/.. ABSOLUTE)
message(STATUS "DYNAMIC_REFLECTION_BINARAY_DIR: ${DYNAMIC_REFLECTION_BINARAY_DIR}")
find_program(
	GENERATOR
	lux_meta_generator
	PATHS ${DYNAMIC_REFLECTION_BINARAY_DIR}
	NO_CACHE
	NO_DEFAULT_PATH
	NO_CMAKE_INSTALL_PREFIX
	NO_PACKAGE_ROOT_PATH
	NO_CMAKE_PATH
)
if(GENERATOR)
	message(STATUS "Found lux_meta_generator: ${GENERATOR}")
	add_static_meta_target(
		TARGET_NAME 		test_header2_meta
		SOURCE_FILE 		${CMAKE_CURRENT_SOURCE_DIR}/generate_static_meta_test.cpp
		METAS 				${CMAKE_CURRENT_SOURCE_DIR}/test_header2.hpp
		META_GEN_OUT_DIR 	${CMAKE_BINARY_DIR}/metagen
		META_GENERATOR 		${GENERATOR}
		COMPILE_COMMANDS 	${CMAKE_BINARY_DIR}/compile_commands.json
	)
	
	add_executable(
		generate_static_meta_test
		${CMAKE_CURRENT_SOURCE_DIR}/generate_static_meta_test.cpp
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
	
	add_dependencies(
		generate_static_meta_test
		test_header2_meta
	)
endif()

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

add_executable(
	serializer_test
	${CMAKE_CURRENT_SOURCE_DIR}/serializer_test.cpp
)

target_link_libraries(
	serializer_test
	PRIVATE
	lux::cxx::dynamic_reflection_parser
)
