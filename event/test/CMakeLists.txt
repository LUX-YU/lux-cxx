add_executable(
    signal_test
    signal_test.cpp
)

target_link_libraries(
    signal_test
    PRIVATE
    event
)
