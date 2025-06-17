add_interface_component(
    COMPONENT_NAME                  archtype
    NAMESPACE                       lux::cxx
)

component_include_directories(
    archtype
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)
set(ENABLE_ARCHTYPE_TEST true CACHE BOOL "Enable archtype test")
if(ENABLE_ARCHTYPE_TEST)
    add_subdirectory(test)
endif()