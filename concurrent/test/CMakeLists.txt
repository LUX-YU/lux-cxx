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