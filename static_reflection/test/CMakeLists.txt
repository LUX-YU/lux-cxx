add_executable(
    static_reflection_test
    static_reflection_test.cpp
)

target_link_libraries(
    static_reflection_test
    PRIVATE
    static_reflection
)