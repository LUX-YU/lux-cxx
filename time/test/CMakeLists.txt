add_executable(time_test time_test.cpp)
target_link_libraries(time_test PRIVATE lux::cxx::time)
target_compile_features(time_test PRIVATE cxx_std_20)
add_test(NAME time_test COMMAND time_test)
