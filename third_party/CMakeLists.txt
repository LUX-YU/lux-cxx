# Vendored third-party dependencies.
#
# Bundled in-tree so the project carries no external find_package() requirement
# for these (both licenses permit redistribution):
#   - nlohmann_json  v3.12.0  (MIT)   header-only
#   - tinyxml2       v11.0.0  (zlib)  one .cpp
#
# Exposed as first-class components so consumers receive the headers on install:
#   lux::cxx::nlohmann_json   (INTERFACE)
#   lux::cxx::tinyxml2        (STATIC)

#[[========================  nlohmann_json (header-only)  ========================]]
add_interface_component(
    COMPONENT_NAME nlohmann_json
    NAMESPACE      lux::cxx
)

component_include_directories(
    nlohmann_json
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json/include
    INSTALL_TIME
        include
)

#[[============================  tinyxml2 (static)  =============================]]
add_component(
    COMPONENT_NAME tinyxml2
    NAMESPACE      lux::cxx
    STATIC
    SOURCE_FILES   ${CMAKE_CURRENT_SOURCE_DIR}/tinyxml2/src/tinyxml2.cpp
)

component_include_directories(
    tinyxml2
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/tinyxml2/include
    INSTALL_TIME
        include
)
