add_executable(
    intrusive_ptr_test
    intrusive_ptr_test.cpp
)

target_link_libraries(
    intrusive_ptr_test
    PRIVATE
    lux::cxx::memory
)

add_executable(
    object_pool_test
    object_pool_test.cpp
)

target_link_libraries(
    object_pool_test
    PRIVATE
    lux::cxx::memory
)