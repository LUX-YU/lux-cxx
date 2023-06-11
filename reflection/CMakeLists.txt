find_package(Clang REQUIRED)

add_component(
    COMPONENT_NAME                  reflection
    NAMESPACE                       lux::cxx
    SOURCE_FILES                    ${CMAKE_CURRENT_SOURCE_DIR}/src/ClangParser.cpp
                                    ${CMAKE_CURRENT_SOURCE_DIR}/src/LuxCxxParser.cpp
                                    ${CMAKE_CURRENT_SOURCE_DIR}/src/LuxCxxParserImpl.cpp
    BUILD_TIME_EXPORT_INCLUDE_DIRS  ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME_INCLUDE_PREFIX     include
    PRIVATE_INCLUDE_DIRS            pinclude
                                    ${CLANG_INCLUDE_DIRS}
    PUBLIC_LIBRARIES                lux::cxx::visibility
                                    libclang
    PRIVATE_DEFINITIONS             LUX_CXX_LIBRARY
)
