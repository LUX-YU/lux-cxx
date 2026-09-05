get_filename_component(PARENT_DIR ../ ABSOLUTE)
set(REFLECTION_PUBLIC_DIR ${PARENT_DIR}/include)

include_component_cmake_scripts(reflection_generator)

add_executable(compile_environment_test compile_environment_test.cpp)
target_link_libraries(compile_environment_test PRIVATE
    lux::cxx::reflection_generator lux::cxx::reflection_parser lux::cxx::nlohmann_json)
add_test(NAME compile_environment_test
    COMMAND compile_environment_test "${CMAKE_CURRENT_BINARY_DIR}/compile-environment-fixture")
set_tests_properties(compile_environment_test PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/..")

add_executable(
	meta_parse_test
	${CMAKE_CURRENT_SOURCE_DIR}/meta_parse_test.cpp
)

target_link_libraries(
	meta_parse_test
	PRIVATE
		lux::cxx::reflection_parser
)

add_executable(
	external_reflect_test
	${CMAKE_CURRENT_SOURCE_DIR}/external_reflect_test.cpp
)

target_link_libraries(
	external_reflect_test
	PRIVATE
		lux::cxx::reflection_parser
)

add_executable(
	meta_deserializer_test
	${CMAKE_CURRENT_SOURCE_DIR}/meta_deserializer_test.cpp
)

target_link_libraries(
	meta_deserializer_test
	PRIVATE
		lux::cxx::reflection_runtime
)

add_executable(
	extended_coverage_test
	${CMAKE_CURRENT_SOURCE_DIR}/extended_coverage_test.cpp
)

target_link_libraries(
	extended_coverage_test
	PRIVATE
		lux::cxx::reflection_parser
)

add_executable(
	meta_roundtrip_test
	${CMAKE_CURRENT_SOURCE_DIR}/meta_roundtrip_test.cpp
)

add_executable(
	meta_ir_test
	${CMAKE_CURRENT_SOURCE_DIR}/meta_ir_test.cpp
)

add_executable(
	meta_ir_fuzz_test
	${CMAKE_CURRENT_SOURCE_DIR}/meta_ir_fuzz_test.cpp
)

target_link_libraries(
	meta_ir_fuzz_test
	PRIVATE
		lux::cxx::reflection_runtime
)

target_link_libraries(
	meta_ir_test
	PRIVATE
		lux::cxx::reflection_runtime
)

get_filename_component(REFLECTION_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/.. ABSOLUTE)

foreach(_reflection_test
	meta_parse_test
	external_reflect_test
	meta_deserializer_test
	extended_coverage_test
	meta_roundtrip_test
	meta_ir_test
	meta_ir_fuzz_test
)
	add_test(NAME ${_reflection_test} COMMAND ${_reflection_test})
	# Parser/runtime DLLs and their copied vcpkg dependencies are emitted in the
	# reflection binary directory, not the per-test executable directory.
	set_tests_properties(
		${_reflection_test}
		PROPERTIES WORKING_DIRECTORY ${REFLECTION_BINARY_DIR}
	)
endforeach()

target_link_libraries(
	meta_roundtrip_test
	PRIVATE
		lux::cxx::reflection_parser
)

message(STATUS "REFLECTION_BINARY_DIR: ${REFLECTION_BINARY_DIR}")
find_program(
	LUX_GENERATOR
	lux_meta_generator
	PATHS ${REFLECTION_BINARY_DIR}
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
			lux::cxx::reflection_runtime
	)
	add_test(NAME generator_test COMMAND generator_test)
	set_tests_properties(
		generator_test
		PROPERTIES WORKING_DIRECTORY ${REFLECTION_BINARY_DIR}
	)
	set(codegen_input "${CMAKE_CURRENT_BINARY_DIR}/projection-input.json")
	file(GENERATE OUTPUT "${codegen_input}" CONTENT "{\"value\":73}\n")

	lux_add_codegen_job(
		NAME          test_meta_job
		GENERATOR     ${LUX_GENERATOR}
		MARKER        marked
		SOURCE_FILE   ${CMAKE_CURRENT_SOURCE_DIR}/generator_test.cpp
		TARGET_FILES  ${CMAKE_CURRENT_SOURCE_DIR}/test_header.hpp
		LOGICAL_PATHS test_header.hpp
	)
	lux_codegen_add_projection(
		JOB           test_meta_job
		NAME          static_meta
		TEMPLATE      ${TEST_TEMPLATE_DIR}/test_meta.template.inja
		OUTPUT_ROOT   ${CMAKE_BINARY_DIR}/metagen
		OUTPUT_SUFFIX .meta.hpp
		JSON_FIELD    "{\n  \"foo\":123,\n  \"bar\":[1,2,3]\n}"
		JSON_FILE     "external_data=${codegen_input}"
	)
	# The second projection intentionally reuses the same parse and IR.
	lux_codegen_add_projection(
		JOB           test_meta_job
		NAME          mirror_meta
		TEMPLATE      ${TEST_TEMPLATE_DIR}/test_meta.template.inja
		OUTPUT_ROOT   ${CMAKE_BINARY_DIR}/metagen
		OUTPUT_SUFFIX .mirror.hpp
	)
	lux_target_add_codegen(
		TARGET generator_test
		JOB test_meta_job
		DONT_ADD_TO_SOURCE
	)

	lux_add_codegen_job(
		NAME          test_annotation_job
		GENERATOR     ${LUX_GENERATOR}
		MARKER        marked
		SOURCE_FILE   ${CMAKE_CURRENT_SOURCE_DIR}/generator_test.cpp
		TARGET_FILES  ${CMAKE_CURRENT_SOURCE_DIR}/test_annotation_callbacks.hpp
		LOGICAL_PATHS test_annotation_callbacks.hpp
	)
	lux_codegen_add_projection(
		JOB           test_annotation_job
		NAME          annotation
		TEMPLATE      ${TEST_TEMPLATE_DIR}/annotation_callbacks_test.template.inja
		OUTPUT_ROOT   ${CMAKE_BINARY_DIR}/metagen
		OUTPUT_SUFFIX .anno.hpp
	)
	lux_codegen_add_validation(
		JOB           test_annotation_job
		NAME          annotation_semantics
		TEMPLATE      ${TEST_TEMPLATE_DIR}/annotation_callbacks_test.validation.inja
	)
	lux_target_add_codegen(
		TARGET generator_test
		JOB test_annotation_job
		DONT_ADD_TO_SOURCE
	)
	add_test(
		NAME generator_validation_failure
		COMMAND ${CMAKE_COMMAND}
			-DGENERATOR=${LUX_GENERATOR}
			-DCONFIG=${CMAKE_CURRENT_BINARY_DIR}/lux_codegen/test_annotation_job.json
			-DPASS_TEMPLATE=${TEST_TEMPLATE_DIR}/annotation_callbacks_test.validation.inja
			-DFAIL_TEMPLATE=${TEST_TEMPLATE_DIR}/validation_failure_test.validation.inja
			-DPUBLISHED_OUTPUT=${CMAKE_BINARY_DIR}/metagen/test_annotation_callbacks.anno.hpp
			-DTEST_CONFIG=${CMAKE_CURRENT_BINARY_DIR}/lux_codegen/test_annotation_failure.json
			-P ${CMAKE_CURRENT_SOURCE_DIR}/generator_validation_failure_test.cmake
	)

	lux_add_codegen_job(
		NAME          test_lineage_job
		GENERATOR     ${LUX_GENERATOR}
		MARKER        marked
		SOURCE_FILE   ${CMAKE_CURRENT_SOURCE_DIR}/generator_test.cpp
		TARGET_FILES  ${CMAKE_CURRENT_SOURCE_DIR}/template_layout_consumer.hpp
		LOGICAL_PATHS template_layout_consumer.hpp
	)
	lux_codegen_add_projection(
		JOB           test_lineage_job
		NAME          lineage
		TEMPLATE      ${TEST_TEMPLATE_DIR}/template_lineage_test.template.inja
		OUTPUT_ROOT   ${CMAKE_BINARY_DIR}/metagen
		OUTPUT_SUFFIX .lineage.hpp
	)
	lux_target_add_codegen(
		TARGET generator_test
		JOB test_lineage_job
		DONT_ADD_TO_SOURCE
	)

	add_dependencies(test_meta_job_generate lux_meta_generator)
	add_dependencies(test_annotation_job_generate lux_meta_generator)
	add_dependencies(test_lineage_job_generate lux_meta_generator)
endif()
