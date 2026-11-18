include(cmake/Dependencies.cmake)
include(cmake/TestOptions.cmake)

add_interface_component(COMPONENT_NAME execution NAMESPACE lux::cxx)
component_include_directories(execution
    BUILD_TIME_EXPORT ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME include)
component_add_transitive_commands(execution
    "find_package(Threads REQUIRED GLOBAL)"
    "find_package(stdexec CONFIG REQUIRED GLOBAL)"
    "find_package(concurrentqueue CONFIG REQUIRED GLOBAL)")

target_compile_features(execution INTERFACE cxx_std_20)
target_link_libraries(execution INTERFACE
    STDEXEC::stdexec
    concurrentqueue::concurrentqueue
    Threads::Threads)

option(LUX_EXECUTION_BUILD_TESTS "Build execution tests" ${LUX_CXX_BUILD_TESTS})
option(LUX_EXECUTION_BUILD_EXAMPLES "Build execution examples" OFF)
option(LUX_EXECUTION_BUILD_BENCHMARKS "Build execution microbenchmark" OFF)
if(LUX_EXECUTION_BUILD_TESTS AND BUILD_TESTING)
    add_subdirectory(test)
endif()
if(LUX_EXECUTION_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(LUX_EXECUTION_BUILD_BENCHMARKS)
    add_executable(lux_execution_benchmark benchmark/manual_context.cpp)
    target_link_libraries(lux_execution_benchmark PRIVATE lux::cxx::execution)
    lux_execution_test_options(lux_execution_benchmark)
endif()
