add_executable(core_test core_test.cpp)
target_link_libraries(core_test PRIVATE lux::cxx::core)
target_compile_features(core_test PRIVATE cxx_std_20)
add_test(NAME core_test COMMAND core_test)
