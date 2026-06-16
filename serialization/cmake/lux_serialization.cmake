# ===========================================================================
# lux_serialization.cmake
#
# Turn-key CMake helpers that drive lux_meta_generator with the bundled
# serialization template (serializable.template.inja) to emit
# `lux::cxx::ser::meta_info<T>` specialisations for the user's annotated types.
#
# These functions delegate to add_meta()/target_add_meta() from the reflection
# module's meta.cmake. A consumer therefore typically does:
#
#     find_package(lux-cxx REQUIRED COMPONENTS reflection_generator serialization)
#     # Pass the NAMESPACED component name — the bare name (e.g. "reflection_generator")
#     # only exists inside lux-cxx's own build tree, not in a consumer's.
#     include_component_cmake_scripts(lux::cxx::reflection_generator) # add_meta/target_add_meta
#     include_component_cmake_scripts(lux::cxx::serialization)        # lux_target_add_serialization
#
#     add_executable(app main.cpp)
#     target_link_libraries(app PRIVATE lux::cxx::serialization)
#     lux_target_add_serialization(          # no GENERATOR needed — auto-located
#         TARGET  app
#         HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/config.hpp
#     )
#     # main.cpp: #include "config.hpp"  then  #include "config.serialize.hpp"
# ===========================================================================

if(_LUX_SERIALIZATION_TOOLS_INCLUDED_)
  return()
endif()
set(_LUX_SERIALIZATION_TOOLS_INCLUDED_ TRUE)

# ---------------------------------------------------------------------------
# Locate the bundled serialization template. Works both in the source tree
# (cmake/ + templates/ siblings) and after install (component_add_cmake_scripts
# flattens the template next to this script under <comp>/cmake_scripts/).
# ---------------------------------------------------------------------------
set(_lux_ser_template "")
foreach(_cand
    "${CMAKE_CURRENT_LIST_DIR}/serializable.template.inja"               # installed layout
    "${CMAKE_CURRENT_LIST_DIR}/../templates/serializable.template.inja"  # source-tree layout
)
    if(EXISTS "${_cand}")
        get_filename_component(_lux_ser_template "${_cand}" ABSOLUTE)
        break()
    endif()
endforeach()
if(NOT _lux_ser_template)
    message(WARNING "[lux_serialization] serializable.template.inja not found near ${CMAKE_CURRENT_LIST_DIR}")
endif()
set(LUX_SERIALIZATION_TEMPLATE "${_lux_ser_template}"
    CACHE FILEPATH "Path to the bundled serialization inja template")

# ---------------------------------------------------------------------------
# lux_target_add_serialization
#
#   TARGET   <tgt>                 (required) target the generated meta is attached to
#   HEADERS  <h1.hpp> [h2 ...]     (required) annotated headers to scan
#   NAME     <metaName>            default: <TARGET>_serialization
#   MARKER   <marker>              default: serializable
#   META_SUFFIX <suffix>          default: .serialize.hpp
#   GENERATOR <exe|genexpr>        default: lux::cxx::lux_meta_generator (auto-located). Omit it.
#   OUT_DIR  <dir>                 default: ${CMAKE_BINARY_DIR}/metagen
#   COMPILE_COMMANDS <json>
#   SOURCE_FILE <cpp>
#   EXTRA_COMPILE_OPTIONS <opt...>
#   [ECHO] [ALWAYS_REGENERATE]
#
# The generated <stem>.serialize.hpp files are header-only; OUT_DIR is added to
# the target's include path so they can be #included next to the source headers.
# ---------------------------------------------------------------------------
function(lux_target_add_serialization)
    set(_opts  ECHO ALWAYS_REGENERATE)
    set(_one   TARGET NAME MARKER META_SUFFIX GENERATOR OUT_DIR COMPILE_COMMANDS SOURCE_FILE)
    set(_multi HEADERS EXTRA_COMPILE_OPTIONS)
    cmake_parse_arguments(S "${_opts}" "${_one}" "${_multi}" ${ARGN})

    if(NOT COMMAND add_meta OR NOT COMMAND target_add_meta)
        message(FATAL_ERROR
            "[lux_serialization] add_meta()/target_add_meta() are not available. "
            "Call include_component_cmake_scripts(reflection_generator) before "
            "lux_target_add_serialization().")
    endif()
    if(NOT S_TARGET)
        message(FATAL_ERROR "[lux_serialization] TARGET is required")
    endif()
    if(NOT S_HEADERS)
        message(FATAL_ERROR "[lux_serialization] HEADERS is required")
    endif()
    if(NOT LUX_SERIALIZATION_TEMPLATE)
        message(FATAL_ERROR "[lux_serialization] serialization template path is unknown")
    endif()

    if(NOT S_NAME)
        set(S_NAME "${S_TARGET}_serialization")
    endif()
    if(NOT S_MARKER)
        set(S_MARKER "serializable")
    endif()
    if(NOT S_META_SUFFIX)
        set(S_META_SUFFIX ".serialize.hpp")
    endif()

    # ---- add_meta: register the meta object with serialization defaults ----
    set(_addmeta_args
        NAME         "${S_NAME}"
        MARKER       "${S_MARKER}"
        TEMPLATE     "${LUX_SERIALIZATION_TEMPLATE}"
        META_SUFFIX  "${S_META_SUFFIX}"
        TARGET_FILES ${S_HEADERS}
    )
    if(S_GENERATOR)
        list(APPEND _addmeta_args GENERATOR "${S_GENERATOR}")
    endif()
    if(S_OUT_DIR)
        list(APPEND _addmeta_args OUT_DIR "${S_OUT_DIR}")
    endif()
    if(S_COMPILE_COMMANDS)
        list(APPEND _addmeta_args COMPILE_COMMANDS "${S_COMPILE_COMMANDS}")
    endif()
    if(S_SOURCE_FILE)
        list(APPEND _addmeta_args SOURCE_FILE "${S_SOURCE_FILE}")
    endif()
    if(S_EXTRA_COMPILE_OPTIONS)
        list(APPEND _addmeta_args EXTRA_COMPILE_OPTIONS ${S_EXTRA_COMPILE_OPTIONS})
    endif()
    if(S_ECHO)
        list(APPEND _addmeta_args ECHO)
    endif()
    add_meta(${_addmeta_args})

    # ---- target_add_meta: wire generation into the build of TARGET ----
    set(_tam_args TARGET "${S_TARGET}" NAME "${S_NAME}")
    if(S_ALWAYS_REGENERATE)
        list(APPEND _tam_args ALWAYS_REGENERATE)
    endif()
    if(S_ECHO)
        list(APPEND _tam_args ECHO)
    endif()
    target_add_meta(${_tam_args})
endfunction()
