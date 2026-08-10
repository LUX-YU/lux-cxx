add_executable(sha256_test
    sha256_test.cpp
)

target_link_libraries(sha256_test
    PRIVATE
        lux::cxx::algorithm
)

target_compile_features(sha256_test PRIVATE cxx_std_20)

add_test(NAME sha256_test COMMAND sha256_test)
