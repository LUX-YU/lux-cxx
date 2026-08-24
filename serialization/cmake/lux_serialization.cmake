# ===========================================================================
# lux_serialization.cmake
#
# Turn-key CMake helpers that drive lux_meta_generator with the bundled
# serialization template (serializable.template.inja) to emit
# `lux::cxx::ser::meta_info<T>` specialisations for the user's annotated types.
#
# These functions delegate to the parse-job/projection API from the reflection
# module's meta.cmake. A consumer therefore typically does:
#
#     find_package(lux-cxx REQUIRED COMPONENTS reflection_generator serialization)
#     # Pass the NAMESPACED component name — the bare name (e.g. "reflection_generator")
#     # only exists inside lux-cxx's own build tree, not in a consumer's.
#     include_component_cmake_scripts(lux::cxx::reflection_generator)
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
    set(_opts  ECHO)
    set(_one   TARGET NAME MARKER META_SUFFIX GENERATOR OUT_DIR COMPILE_COMMANDS SOURCE_FILE)
    set(_multi HEADERS EXTRA_COMPILE_OPTIONS)
    cmake_parse_arguments(S "${_opts}" "${_one}" "${_multi}" ${ARGN})

    if(NOT COMMAND lux_add_codegen_job OR
       NOT COMMAND lux_codegen_add_projection OR
       NOT COMMAND lux_target_add_codegen)
        message(FATAL_ERROR
            "[lux_serialization] multi-projection codegen API is not available. "
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

    if(NOT S_OUT_DIR)
        set(S_OUT_DIR "${CMAKE_BINARY_DIR}/metagen")
    endif()
    set(_logical_paths "")
    foreach(_header IN LISTS S_HEADERS)
        file(RELATIVE_PATH _logical "${CMAKE_CURRENT_SOURCE_DIR}" "${_header}")
        if(_logical MATCHES "^\\.\\.")
            get_filename_component(_logical "${_header}" NAME)
        endif()
        list(APPEND _logical_paths "${_logical}")
    endforeach()

    set(_job_args
        NAME          "${S_NAME}"
        MARKER        "${S_MARKER}"
        TARGET_FILES  ${S_HEADERS}
        LOGICAL_PATHS ${_logical_paths}
    )
    if(S_GENERATOR)
        list(APPEND _job_args GENERATOR "${S_GENERATOR}")
    endif()
    if(S_COMPILE_COMMANDS)
        list(APPEND _job_args COMPILE_COMMANDS "${S_COMPILE_COMMANDS}")
    endif()
    if(S_SOURCE_FILE)
        list(APPEND _job_args SOURCE_FILE "${S_SOURCE_FILE}")
    endif()
    if(S_EXTRA_COMPILE_OPTIONS)
        list(APPEND _job_args EXTRA_COMPILE_OPTIONS ${S_EXTRA_COMPILE_OPTIONS})
    endif()
    lux_add_codegen_job(${_job_args})
    lux_codegen_add_projection(
        JOB           "${S_NAME}"
        NAME          serialization
        TEMPLATE      "${LUX_SERIALIZATION_TEMPLATE}"
        OUTPUT_ROOT   "${S_OUT_DIR}"
        OUTPUT_SUFFIX "${S_META_SUFFIX}"
    )
    lux_target_add_codegen(TARGET "${S_TARGET}" JOB "${S_NAME}")
endfunction()
