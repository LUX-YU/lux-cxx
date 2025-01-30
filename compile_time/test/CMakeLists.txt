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
    computer_graph_test
    computer_graph_test.cpp
)

target_link_libraries(
    computer_graph_test
    PRIVATE
    compile_time
)