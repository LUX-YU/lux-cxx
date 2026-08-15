add_executable(diagnostic_test diagnostic_test.cpp)
target_link_libraries(diagnostic_test PRIVATE lux::cxx::diagnostic)
target_compile_features(diagnostic_test PRIVATE cxx_std_20)
add_test(NAME diagnostic_test COMMAND diagnostic_test)
