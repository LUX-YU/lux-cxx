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

	add_executable(
		generate_meta_test
		${CMAKE_CURRENT_SOURCE_DIR}/generate_meta_test.cpp
		${GENERATED_META_FILES}
	)
	
	target_link_libraries(
		generate_meta_test
		PRIVATE
		dynamic_reflection_runtime
	)
	
	target_include_directories(
		generate_meta_test
		PRIVATE  
		${CMAKE_BINARY_DIR}/metagen
		${CMAKE_CURRENT_SOURCE_DIR}
	)

	add_meta(
		NAME test_meta
		META_GEN_OUT_DIR     ${CMAKE_BINARY_DIR}/metagen
		META_GENERATOR       ${GENERATOR}
		TARGET_FILES
		    ${CMAKE_CURRENT_SOURCE_DIR}/test_header2.hpp
		ECHO
	)

	meta_add_files(
		test_meta
		TARGET_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_header.hpp
	)

	target_add_meta(
		TARGET  generate_meta_test
		NAME    test_meta
		ALWAYS_REGENERATE
		ECHO
	)

	add_dependencies(
		test_meta_gen
		lux_meta_generator
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
