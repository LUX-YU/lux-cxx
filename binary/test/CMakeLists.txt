add_executable(binary_test binary_test.cpp)
target_link_libraries(binary_test PRIVATE lux::cxx::binary)
target_compile_features(binary_test PRIVATE cxx_std_20)
add_test(NAME binary_test COMMAND binary_test)

add_executable(binary_fuzz_test binary_fuzz_test.cpp)
target_link_libraries(binary_fuzz_test PRIVATE lux::cxx::binary)
target_compile_features(binary_fuzz_test PRIVATE cxx_std_20)
add_test(NAME binary_fuzz_test COMMAND binary_fuzz_test)
