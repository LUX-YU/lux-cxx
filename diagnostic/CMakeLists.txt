add_interface_component(
    COMPONENT_NAME diagnostic
    NAMESPACE      lux::cxx
)

component_include_directories(
    diagnostic
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

component_add_internal_dependencies(
    diagnostic
        lux::cxx::core
        lux::cxx::time
)

if(LUX_CXX_BUILD_TESTS AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()

lux_add_header_self_containment(
    diagnostic_header_self_containment
    lux::cxx::diagnostic
    lux/cxx/diagnostic/Diagnostic.hpp
    lux/cxx/diagnostic/DiagnosticRecord.hpp
)
