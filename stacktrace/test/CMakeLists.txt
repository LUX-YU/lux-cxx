add_executable(stacktrace_test
    ${CMAKE_CURRENT_SOURCE_DIR}/stacktrace_test.cpp
)

target_link_libraries(stacktrace_test
    PRIVATE
        lux::cxx::stacktrace
)
