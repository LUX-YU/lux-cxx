find_package(Clang REQUIRED)

add_component(
    COMPONENT_NAME              reflection
    NAMESPACE                   lux::cxx
    SOURCE_FILES                ${CMAKE_CURRENT_SOURCE_DIR}/src/ClangParser.cpp
                                ${CMAKE_CURRENT_SOURCE_DIR}/src/LuxCxxParser.cpp
                                ${CMAKE_CURRENT_SOURCE_DIR}/src/LuxCxxParserImpl.cpp
    EXPORT_INCLUDE_DIRS         include
    PRIVATE_INCLUDE_DIRS        pinclude
                                ${CLANG_INCLUDE_DIRS}
    PUBLIC_LIBRARIES            lux::cxx::visibility
                                libclang
)
