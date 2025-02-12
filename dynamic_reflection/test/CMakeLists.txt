get_filename_component(PARENT_DIR ../ ABSOLUTE)
set(DREF_PUBLIC_DIR ${PARENT_DIR}/include)

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
		dref_runtime_test
		${CMAKE_CURRENT_SOURCE_DIR}/dref_runtime_test.cpp
)

target_link_libraries(
		dref_runtime_test
		PRIVATE
		lux::cxx::dynamic_reflection_runtime
)

add_executable(
		meta_unit_serializetion_test
		${CMAKE_CURRENT_SOURCE_DIR}/meta_unit_serializetion_test.cpp
)

target_link_libraries(
		meta_unit_serializetion_test
		PRIVATE
		lux::cxx::dynamic_reflection_parser
)
