if(NOT DEFINED GENERATOR OR NOT DEFINED CONFIG OR
   NOT DEFINED PASS_TEMPLATE OR NOT DEFINED FAIL_TEMPLATE OR
   NOT DEFINED PUBLISHED_OUTPUT OR NOT DEFINED TEST_CONFIG)
    message(FATAL_ERROR "generator validation failure test arguments are incomplete")
endif()

file(READ "${CONFIG}" _config)
string(REPLACE "${PASS_TEMPLATE}" "${FAIL_TEMPLATE}" _config "${_config}")
if(_config STREQUAL "")
    message(FATAL_ERROR "failed to prepare validation failure config")
endif()
file(WRITE "${TEST_CONFIG}" "${_config}")

file(SHA256 "${PUBLISHED_OUTPUT}" _before)
execute_process(
    COMMAND "${GENERATOR}" "${TEST_CONFIG}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
)
if(_result EQUAL 0)
    message(FATAL_ERROR "semantic validation failure unexpectedly succeeded")
endif()
if(NOT _stderr MATCHES "test_annotation_callbacks.hpp: ValidationFailureProbe: intentional semantic validation failure")
    message(FATAL_ERROR "semantic diagnostic lacks logical path/declaration: ${_stderr}")
endif()

file(SHA256 "${PUBLISHED_OUTPUT}" _after)
if(NOT _before STREQUAL _after)
    message(FATAL_ERROR "failed validation overwrote a previously published output")
endif()
