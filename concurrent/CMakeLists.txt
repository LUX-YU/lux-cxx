add_interface_component(
    COMPONENT_NAME                  concurrent
    NAMESPACE                       lux::cxx
)

component_include_directories(
    concurrent
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

component_add_internal_dependencies(
    concurrent
        lux::cxx::compile_time
        lux::cxx::core
)

set(ENABLE_CONCURRENT_TEST ${LUX_CXX_BUILD_TESTS} CACHE BOOL "Enable concurrent test")
if(ENABLE_CONCURRENT_TEST AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()

lux_add_header_self_containment(
    concurrent_changed_header_self_containment
    lux::cxx::concurrent
    lux/cxx/concurrent/AdmissionGate.hpp
    lux/cxx/concurrent/AtomicWait.hpp
    lux/cxx/concurrent/BlockingQueue.hpp
    lux/cxx/concurrent/BudgetGate.hpp
    lux/cxx/concurrent/LockFreeQueue.hpp
    lux/cxx/concurrent/QueueStatus.hpp
    lux/cxx/concurrent/ThreadPool.hpp
)
