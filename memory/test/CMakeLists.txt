add_executable(
    intrusive_ptr_test
    intrusive_ptr_test.cpp
)

target_link_libraries(
    intrusive_ptr_test
    PRIVATE
    lux::cxx::memory
)

add_test(NAME intrusive_ptr_test COMMAND intrusive_ptr_test)

add_executable(
    object_pool_test
    object_pool_test.cpp
)

target_link_libraries(
    object_pool_test
    PRIVATE
    lux::cxx::memory
)

add_test(NAME object_pool_test COMMAND object_pool_test)

add_executable(memory_primitives_test memory_primitives_test.cpp)
target_link_libraries(memory_primitives_test PRIVATE lux::cxx::memory)
add_test(NAME memory_primitives_test COMMAND memory_primitives_test)

option(ENABLE_MEMORY_BENCH "Build memory microbenchmarks" OFF)
if(ENABLE_MEMORY_BENCH)
    add_executable(object_pool_benchmark object_pool_benchmark.cpp)
    target_link_libraries(object_pool_benchmark PRIVATE lux::cxx::memory)
endif()
