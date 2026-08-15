add_executable(abi_test abi_test.cpp)
target_link_libraries(abi_test PRIVATE lux::cxx::abi)
target_compile_features(abi_test PRIVATE cxx_std_20)
add_test(NAME abi_test COMMAND abi_test)
