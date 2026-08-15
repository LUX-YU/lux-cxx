add_executable(
	tree_test
	tree_test.cpp
)

target_link_libraries(
	tree_test
	PRIVATE
	lux::cxx::container
)
add_test(NAME tree_test COMMAND tree_test)

add_executable(
	sparse_set_test
	sparse_set_test.cpp
)

target_link_libraries(
	sparse_set_test
	PRIVATE
	lux::cxx::container
)
add_test(NAME sparse_set_test COMMAND sparse_set_test)

add_executable(
	orth_tree_test
	orth_tree_test.cpp
)

target_link_libraries(
	orth_tree_test
	PRIVATE
	lux::cxx::container
)
add_test(NAME orth_tree_test COMMAND orth_tree_test)

add_executable(
	slot_map_test
	slot_map_test.cpp
)

target_link_libraries(
	slot_map_test
	PRIVATE
	lux::cxx::container
)
add_test(NAME slot_map_test COMMAND slot_map_test)

add_executable(
	basic_sparse_set_test
	basic_sparse_set_test.cpp
)

target_link_libraries(
	basic_sparse_set_test
	PRIVATE
	lux::cxx::container
)
add_test(NAME basic_sparse_set_test COMMAND basic_sparse_set_test)

add_executable(
	small_vector_test
	small_vector_test.cpp
)

target_link_libraries(
	small_vector_test
	PRIVATE
	lux::cxx::container
)
add_test(NAME small_vector_test COMMAND small_vector_test)

add_executable(small_vector_allocator_test small_vector_allocator_test.cpp)
target_link_libraries(small_vector_allocator_test PRIVATE lux::cxx::container)
add_test(NAME small_vector_allocator_test COMMAND small_vector_allocator_test)

add_executable(stable_slot_map_test stable_slot_map_test.cpp)
target_link_libraries(stable_slot_map_test PRIVATE lux::cxx::container)
add_test(NAME stable_slot_map_test COMMAND stable_slot_map_test)

# ---- Microbenchmark (opt-in, build Release for meaningful numbers) ----
# ENABLE_CONTAINER_BENCH builds the std/self comparisons (SmallVector vs
# std::vector; SparseSet/BasicSparseSet/SlotMap vs std::unordered_map). If
# ENABLE_COMPARE_WITH_ENTT is ALSO on, the EnTT comparison section is compiled
# in (LUX_BENCH_WITH_ENTT) and EnTT::EnTT is linked. Reuses the shared
# bench_common.hpp harness from archtype/test.
set(ENABLE_CONTAINER_BENCH false CACHE BOOL "Build the container microbenchmark")
if(ENABLE_CONTAINER_BENCH)
	add_executable(container_bench container_bench.cpp)
	target_link_libraries(container_bench PRIVATE lux::cxx::container)
	target_include_directories(container_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../archtype/test)
	if(ENABLE_COMPARE_WITH_ENTT)
		find_package(EnTT CONFIG REQUIRED)
		target_link_libraries(container_bench PRIVATE EnTT::EnTT)
		target_compile_definitions(container_bench PRIVATE LUX_BENCH_WITH_ENTT=1)
	endif()
endif()
