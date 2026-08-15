add_interface_component(
    COMPONENT_NAME abi
    NAMESPACE      lux::cxx
)

component_include_directories(
    abi
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/include
    INSTALL_TIME
        include
)

component_add_internal_dependencies(
    abi
        lux::cxx::algorithm
        lux::cxx::core
)

include(TestBigEndian)
test_big_endian(LUX_CXX_ABI_BIG_ENDIAN)

if(LUX_CXX_ABI_BIG_ENDIAN)
    set(LUX_CXX_ABI_ENDIAN "big")
else()
    set(LUX_CXX_ABI_ENDIAN "little")
endif()

if(MSVC)
    set(LUX_CXX_ABI_STANDARD_LIBRARY "msvc-stl")
    set(LUX_CXX_ABI_CRT "msvc")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_FLAGS MATCHES "stdlib=libc\\+\\+")
    set(LUX_CXX_ABI_STANDARD_LIBRARY "libc++")
    set(LUX_CXX_ABI_CRT "platform")
else()
    set(LUX_CXX_ABI_STANDARD_LIBRARY "libstdc++")
    set(LUX_CXX_ABI_CRT "platform")
endif()

string(TOUPPER "${CMAKE_BUILD_TYPE}" _lux_cxx_abi_config)
set(_lux_cxx_abi_config_flags "")
if(NOT _lux_cxx_abi_config STREQUAL "")
    set(_lux_cxx_abi_config_flags
        "${CMAKE_CXX_FLAGS_${_lux_cxx_abi_config}}"
    )
endif()
set(_lux_cxx_abi_flags "${CMAKE_CXX_FLAGS} ${_lux_cxx_abi_config_flags}")
if(_lux_cxx_abi_flags MATCHES "(/EHs-|/EHc-|[-]fno-exceptions)")
    set(LUX_CXX_ABI_EXCEPTIONS 0)
else()
    set(LUX_CXX_ABI_EXCEPTIONS 1)
endif()
if(_lux_cxx_abi_flags MATCHES "(/GR-|[-]fno-rtti)")
    set(LUX_CXX_ABI_RTTI 0)
else()
    set(LUX_CXX_ABI_RTTI 1)
endif()
if(_lux_cxx_abi_flags MATCHES "sanitize=([^ ]+)")
    set(LUX_CXX_ABI_SANITIZERS "${CMAKE_MATCH_1}")
else()
    set(LUX_CXX_ABI_SANITIZERS "none")
endif()
if(CMAKE_BUILD_TYPE MATCHES "[Dd]ebug")
    set(LUX_CXX_ABI_DEBUG 1)
else()
    set(LUX_CXX_ABI_DEBUG 0)
endif()

set(LUX_CXX_ABI_TARGET "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(LUX_CXX_ABI_POINTER_BITS "${CMAKE_SIZEOF_VOID_P}")
math(EXPR LUX_CXX_ABI_POINTER_BITS "${LUX_CXX_ABI_POINTER_BITS} * 8")
set(LUX_CXX_ABI_API_MAJOR "${PROJECT_VERSION_MAJOR}")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildInfo.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/lux/cxx/abi/BuildInfo.hpp
    @ONLY
)

if(LUX_CXX_BUILD_TESTS AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/include/lux/cxx/abi/BuildInfo.hpp
    DESTINATION include/lux/cxx/abi
)

lux_add_header_self_containment(
    abi_header_self_containment
    lux::cxx::abi
    lux/cxx/abi/Abi.hpp
    lux/cxx/abi/BuildInfo.hpp
)
