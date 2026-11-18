add_interface_component(
    COMPONENT_NAME core
    NAMESPACE      lux::cxx
)

component_include_directories(
    core
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

component_add_internal_dependencies(
    core
        lux::cxx::compile_time
)

if(LUX_CXX_BUILD_TESTS AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()

lux_add_header_self_containment(
    core_header_self_containment
    lux::cxx::core
    lux/cxx/core/CheckedArithmetic.hpp
    lux/cxx/core/Core.hpp
    lux/cxx/core/Delegate.hpp
    lux/cxx/core/EnumFlags.hpp
    lux/cxx/core/FixedText.hpp
    lux/cxx/core/Format.hpp
    lux/cxx/core/function_ref.hpp
    lux/cxx/core/move_only_function.hpp
    lux/cxx/core/Schema.hpp
    lux/cxx/core/scope_exit.hpp
    lux/cxx/core/StableNameId.hpp
    lux/cxx/core/StrongId.hpp
)
