cmake_minimum_required(VERSION 3.18)
project(lux-cxx-install-consumer LANGUAGES CXX)

enable_testing()
find_package(
    lux-cxx
    CONFIG
    REQUIRED
    COMPONENTS
        abi
        algorithm
        binary
        concurrent
        container
        core
        diagnostic
        memory
        time
        units
)

add_executable(lux_cxx_install_consumer main.cpp)
target_compile_features(lux_cxx_install_consumer PRIVATE cxx_std_20)
target_link_libraries(
    lux_cxx_install_consumer
    PRIVATE
        lux::cxx::abi
        lux::cxx::binary
        lux::cxx::concurrent
        lux::cxx::container
        lux::cxx::diagnostic
        lux::cxx::memory
        lux::cxx::units
)
add_test(NAME lux_cxx_install_consumer COMMAND lux_cxx_install_consumer)
