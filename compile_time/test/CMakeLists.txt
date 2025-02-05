add_executable(
    type_info_test
    type_info_test.cpp
)

target_link_libraries(
    type_info_test
    PRIVATE
    compile_time
)

add_executable(
    computer_pipeline_test
    computer_pipeline_test.cpp
)

target_link_libraries(
    computer_pipeline_test
    PRIVATE
    compile_time
)

add_executable(
    sequence_sort_test
    sequence_sort_test.cpp
)

target_link_libraries(
    sequence_sort_test
    PRIVATE
    compile_time
)