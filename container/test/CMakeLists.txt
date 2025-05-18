add_executable(
	tree_test
	tree_test.cpp
)

target_link_libraries(
	tree_test
	PRIVATE
	lux::cxx::container
)

add_executable(
	sparse_set_test
	sparse_set_test.cpp
)

target_link_libraries(
	sparse_set_test
	PRIVATE
	lux::cxx::container
)
