add_executable(
	arguments_test
	arguments_test.cpp
)

target_link_libraries(
	arguments_test
	arguments
)

add_test(NAME arguments_test COMMAND arguments_test)
