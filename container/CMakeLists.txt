add_interface_component(
    COMPONENT_NAME                  container
    NAMESPACE                       lux::cxx
)

component_include_directories(
    container
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

set(ENABLE_CONTAINER_TEST ${LUX_CXX_BUILD_TESTS} CACHE BOOL "Enable container test")
if(ENABLE_CONTAINER_TEST AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()

lux_add_header_self_containment(
    container_changed_header_self_containment
    lux::cxx::container
    lux/cxx/container/SlotMap.hpp
    lux/cxx/container/SmallVector.hpp
    lux/cxx/container/StableSlotMap.hpp
)
