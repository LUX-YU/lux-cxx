function(lux_add_concurrent_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE lux::cxx::concurrent)
    target_compile_features(${target} PRIVATE cxx_std_20)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

lux_add_concurrent_test(timer_test timer_test.cpp)
lux_add_concurrent_test(thread_pool_test thread_pool_test.cpp)
lux_add_concurrent_test(blocking_queue_test blocking_queue_test.cpp)
lux_add_concurrent_test(
    spsc_lckfree_ring_queue_test
    spsc_lckfree_ring_queue_test.cpp
)
lux_add_concurrent_test(parallel_pipeline_test parallel_pipeline_test.cpp)
lux_add_concurrent_test(concurrent_gate_test concurrent_gate_test.cpp)
