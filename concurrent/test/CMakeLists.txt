add_executable(
    timer_test
    timer_test.cpp
)

target_link_libraries(
    timer_test
    PRIVATE
    concurrent
)