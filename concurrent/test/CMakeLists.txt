add_executable(
    timer_test
    timer_test.cpp
)

target_link_libraries(
    timer_test
    PRIVATE
    concurrent
)

add_executable(
    thread_pool_test
    thread_pool_test.cpp
)

target_link_libraries(
    thread_pool_test
    PRIVATE
    concurrent
)

add_executable(
    blocking_queue_test
    blocking_queue_test.cpp
)

target_link_libraries(
    blocking_queue_test
    PRIVATE
    concurrent
)

add_executable(
    spsc_lckfree_ring_queue_test
    spsc_lckfree_ring_queue_test.cpp
)

target_link_libraries(
    spsc_lckfree_ring_queue_test
    PRIVATE
    concurrent
)