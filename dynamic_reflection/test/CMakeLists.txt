get_filename_component(PARENT_DIR ../ ABSOLUTE)
set(DREF_PUBLIC_DIR ${PARENT_DIR}/include)

# macro definition
set(LUX_DREF_TEST_GEN_FILE_PATH 
    ${CMAKE_BINARY_DIR}/gen/include/clang_config.h
)
set(LUX_CXX_INCLUDE_DIR_DEF
    ${DREF_PUBLIC_DIR}
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/clang_config.h.in
    ${LUX_DREF_TEST_GEN_FILE_PATH}
)

# add test targets
add_executable(
    dref_test_libclang
    dref_test_libclang.cpp
)

target_link_libraries(
    dref_test_libclang
    PRIVATE
    lux::cxx::dynamic_reflection
    libclang
)

target_include_directories(
    dref_test_libclang
    PRIVATE
    ${LUX_DREF_TEST_GEN_FILE_PATH}
)

#[[
add_executable(
    dref_test_runtime_invoke
    dref_test_runtime_invoke.cpp
)

target_link_libraries(
    dref_test_runtime_invoke
    PRIVATE
    lux::cxx::dynamic_reflection
    libclang
)

target_include_directories(
    dref_test_runtime_invoke
    PRIVATE
    ${LUX_DREF_TEST_GEN_FILE_PATH}
)
]]
