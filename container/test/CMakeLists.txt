add_executable(
	tree_test
	tree_test.cpp
)

target_link_libraries(
	tree_test
	PRIVATE
	lux::cxx::container
)