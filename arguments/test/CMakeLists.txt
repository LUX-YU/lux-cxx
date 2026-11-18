find_package(Threads REQUIRED)

add_executable(arguments_test arguments_test.cpp)
target_link_libraries(arguments_test PRIVATE lux::cxx::arguments)
add_test(NAME arguments_test COMMAND arguments_test)

foreach(name IN ITEMS typed allocations equivalence structured structured_allocations structured_property)
    add_executable(arguments_${name}_test ${name}_test.cpp)
    target_link_libraries(arguments_${name}_test PRIVATE lux::cxx::arguments)
    add_test(NAME arguments_${name}_test COMMAND arguments_${name}_test)
endforeach()

target_link_libraries(arguments_equivalence_test PRIVATE Threads::Threads)
target_link_libraries(arguments_structured_property_test PRIVATE Threads::Threads)
