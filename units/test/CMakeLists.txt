add_executable(units_test units_test.cpp)
target_link_libraries(units_test PRIVATE lux::cxx::units)
target_compile_features(units_test PRIVATE cxx_std_20)
add_test(NAME units_test COMMAND units_test)
