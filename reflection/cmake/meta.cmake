# ===========================================
# Include guard
# ===========================================
if(_COMPONENT_META_TOOLS_INCLUDED_)
  return()
endif()
set(_COMPONENT_META_TOOLS_INCLUDED_ TRUE)

# ---------------------------------------------------------------------------
# Expose the generator as the namespaced target lux::cxx::lux_meta_generator,
# consistent with the lux::cxx::* library targets. In lux-cxx's own build tree an
# ALIAS already exists; in a consumer's tree we synthesise an IMPORTED executable
# pointing at the installed bin/lux_meta_generator (located via find_program — the
# install prefix is already on CMAKE_PREFIX_PATH after find_package(lux-cxx)).
# Either way, downstream can use $<TARGET_FILE:lux::cxx::lux_meta_generator> or, more
# simply, just omit GENERATOR from lux_target_add_serialization / add_meta.
# ---------------------------------------------------------------------------
if(NOT TARGET lux::cxx::lux_meta_generator)
    find_program(LUX_META_GENERATOR_EXECUTABLE NAMES lux_meta_generator)
    if(LUX_META_GENERATOR_EXECUTABLE)
        add_executable(lux::cxx::lux_meta_generator IMPORTED GLOBAL)
        set_target_properties(lux::cxx::lux_meta_generator PROPERTIES
            IMPORTED_LOCATION "${LUX_META_GENERATOR_EXECUTABLE}")
    endif()
endif()

# -------------------------------------------------
# _meta_json_escape(<out_var> <in_string>)
#   Escape a string so it is safe inside a JSON string literal. The generator's
#   *_meta_config.json is hand-written with file(WRITE/APPEND), so any value that
#   contains a backslash (Windows paths like C:\Users\...) or a double-quote would
#   otherwise produce invalid JSON (\U / \b / \t are illegal-or-wrong escapes).
#   Order matters: escape backslash FIRST, then the double-quote — otherwise the
#   backslash added for the quote escape would itself get doubled.
#
#   This is a function (NOT a macro): a macro substitutes its arguments textually,
#   so a value containing backslashes/quotes would be re-parsed as code at expansion
#   ("Invalid character escape '\U'"). A function binds the argument as a real
#   variable value, so the special characters are handled correctly.
# -------------------------------------------------
function(_meta_json_escape _out _in)
    set(_t "${_in}")
    string(REPLACE "\\" "\\\\" _t "${_t}")
    string(REPLACE "\"" "\\\"" _t "${_t}")
    set(${_out} "${_t}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# Multi-projection code generation API.
# A job owns parsing inputs; projections own templates and output policy.  The
# generator receives all projections in one invocation, so adding a projection
# never reparses a target file.
# -----------------------------------------------------------------------------
function(lux_add_codegen_job)
    set(one_value_args NAME GENERATOR MARKER COMPILE_COMMANDS SOURCE_FILE)
    set(multi_value_args TARGET_FILES LOGICAL_PATHS EXTRA_COMPILE_OPTIONS)
    set(optional_args PARSE_INCLUDED_MARKED DRY_RUN)
    cmake_parse_arguments(ARGS "${optional_args}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT ARGS_NAME OR NOT ARGS_MARKER OR NOT ARGS_TARGET_FILES)
        message(FATAL_ERROR
            "[lux_add_codegen_job] NAME, MARKER and TARGET_FILES are required")
    endif()
    list(LENGTH ARGS_TARGET_FILES _file_count)
    list(LENGTH ARGS_LOGICAL_PATHS _logical_count)
    if(NOT _file_count EQUAL _logical_count)
        message(FATAL_ERROR
            "[lux_add_codegen_job] TARGET_FILES and LOGICAL_PATHS must have equal length")
    endif()
    if(TARGET "${ARGS_NAME}")
        message(FATAL_ERROR "[lux_add_codegen_job] duplicate job '${ARGS_NAME}'")
    endif()

    if(NOT ARGS_GENERATOR)
        if(TARGET lux::cxx::lux_meta_generator)
            set(ARGS_GENERATOR "$<TARGET_FILE:lux::cxx::lux_meta_generator>")
        else()
            find_program(ARGS_GENERATOR lux_meta_generator REQUIRED)
        endif()
    endif()
    if(NOT ARGS_COMPILE_COMMANDS)
        set(ARGS_COMPILE_COMMANDS "${CMAKE_BINARY_DIR}/compile_commands.json")
    endif()

    add_custom_target("${ARGS_NAME}")
    set_target_properties("${ARGS_NAME}" PROPERTIES
        LUX_CODEGEN_GENERATOR       "${ARGS_GENERATOR}"
        LUX_CODEGEN_MARKER          "${ARGS_MARKER}"
        LUX_CODEGEN_COMPILE_COMMANDS "${ARGS_COMPILE_COMMANDS}"
        LUX_CODEGEN_SOURCE_FILE     "${ARGS_SOURCE_FILE}"
        LUX_CODEGEN_TARGET_FILES    "${ARGS_TARGET_FILES}"
        LUX_CODEGEN_LOGICAL_PATHS   "${ARGS_LOGICAL_PATHS}"
        LUX_CODEGEN_COMPILE_OPTIONS "${ARGS_EXTRA_COMPILE_OPTIONS}"
        LUX_CODEGEN_PARSE_INCLUDED  "${ARGS_PARSE_INCLUDED_MARKED}"
        LUX_CODEGEN_DRY_RUN         "${ARGS_DRY_RUN}"
        LUX_CODEGEN_PROJECTIONS     ""
    )
endfunction()

function(lux_codegen_add_projection)
    set(one_value_args JOB NAME TEMPLATE OUTPUT_ROOT OUTPUT_SUFFIX)
    set(multi_value_args JSON_FIELD)
    set(optional_args FLAT_OUTPUT SERIAL_META)
    cmake_parse_arguments(ARGS "${optional_args}" "${one_value_args}" "${multi_value_args}" ${ARGN})
    if(NOT ARGS_JOB OR NOT TARGET "${ARGS_JOB}" OR NOT ARGS_NAME OR
       NOT ARGS_TEMPLATE OR NOT ARGS_OUTPUT_ROOT OR NOT ARGS_OUTPUT_SUFFIX)
        message(FATAL_ERROR
            "[lux_codegen_add_projection] JOB, NAME, TEMPLATE, OUTPUT_ROOT and OUTPUT_SUFFIX are required")
    endif()

    get_target_property(_projections "${ARGS_JOB}" LUX_CODEGEN_PROJECTIONS)
    if(ARGS_NAME IN_LIST _projections)
        message(FATAL_ERROR
            "[lux_codegen_add_projection] duplicate projection '${ARGS_NAME}'")
    endif()
    list(APPEND _projections "${ARGS_NAME}")
    set_target_properties("${ARGS_JOB}" PROPERTIES
        LUX_CODEGEN_PROJECTIONS "${_projections}"
        "LUX_CODEGEN_${ARGS_NAME}_TEMPLATE" "${ARGS_TEMPLATE}"
        "LUX_CODEGEN_${ARGS_NAME}_OUTPUT_ROOT" "${ARGS_OUTPUT_ROOT}"
        "LUX_CODEGEN_${ARGS_NAME}_OUTPUT_SUFFIX" "${ARGS_OUTPUT_SUFFIX}"
        "LUX_CODEGEN_${ARGS_NAME}_JSON_FIELDS" "${ARGS_JSON_FIELD}"
        "LUX_CODEGEN_${ARGS_NAME}_FLAT" "${ARGS_FLAT_OUTPUT}"
        "LUX_CODEGEN_${ARGS_NAME}_SERIAL_META" "${ARGS_SERIAL_META}"
    )
endfunction()

function(lux_target_add_codegen)
    set(one_value_args TARGET JOB)
    set(multi_value_args PROJECTIONS)
    set(optional_args DONT_ADD_TO_SOURCE DONT_INCLUDE)
    cmake_parse_arguments(ARGS "${optional_args}" "${one_value_args}" "${multi_value_args}" ${ARGN})
    if(NOT ARGS_TARGET OR NOT TARGET "${ARGS_TARGET}" OR
       NOT ARGS_JOB OR NOT TARGET "${ARGS_JOB}")
        message(FATAL_ERROR "[lux_target_add_codegen] valid TARGET and JOB are required")
    endif()

    get_target_property(_known "${ARGS_JOB}" LUX_CODEGEN_PROJECTIONS)
    if(NOT ARGS_PROJECTIONS)
        set(ARGS_PROJECTIONS ${_known})
    endif()
    foreach(_projection IN LISTS ARGS_PROJECTIONS)
        if(NOT _projection IN_LIST _known)
            message(FATAL_ERROR
                "[lux_target_add_codegen] unknown projection '${_projection}'")
        endif()
    endforeach()

    get_target_property(_files "${ARGS_JOB}" LUX_CODEGEN_TARGET_FILES)
    get_target_property(_logical "${ARGS_JOB}" LUX_CODEGEN_LOGICAL_PATHS)
    get_target_property(_generator "${ARGS_JOB}" LUX_CODEGEN_GENERATOR)
    get_target_property(_marker "${ARGS_JOB}" LUX_CODEGEN_MARKER)
    get_target_property(_cc "${ARGS_JOB}" LUX_CODEGEN_COMPILE_COMMANDS)
    get_target_property(_source "${ARGS_JOB}" LUX_CODEGEN_SOURCE_FILE)
    get_target_property(_options "${ARGS_JOB}" LUX_CODEGEN_COMPILE_OPTIONS)
    get_target_property(_parse_included "${ARGS_JOB}" LUX_CODEGEN_PARSE_INCLUDED)
    get_target_property(_dry_run "${ARGS_JOB}" LUX_CODEGEN_DRY_RUN)

    if(NOT _source)
        get_target_property(_sources "${ARGS_TARGET}" SOURCES)
        foreach(_candidate IN LISTS _sources)
            if(_candidate MATCHES "\\.(cpp|cxx|cc)$")
                get_filename_component(_source "${_candidate}" ABSOLUTE
                    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
                break()
            endif()
        endforeach()
    endif()

    set(_config_dir "${CMAKE_CURRENT_BINARY_DIR}/lux_codegen")
    file(MAKE_DIRECTORY "${_config_dir}")
    set(_config "${_config_dir}/${ARGS_JOB}.json")
    _meta_json_escape(_e_marker "${_marker}")
    _meta_json_escape(_e_cc "${_cc}")
    _meta_json_escape(_e_source "${_source}")
    file(WRITE "${_config}" "{\n  \"marker\": \"${_e_marker}\",\n")
    file(APPEND "${_config}" "  \"compile_commands\": \"${_e_cc}\",\n")
    file(APPEND "${_config}" "  \"source_file\": \"${_e_source}\",\n")
    file(APPEND "${_config}" "  \"target_files\": [\n")
    list(LENGTH _files _count)
    math(EXPR _last "${_count} - 1")
    foreach(_index RANGE 0 ${_last})
        list(GET _files ${_index} _file)
        list(GET _logical ${_index} _path)
        _meta_json_escape(_e_file "${_file}")
        _meta_json_escape(_e_path "${_path}")
        if(_index LESS _last)
            set(_comma ",")
        else()
            set(_comma "")
        endif()
        file(APPEND "${_config}"
            "    {\"physical_path\": \"${_e_file}\", \"logical_path\": \"${_e_path}\"}${_comma}\n")
    endforeach()
    file(APPEND "${_config}" "  ],\n  \"projections\": [\n")

    set(_outputs "")
    set(_templates "")
    list(LENGTH ARGS_PROJECTIONS _projection_count)
    math(EXPR _projection_last "${_projection_count} - 1")
    foreach(_pi RANGE 0 ${_projection_last})
        list(GET ARGS_PROJECTIONS ${_pi} _projection)
        get_target_property(_template "${ARGS_JOB}" "LUX_CODEGEN_${_projection}_TEMPLATE")
        get_target_property(_root "${ARGS_JOB}" "LUX_CODEGEN_${_projection}_OUTPUT_ROOT")
        get_target_property(_suffix "${ARGS_JOB}" "LUX_CODEGEN_${_projection}_OUTPUT_SUFFIX")
        get_target_property(_fields "${ARGS_JOB}" "LUX_CODEGEN_${_projection}_JSON_FIELDS")
        get_target_property(_flat "${ARGS_JOB}" "LUX_CODEGEN_${_projection}_FLAT")
        get_target_property(_serial "${ARGS_JOB}" "LUX_CODEGEN_${_projection}_SERIAL_META")
        list(APPEND _templates "${_template}")
        _meta_json_escape(_e_name "${_projection}")
        _meta_json_escape(_e_template "${_template}")
        _meta_json_escape(_e_root "${_root}")
        _meta_json_escape(_e_suffix "${_suffix}")
        if(_flat)
            set(_relative false)
        else()
            set(_relative true)
        endif()
        if(_serial)
            set(_serial_json true)
        else()
            set(_serial_json false)
        endif()
        file(APPEND "${_config}"
            "    {\"name\": \"${_e_name}\", \"template_path\": \"${_e_template}\", \"output_root\": \"${_e_root}\", \"output_suffix\": \"${_e_suffix}\", \"include_relative\": ${_relative}, \"serial_meta\": ${_serial_json}, \"custom_fields_json\": [")
        set(_first_field TRUE)
        foreach(_field IN LISTS _fields)
            _meta_json_escape(_e_field "${_field}")
            if(NOT _first_field)
                file(APPEND "${_config}" ", ")
            endif()
            file(APPEND "${_config}" "\"${_e_field}\"")
            set(_first_field FALSE)
        endforeach()
        if(_pi LESS _projection_last)
            set(_comma ",")
        else()
            set(_comma "")
        endif()
        file(APPEND "${_config}" "]}${_comma}\n")

        foreach(_index RANGE 0 ${_last})
            list(GET _logical ${_index} _path)
            if(_flat)
                get_filename_component(_path "${_path}" NAME)
            endif()
            get_filename_component(_directory "${_path}" DIRECTORY)
            get_filename_component(_name "${_path}" NAME_WE)
            list(APPEND _outputs "${_root}/${_directory}/${_name}${_suffix}")
        endforeach()
    endforeach()
    file(APPEND "${_config}" "  ],\n  \"extra_compile_options\": [")
    set(_first_option TRUE)
    foreach(_option IN LISTS _options)
        _meta_json_escape(_e_option "${_option}")
        if(NOT _first_option)
            file(APPEND "${_config}" ", ")
        endif()
        file(APPEND "${_config}" "\"${_e_option}\"")
        set(_first_option FALSE)
    endforeach()
    if(_parse_included)
        set(_parse_json true)
    else()
        set(_parse_json false)
    endif()
    if(_dry_run)
        set(_dry_json true)
    else()
        set(_dry_json false)
    endif()
    file(APPEND "${_config}"
        "],\n  \"parse_included_marked\": ${_parse_json},\n  \"dry_run\": ${_dry_json}\n}\n")

    list(REMOVE_DUPLICATES _outputs)
    list(LENGTH _outputs _unique_output_count)
    math(EXPR _expected_output_count "${_count} * ${_projection_count}")
    if(NOT _unique_output_count EQUAL _expected_output_count)
        message(FATAL_ERROR "[lux_target_add_codegen] configured output collision")
    endif()

    add_custom_command(
        OUTPUT ${_outputs}
        COMMAND "${_generator}" "${_config}"
        DEPENDS "${_generator}" ${_files} ${_templates} "${_config}"
        COMMENT "[lux_target_add_codegen] ${ARGS_JOB}: parse once, render ${_projection_count} projection(s)"
        VERBATIM
    )
    set(_generation_target "${ARGS_JOB}_generate")
    add_custom_target("${_generation_target}" DEPENDS ${_outputs})
    add_dependencies("${ARGS_TARGET}" "${_generation_target}")
    if(NOT ARGS_DONT_ADD_TO_SOURCE)
        target_sources("${ARGS_TARGET}" PRIVATE ${_outputs})
        set_source_files_properties(${_outputs} PROPERTIES GENERATED TRUE)
    endif()
    if(NOT ARGS_DONT_INCLUDE)
        foreach(_projection IN LISTS ARGS_PROJECTIONS)
            get_target_property(_root "${ARGS_JOB}" "LUX_CODEGEN_${_projection}_OUTPUT_ROOT")
            target_include_directories("${ARGS_TARGET}" PRIVATE "${_root}")
        endforeach()
    endif()
endfunction()

# -------------------------------------------------
# add_meta:
#   Defines a meta information generation object. Parameters (required/optional):
#
#   Required parameters (one_value_args):
#       NAME                <MetaObjectName>        -- Unique name for storing target properties and later use.
#       MARKER              <marker>                -- Annotation marker for C++ declarations.
#       TEMPLATE            <template_path>         -- Custom generation template path (using inja syntax).
#       OUT_DIR             <out_dir>               -- Output directory for generated files.
#       COMPILE_COMMANDS    <compile_commands>      -- Path to compile_commands file (usually generated by the build system).
#       META_SUFFIX         <meta_suffix>           -- Suffix for the generated meta information files.
#       SOURCE_FILE         <source_file>           -- Source file from compile_commands used to locate the compile options (if empty, supply options via EXTRA_COMPILE_OPTIONS).
#       SERIAL_META         <ON/OFF>                -- Whether to serialize parsed meta information to a JSON file (default is ON).
#       DRY_RUN             <ON/OFF>                -- Whether to perform a dry run (no files generated).
#
#   Multi-value parameters (multi_value_args):
#       TARGET_FILES        file1.hpp file2.hpp ...   -- List of files to generate meta information for.
#       EXTRA_COMPILE_OPTIONS <option1> <option2> ...  -- Additional compile options.
#
#   Optional parameters:
#       ECHO                -- Print verbose information (for debugging).
#       ALWAYS_REGENERATE   -- Force regeneration every time.
#
function(add_meta)
    set(one_value_args
        NAME
        GENERATOR
        MARKER
        TEMPLATE
        OUT_DIR
        COMPILE_COMMANDS
        META_SUFFIX
        SOURCE_FILE
        SERIAL_META
        DRY_RUN
        PARSE_INCLUDED_MARKED
    )
    set(multi_value_args
        TARGET_FILES
        EXTRA_COMPILE_OPTIONS
        JSON_FIELD
    )
    set(optional_args ECHO ALWAYS_REGENERATE)
    cmake_parse_arguments(ARGS "${optional_args}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT ARGS_NAME)
        message(FATAL_ERROR "[add_meta] NAME parameter is required")
    endif()

    set(_meta_name "${ARGS_NAME}")

    if(TARGET ${_meta_name})
        message(WARNING "[add_meta] Target '${_meta_name}' already exists; its properties will be overwritten.")
    else()
        add_custom_target(${_meta_name} ALL
            COMMENT "[add_meta] Placeholder target for meta object '${_meta_name}'."
        )
    endif()

    # Set default values
    if(NOT ARGS_OUT_DIR)
        set(ARGS_OUT_DIR "${CMAKE_BINARY_DIR}/metagen")
    endif()
    if(NOT ARGS_COMPILE_COMMANDS)
        set(ARGS_COMPILE_COMMANDS "${CMAKE_BINARY_DIR}/compile_commands.json")
        # if(NOT EXISTS ${ARGS_COMPILE_COMMANDS})
        #     message(FATAL_ERROR "[add_meta] COMPILE_COMMANDS file not found: ${ARGS_COMPILE_COMMANDS}. Please generate it with your build system.")
        # endif()
    endif()
    if(NOT ARGS_META_SUFFIX)
        set(ARGS_META_SUFFIX ".meta.cpp")
    endif()
    if(NOT ARGS_SOURCE_FILE)
        set(ARGS_SOURCE_FILE "")  # If empty, please supply compile options via EXTRA_COMPILE_OPTIONS.
    endif()
    if(NOT ARGS_SERIAL_META)
        set(ARGS_SERIAL_META ON)
    endif()
    if(NOT ARGS_DRY_RUN)
        set(ARGS_DRY_RUN OFF)
    endif()
    if(NOT ARGS_PARSE_INCLUDED_MARKED)
        # OFF keeps the main-file-only guard; ON also parses marked declarations
        # from headers the target file includes (see ParseOptions in CxxParser.hpp).
        set(ARGS_PARSE_INCLUDED_MARKED OFF)
    endif()

    # Locate the generator. Default: the namespaced target lux::cxx::lux_meta_generator
    # (the in-tree alias, or the IMPORTED target synthesised at the top of this file
    # from the installed binary); fall back to a bare find_program. Consumers should
    # NOT need to pass GENERATOR at all.
    if(NOT ARGS_GENERATOR)
        if(TARGET lux::cxx::lux_meta_generator)
            set(LUX_META_GENERATOR "$<TARGET_FILE:lux::cxx::lux_meta_generator>")
        else()
            find_program(LUX_META_GENERATOR lux_meta_generator REQUIRED)
            if(NOT LUX_META_GENERATOR)
                message(FATAL_ERROR "[add_meta] Could not find the lux_meta_generator executable")
            endif()
        endif()
    else()
        set(LUX_META_GENERATOR "${ARGS_GENERATOR}")
        if(ARGS_ECHO)
            message(STATUS "[add_meta] using generator: ${LUX_META_GENERATOR}")
        endif()
    endif()

    # Store configuration in target properties (prefix properties with META_)
    set_target_properties(${_meta_name} PROPERTIES
        META_GENERATOR              "${LUX_META_GENERATOR}"
        META_MARKER                 "${ARGS_MARKER}"
        META_TEMPLATE_PATH          "${ARGS_TEMPLATE}"
        META_OUT_DIR                "${ARGS_OUT_DIR}"
        META_COMPILE_COMMANDS       "${ARGS_COMPILE_COMMANDS}"
        META_TARGET_FILES           "${ARGS_TARGET_FILES}"
        META_META_SUFFIX            "${ARGS_META_SUFFIX}"
        META_SOURCE_FILE            "${ARGS_SOURCE_FILE}"
        META_EXTRA_COMPILE_OPTIONS  "${ARGS_EXTRA_COMPILE_OPTIONS}"
        META_SERIAL_META            "${ARGS_SERIAL_META}"
        META_DRY_RUN                "${ARGS_DRY_RUN}"
        META_PARSE_INCLUDED_MARKED  "${ARGS_PARSE_INCLUDED_MARKED}"
        META_ECHO                   "${ARGS_ECHO}"
        META_JSON_FIELD             "${ARGS_JSON_FIELD}"
    )
endfunction()

# -------------------------------------------------
# meta_add_files:
#   Appends additional files to the TARGET_FILES property of the specified meta object.
#   Usage:
#       meta_add_files(<MetaObjectName> TARGET_FILES file1.hpp file2.hpp ...)
#
function(meta_add_files)
    if("${ARGV0}" STREQUAL "")
        message(FATAL_ERROR "[meta_add_files] The first argument must be the MetaObjectName")
    endif()
    set(_meta_name "${ARGV0}")
    list(REMOVE_AT ARGV 0)
    if(NOT ARGV)
        message(FATAL_ERROR "[meta_add_files] At least one TARGET_FILES parameter is required")
    endif()
    get_target_property(_existing_files ${_meta_name} META_TARGET_FILES)
    if(NOT _existing_files)
        set(_existing_files "")
    endif()
    list(APPEND _existing_files ${ARGV})
    list(REMOVE_DUPLICATES _existing_files)
    set_target_properties(${_meta_name} PROPERTIES
        META_TARGET_FILES "${_existing_files}"
    )
endfunction()

# -------------------------------------------------
# target_add_meta:
#   Reads the TARGET_FILES information from the Meta object, writes a JSON configuration file,
#   calls the generator executable to produce output files, and adds the generated files to the target.
#
#   Usage:
#       target_add_meta(
#           NAME   <MetaObjectName>
#           TARGET <YourTarget>
#           [ALWAYS_REGENERATE]
#           [ECHO]
#       )
function(target_add_meta)
    set(one_value_args NAME TARGET)
    set(optional_args ALWAYS_REGENERATE ECHO DONT_ADD_TO_SOURCE DONT_INCLUDE)
    # NOTE: the option list must be passed as the <options> argument; it used to be
    # "" here, so ALWAYS_REGENERATE / ECHO / DONT_ADD_TO_SOURCE / DONT_INCLUDE were
    # silently never parsed (always false).
    cmake_parse_arguments(ARGS "${optional_args}" "${one_value_args}" "" ${ARGN})

    if(NOT ARGS_NAME)
        message(FATAL_ERROR "[target_add_meta] NAME parameter is required")
    endif()
    if(NOT ARGS_TARGET)
        message(FATAL_ERROR "[target_add_meta] TARGET parameter is required")
    endif()

    set(_meta_name "${ARGS_NAME}")
    set(_target_name "${ARGS_TARGET}")

    # Retrieve configuration from the Meta object
    get_target_property(_meta_out_dir  ${_meta_name} META_OUT_DIR)
    get_target_property(_meta_cc_json  ${_meta_name} META_COMPILE_COMMANDS)
    get_target_property(_meta_template ${_meta_name} META_TEMPLATE_PATH)
    get_target_property(_meta_marker   ${_meta_name} META_MARKER)
    get_target_property(_meta_target_files ${_meta_name} META_TARGET_FILES)
    get_target_property(_meta_meta_suffix ${_meta_name} META_META_SUFFIX)
    get_target_property(_meta_source_file ${_meta_name} META_SOURCE_FILE)
    get_target_property(_meta_extra_compile_options ${_meta_name} META_EXTRA_COMPILE_OPTIONS)
    get_target_property(_meta_serial_meta ${_meta_name} META_SERIAL_META)
    get_target_property(_meta_dry_run ${_meta_name} META_DRY_RUN)
    get_target_property(_meta_parse_included ${_meta_name} META_PARSE_INCLUDED_MARKED)
    get_target_property(_meta_echo   ${_meta_name} META_ECHO)
    get_target_property(_meta_gen_exe ${_meta_name} META_GENERATOR)
    get_target_property(_meta_json_fields ${_meta_name} META_JSON_FIELD)

    if(NOT _meta_out_dir)
        set(_meta_out_dir "${CMAKE_BINARY_DIR}/metagen")
    endif()
    if(NOT _meta_cc_json)
        set(_meta_cc_json "${CMAKE_BINARY_DIR}/compile_commands.json")
    endif()
    if(NOT _meta_template)
        message(FATAL_ERROR "[target_add_meta] META_TEMPLATE_PATH (TEMPLATE parameter) is not provided in ${_meta_name}")
    endif()
    if(NOT _meta_meta_suffix)
        set(_meta_meta_suffix ".meta.cpp")
    endif()
    if(NOT _meta_serial_meta)
        set(_meta_serial_meta ON)
    endif()
    if(NOT _meta_dry_run)
        set(_meta_dry_run OFF)
    endif()

    # If SOURCE_FILE was supplied explicitly via create_meta(), use it directly;
    # otherwise try to deduce a .cpp source from the target's own source list.
    if(_meta_source_file AND NOT _meta_source_file STREQUAL "NOTFOUND" AND NOT _meta_source_file STREQUAL "")
        set(_main_src "${_meta_source_file}")
    else()
        # Get target source files to determine the entry source for compile_commands
        get_target_property(_srcs "${_target_name}" SOURCES)
        if(NOT _srcs OR _srcs STREQUAL "NOTFOUND")
            set(_main_src "")
        else()
            set(_main_src "")
            foreach(_s IN LISTS _srcs)
                get_filename_component(_ext "${_s}" EXT)
                if(_ext STREQUAL ".cpp" OR _ext STREQUAL ".cxx" OR _ext STREQUAL ".cc")
                    set(_main_src "${_s}")
                    break()
                endif()
            endforeach()
        endif()
    endif()

    # Normalise to an absolute path. The generator matches source_file against
    # compile_commands.json (whose keys are absolute) and against the filesystem,
    # so a relative entry like "main.cpp" — which is exactly what a natural
    # add_executable(app main.cpp) stores in SOURCES — would not be found.
    # Resolve it against the directory in which the target was defined.
    if(_main_src AND NOT IS_ABSOLUTE "${_main_src}")
        get_target_property(_tgt_src_dir "${_target_name}" SOURCE_DIR)
        if(_tgt_src_dir AND NOT _tgt_src_dir STREQUAL "NOTFOUND")
            get_filename_component(_main_src "${_main_src}" ABSOLUTE BASE_DIR "${_tgt_src_dir}")
        else()
            get_filename_component(_main_src "${_main_src}" ABSOLUTE)
        endif()
    endif()

    # If we still have no source file AND no extra compile options, the generator will
    # be unable to locate include paths — abort with a clear diagnostic.
    if(_main_src STREQUAL "")
        if(NOT _meta_extra_compile_options OR _meta_extra_compile_options STREQUAL "NOTFOUND")
            message(FATAL_ERROR
                "[target_add_meta] No source_file could be determined for target '${_target_name}' "
                "and no EXTRA_COMPILE_OPTIONS were provided. Provide either SOURCE_FILE in "
                "create_meta() or EXTRA_COMPILE_OPTIONS with the necessary -I flags.")
        else()
            message(STATUS "[target_add_meta] No .cpp source found for '${_target_name}'; "
                "relying on EXTRA_COMPILE_OPTIONS for include paths.")
        endif()
    endif()

    if(NOT _meta_target_files)
        message(FATAL_ERROR "[target_add_meta] The TARGET_FILES (META_TARGET_FILES) for '${_meta_name}' is empty.")
    endif()
    # Convert _meta_target_files (semicolon-separated) to a list
    set(_meta_files ${_meta_target_files})

    # Generate JSON configuration file with new parameters.
    # Every interpolated value is JSON-escaped (paths may contain backslashes on
    # Windows, e.g. C:\Users\...; raw insertion would emit invalid JSON escapes).
    set(_config_file "${_meta_out_dir}/${_meta_name}_meta_config.json")
    _meta_json_escape(_e_marker   "${_meta_marker}")
    _meta_json_escape(_e_template "${_meta_template}")
    _meta_json_escape(_e_outdir   "${_meta_out_dir}")
    _meta_json_escape(_e_cc       "${_meta_cc_json}")
    _meta_json_escape(_e_src      "${_main_src}")
    _meta_json_escape(_e_suffix   "${_meta_meta_suffix}")

    file(WRITE "${_config_file}" "{\n")
    file(APPEND "${_config_file}" "  \"marker\": \"${_e_marker}\",\n")
    file(APPEND "${_config_file}" "  \"template_path\": \"${_e_template}\",\n")
    file(APPEND "${_config_file}" "  \"out_dir\": \"${_e_outdir}\",\n")
    file(APPEND "${_config_file}" "  \"compile_commands\": \"${_e_cc}\",\n")
    file(APPEND "${_config_file}" "  \"source_file\": \"${_e_src}\",\n")
    file(APPEND "${_config_file}" "  \"target_files\": [\n")
    list(LENGTH _meta_files _mf_len)
    math(EXPR _last_idx "${_mf_len} - 1")
    foreach(i RANGE 0 ${_last_idx})
        list(GET _meta_files ${i} mf)
        _meta_json_escape(_e_mf "${mf}")
        if(i LESS _last_idx)
            file(APPEND "${_config_file}" "    \"${_e_mf}\",\n")
        else()
            file(APPEND "${_config_file}" "    \"${_e_mf}\"\n")
        endif()
    endforeach()
    file(APPEND "${_config_file}" "  ],\n")
    file(APPEND "${_config_file}" "  \"meta_suffix\": \"${_e_suffix}\",\n")
    file(APPEND "${_config_file}" "  \"extra_compile_options\": [\n")
    if(_meta_extra_compile_options)
        # Target properties keep CMake lists as semicolon-separated values already.
        set(_compile_opts ${_meta_extra_compile_options})
        list(LENGTH _compile_opts _co_len)
        math(EXPR _co_last "${_co_len} - 1")
        foreach(i RANGE 0 ${_co_last})
            list(GET _compile_opts ${i} opt)
            _meta_json_escape(_e_opt "${opt}")
            if(i LESS _co_last)
                file(APPEND "${_config_file}" "    \"${_e_opt}\",\n")
            else()
                file(APPEND "${_config_file}" "    \"${_e_opt}\"\n")
            endif()
        endforeach()
    endif()
    file(APPEND "${_config_file}" "  ],\n")

    if("${_meta_serial_meta}" STREQUAL "ON")
      set(serial_meta_value true)
    else()
      set(serial_meta_value false)
    endif()
    if("${_meta_dry_run}" STREQUAL "ON")
      set(dry_run_value true)
    else()
      set(dry_run_value false)
    endif()
    if("${_meta_parse_included}" STREQUAL "ON")
      set(parse_included_value true)
    else()
      set(parse_included_value false)
    endif()

    file(APPEND "${_config_file}" "  \"serial_meta\": ${serial_meta_value},\n")
    file(APPEND "${_config_file}" "  \"parse_included_marked\": ${parse_included_value},\n")
    file(APPEND "${_config_file}" "  \"dry_run\": ${dry_run_value}\n")

    if(_meta_json_fields)
        file(APPEND "${_config_file}" ",\n  \"custom_fields_json\": [\n")
        list(LENGTH _meta_json_fields _len)
        math(EXPR _last "${_len} - 1")
    
        foreach(i RANGE 0 ${_last})
            list(GET _meta_json_fields ${i} field)
            _meta_json_escape(field_escaped "${field}")
            if(i LESS ${_last})
                file(APPEND "${_config_file}"
                    "    \"${field_escaped}\",\n")
            else()
                file(APPEND "${_config_file}"
                    "    \"${field_escaped}\"\n")
            endif()
        endforeach()
        file(APPEND "${_config_file}" "  ]\n")
    else()
        file(APPEND "${_config_file}" "\n")
    endif()

    file(APPEND "${_config_file}" "}\n")

    # For each target file, generate an output file (assuming each input file produces one output file with <basename><meta_suffix>)
    set(_all_generated_files "")
    set(_generated_source_files "")
    foreach(_mf IN LISTS _meta_files)
        get_filename_component(_base "${_mf}" NAME_WE)
        set(_generated_file "${_meta_out_dir}/${_base}${_meta_meta_suffix}")
        list(APPEND _all_generated_files "${_generated_file}")
        # If meta_suffix contains .cpp, treat it as a source file.
        string(FIND "${_meta_meta_suffix}" ".cpp" pos)
        if(NOT pos EQUAL -1)
            list(APPEND _generated_source_files "${_generated_file}")
        endif()
    endforeach()

    if(_meta_echo)
        message(STATUS "[target_add_meta] META_OUT_DIR = ${_meta_out_dir}")
        message(STATUS "  COMPILE_COMMANDS = ${_meta_cc_json}")
        message(STATUS "  TEMPLATE = ${_meta_template}")
        message(STATUS "  MARKER = ${_meta_marker}")
        message(STATUS "  META_SUFFIX = ${_meta_meta_suffix}")
        message(STATUS "  SOURCE_FILE = ${_main_src}")
        message(STATUS "  TARGET_FILES:")
        foreach(gen_file IN LISTS _all_generated_files)
            message(STATUS "-> ${gen_file}")
        endforeach()
    endif()

    if(NOT EXISTS "${_meta_out_dir}")
        file(MAKE_DIRECTORY "${_meta_out_dir}")
    endif()

    set(_meta_gen_target "${_meta_name}_gen")
    if(ARGS_ALWAYS_REGENERATE)
        # Force regeneration on EVERY build: a custom target's COMMAND always runs
        # (unlike add_custom_command OUTPUT, which is skipped when the outputs are
        # up to date). BYPRODUCTS lets consumers still depend on the generated files.
        # (The previous set_property(SOURCE ... SKIP_CACHE) was a no-op — SKIP_CACHE
        # is not a real source property and never forced anything.)
        add_custom_target("${_meta_gen_target}" ALL
            COMMAND "${_meta_gen_exe}" "${_config_file}"
            BYPRODUCTS ${_all_generated_files}
            DEPENDS
                "${_meta_gen_exe}"
                ${_meta_files}
                "${_meta_template}"
                "${_config_file}"
            COMMENT "[target_add_meta] (always) Generating meta for '${_meta_name}'"
            VERBATIM
        )
        if(_generated_source_files)
            # Generated .cpp added to a target's SOURCES must be flagged GENERATED
            # since no add_custom_command OUTPUT declares them in this branch.
            set_source_files_properties(${_generated_source_files} PROPERTIES GENERATED TRUE)
        endif()
    else()
        # Incremental: regenerate only when an input (header / template / config) changes.
        add_custom_command(
            OUTPUT ${_all_generated_files}
            COMMAND "${_meta_gen_exe}" "${_config_file}"
            DEPENDS
                "${_meta_gen_exe}"
                ${_meta_files}
                "${_meta_template}"
                "${_config_file}"
            COMMENT "[target_add_meta] Generating meta information for '${_meta_name}', command: ${_meta_gen_exe} ${_config_file}"
            VERBATIM
        )
        add_custom_target("${_meta_gen_target}"
            DEPENDS ${_all_generated_files}
        )
    endif()

    # Add generated source files to the user's target (if any .cpp files were generated)
    if(_generated_source_files AND NOT ARGS_DONT_ADD_TO_SOURCE)
        target_sources("${_target_name}" PRIVATE ${_generated_source_files})
    endif()
    add_dependencies("${_target_name}" "${_meta_gen_target}")
    if(NOT ARGS_DONT_INCLUDE)
        target_include_directories("${_target_name}" PRIVATE "${_meta_out_dir}")
    endif()

    if(_meta_echo)
        message(STATUS "[target_add_meta] Done: '${_target_name}' will build meta information from '${_meta_name}'.")
    endif()
endfunction()
