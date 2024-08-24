add_executable(
    type_info_test
    type_info_test.cpp
)

target_link_libraries(
    type_info_test
    PRIVATE
    compile_time
)