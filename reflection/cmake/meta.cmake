# ===========================================
# Include guard
# ===========================================
if(_COMPONENT_META_TOOLS_INCLUDED_)
  return()
endif()
set(_COMPONENT_META_TOOLS_INCLUDED_ TRUE)
cmake_policy(PUSH)
if(POLICY CMP0116)
    cmake_policy(SET CMP0116 NEW)
endif()

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
    string(REPLACE "\n" "\\n" _t "${_t}")
    string(REPLACE "\r" "\\r" _t "${_t}")
    string(REPLACE "\t" "\\t" _t "${_t}")
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
    set(multi_value_args TARGET_FILES LOGICAL_PATHS EXTRA_COMPILE_OPTIONS DEPENDS)
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
        LUX_CODEGEN_DEPENDS         "${ARGS_DEPENDS}"
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
        "LUX_CODEGEN_${ARGS_NAME}_VALIDATION" FALSE
    )
endfunction()

function(lux_codegen_add_validation)
    set(one_value_args JOB NAME TEMPLATE)
    set(multi_value_args JSON_FIELD)
    cmake_parse_arguments(ARGS "" "${one_value_args}" "${multi_value_args}" ${ARGN})
    if(NOT ARGS_JOB OR NOT TARGET "${ARGS_JOB}" OR NOT ARGS_NAME OR
       NOT ARGS_TEMPLATE)
        message(FATAL_ERROR
            "[lux_codegen_add_validation] JOB, NAME and TEMPLATE are required")
    endif()

    get_target_property(_projections "${ARGS_JOB}" LUX_CODEGEN_PROJECTIONS)
    if(ARGS_NAME IN_LIST _projections)
        message(FATAL_ERROR
            "[lux_codegen_add_validation] duplicate projection '${ARGS_NAME}'")
    endif()
    list(APPEND _projections "${ARGS_NAME}")
    set_target_properties("${ARGS_JOB}" PROPERTIES
        LUX_CODEGEN_PROJECTIONS "${_projections}"
        "LUX_CODEGEN_${ARGS_NAME}_TEMPLATE" "${ARGS_TEMPLATE}"
        "LUX_CODEGEN_${ARGS_NAME}_OUTPUT_ROOT" "${CMAKE_CURRENT_BINARY_DIR}/lux_codegen/validation"
        "LUX_CODEGEN_${ARGS_NAME}_OUTPUT_SUFFIX" ".${ARGS_NAME}.validation.json"
        "LUX_CODEGEN_${ARGS_NAME}_JSON_FIELDS" "${ARGS_JSON_FIELD}"
        "LUX_CODEGEN_${ARGS_NAME}_FLAT" FALSE
        "LUX_CODEGEN_${ARGS_NAME}_SERIAL_META" FALSE
        "LUX_CODEGEN_${ARGS_NAME}_VALIDATION" TRUE
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
    get_target_property(_depends "${ARGS_JOB}" LUX_CODEGEN_DEPENDS)
    if(NOT _depends)
        set(_depends "")
    endif()
    if(MSVC)
        if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
            list(APPEND _options --target=x86_64-pc-windows-msvc)
        elseif(CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "X86")
            list(APPEND _options --target=i686-pc-windows-msvc)
        elseif(CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "ARM64")
            list(APPEND _options --target=aarch64-pc-windows-msvc)
        else()
            message(FATAL_ERROR "[lux_target_add_codegen] unsupported MSVC target architecture")
        endif()
    endif()
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
    set(_depfile "${_config_dir}/${ARGS_JOB}.d")
    _meta_json_escape(_e_depfile "${_depfile}")
    _meta_json_escape(_e_marker "${_marker}")
    _meta_json_escape(_e_cc "${_cc}")
    _meta_json_escape(_e_source "${_source}")
    set(_config_contents "{\n  \"marker\": \"${_e_marker}\",\n")
    string(APPEND _config_contents "  \"compile_commands\": \"${_e_cc}\",\n  \"depfile\": \"${_e_depfile}\",\n")
    string(APPEND _config_contents "  \"source_file\": \"${_e_source}\",\n")
    string(APPEND _config_contents "  \"target_files\": [\n")
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
        string(APPEND _config_contents
            "    {\"physical_path\": \"${_e_file}\", \"logical_path\": \"${_e_path}\"}${_comma}\n")
    endforeach()
    string(APPEND _config_contents "  ],\n  \"projections\": [\n")

    set(_outputs "")
    set(_templates "")
    set(_publish_projection_count 0)
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
        get_target_property(_validation "${ARGS_JOB}" "LUX_CODEGEN_${_projection}_VALIDATION")
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
        if(_validation)
            set(_validation_json true)
        else()
            set(_validation_json false)
            math(EXPR _publish_projection_count "${_publish_projection_count} + 1")
        endif()
        string(APPEND _config_contents
            "    {\"name\": \"${_e_name}\", \"template_path\": \"${_e_template}\", \"output_root\": \"${_e_root}\", \"output_suffix\": \"${_e_suffix}\", \"include_relative\": ${_relative}, \"serial_meta\": ${_serial_json}, \"validation\": ${_validation_json}, \"custom_fields_json\": [")
        set(_first_field TRUE)
        foreach(_field IN LISTS _fields)
            _meta_json_escape(_e_field "${_field}")
            if(NOT _first_field)
                string(APPEND _config_contents ", ")
            endif()
            string(APPEND _config_contents "\"${_e_field}\"")
            set(_first_field FALSE)
        endforeach()
        if(_pi LESS _projection_last)
            set(_comma ",")
        else()
            set(_comma "")
        endif()
        string(APPEND _config_contents "]}${_comma}\n")

        if(NOT _validation)
            foreach(_index RANGE 0 ${_last})
                list(GET _logical ${_index} _path)
                if(_flat)
                    get_filename_component(_path "${_path}" NAME)
                endif()
                get_filename_component(_directory "${_path}" DIRECTORY)
                get_filename_component(_name "${_path}" NAME_WE)
                list(APPEND _outputs "${_root}/${_directory}/${_name}${_suffix}")
            endforeach()
        endif()
    endforeach()
    string(APPEND _config_contents "  ],\n  \"extra_compile_options\": [")
    set(_first_option TRUE)
    foreach(_option IN LISTS _options)
        _meta_json_escape(_e_option "${_option}")
        if(NOT _first_option)
            string(APPEND _config_contents ", ")
        endif()
        string(APPEND _config_contents "\"${_e_option}\"")
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
    string(APPEND _config_contents
        "],\n  \"parse_included_marked\": ${_parse_json},\n  \"dry_run\": ${_dry_json}\n}\n")

    file(GENERATE OUTPUT "${_config}" CONTENT "${_config_contents}")

    list(REMOVE_DUPLICATES _outputs)
    list(LENGTH _outputs _unique_output_count)
    math(EXPR _expected_output_count "${_count} * ${_publish_projection_count}")
    if(NOT _unique_output_count EQUAL _expected_output_count)
        message(FATAL_ERROR "[lux_target_add_codegen] configured output collision")
    endif()

    add_custom_command(
        OUTPUT ${_outputs}
        COMMAND "${_generator}" "${_config}"
        DEPENDS "${_generator}" ${_files} ${_templates} ${_depends} "${_config}" "${_cc}"
        DEPFILE "${_depfile}"
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

cmake_policy(POP)
