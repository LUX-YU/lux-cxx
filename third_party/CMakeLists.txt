# Vendored third-party dependencies.
#
# Bundled in-tree so the project carries no external find_package() requirement
# for these (both licenses permit redistribution):
#   - nlohmann_json  v3.12.0  (MIT)   header-only
#   - tinyxml2       v11.0.0  (zlib)  one .cpp
#
# Both are INTERNAL implementation details — neither is exported to consumers:
#   - nlohmann_json: an INTERFACE target, linked into the JSON backend through
#     $<BUILD_INTERFACE:> (header-only, so it leaves no symbol for a consumer to
#     resolve). Defined here only so the build tree can reference its headers.
#   - tinyxml2: NOT a component at all — its single .cpp is compiled directly into
#     serialization_xml, so that static archive is self-contained.

#[[========================  nlohmann_json (header-only)  ========================]]
# Prefer a system/vcpkg nlohmann_json; otherwise fall back to the vendored copy
# (always present — a small header, the reliable offline default; no network even
# without LUX_FETCH_DEPS). Either way the rest of the tree links the same
# lux::cxx::nlohmann_json, hidden behind $<BUILD_INTERFACE:> so consumers never see it.
find_package(nlohmann_json QUIET)
if(TARGET nlohmann_json::nlohmann_json)
    add_library(lux_nlohmann_json INTERFACE)
    target_link_libraries(lux_nlohmann_json INTERFACE nlohmann_json::nlohmann_json)
    add_library(lux::cxx::nlohmann_json ALIAS lux_nlohmann_json)
    message(STATUS "[third_party] nlohmann_json: system package ${nlohmann_json_VERSION}")
else()
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
    message(STATUS "[third_party] nlohmann_json: vendored copy")
endif()

#[[============================  tinyxml2 (vendored)  ===========================]]
# tinyxml2 is intentionally NOT a component. Its single source file
# (tinyxml2/src/tinyxml2.cpp) is compiled directly into serialization_xml and its
# headers (tinyxml2/include) are added there as a PRIVATE, build-only include — see
# serialization/CMakeLists.txt. That keeps the static archive self-contained and
# means tinyxml2 never appears in the installed lux-cxx package.
