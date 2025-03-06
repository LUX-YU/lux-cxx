# ===========================================
# Include guard
# ===========================================
if(_COMPONENT_META_TOOLS_INCLUDED_)
  return()
endif()
set(_COMPONENT_META_TOOLS_INCLUDED_ TRUE)

# This function is used to generate reflection metadata for given files.
# It will call the meta generator tool (lux_meta_generator) with arguments:
#   argv[1]: The file path to parse
#   argv[2]: The "include path"
#   argv[3]: The output directory
#   argv[4]: A source file in compile_commands.json
#   argv[5]: compile_commands.json
#
# Example usage:
#   add_meta(
#       TARGET_NAME        MyMetaTarget
#       SOURCE_FILE  my_source.cpp
#       INCLUDE_PATH       "path/to/original.hpp"
#       METAS              file_a.hpp file_b.hpp
#       COMPILE_COMMANDS   ${CMAKE_BINARY_DIR}/compile_commands.json
#   )
#
# Arguments:
#   TARGET_NAME            (Required)  A custom target name.
#   SOURCE_FILE            (Required)  A source file that appears in compile_commands.
#   INCLUDE_PATH           (Required)  The original include path to be injected into the .cpp.
#   METAS                  (Required)  A list of input files for reflection.
#   META_GEN_OUT_DIR       (Optional)  Output directory for generated files (default: ${CMAKE_BINARY_DIR}/metagen).
#   META_GENERATOR         (Optional)  Path to lux_meta_generator exe (if not found automatically).
#   META_IDENTITIES        (Optional)  A var name to store the list of meta file hashes.
#   COMPILE_COMMANDS       (Required)  Path to compile_commands.json
#   RESULT_OUTPUT          (Optional)  A var name to store generation summary message.
#   SOURCE_FILES_OUTPUT    (Optional)  A var name to store the list of generated files.
function(add_meta)
    set(_one_value_arguments
        TARGET_NAME
        SOURCE_FILE
        INCLUDE_PATH
        META_GEN_OUT_DIR
        META_GENERATOR
        META_IDENTITIES
        COMPILE_COMMANDS
        RESULT_OUTPUT
        SOURCE_FILES_OUTPUT
    )
    set(_multi_value_arguments
        METAS
    )
    cmake_parse_arguments(
        ARGS
        ""
        "${_one_value_arguments}"
        "${_multi_value_arguments}"
        ${ARGN}
    )

    # Mandatory checks
    if(NOT ARGS_TARGET_NAME)
        message(FATAL_ERROR "TARGET_NAME is required!")
    endif()
    set(target_name ${ARGS_TARGET_NAME})

    if(NOT ARGS_SOURCE_FILE)
        message(FATAL_ERROR "SOURCE_FILE is required!")
    endif()

    if(NOT ARGS_METAS)
        message(FATAL_ERROR "No meta files provided via METAS!")
    endif()

    if(NOT ARGS_INCLUDE_PATH)
        message(FATAL_ERROR "INCLUDE_PATH is required!")
    endif()

    if(NOT ARGS_COMPILE_COMMANDS)
        message(FATAL_ERROR "COMPILE_COMMANDS is required for meta generation!")
    endif()

    # If META_GENERATOR is not specified, try to find 'lux_meta_generator'
    if(ARGS_META_GENERATOR)
        set(META_GENERATOR ${ARGS_META_GENERATOR})
    else()
        find_program(META_GENERATOR lux_meta_generator)
    endif()
    if(NOT META_GENERATOR)
        message(FATAL_ERROR "Could not find meta generator 'lux_meta_generator'!")
    endif()

    # Default output dir
    if(NOT ARGS_META_GEN_OUT_DIR)
        set(ARGS_META_GEN_OUT_DIR "${CMAKE_BINARY_DIR}/metagen")
    endif()
    if(NOT EXISTS "${ARGS_META_GEN_OUT_DIR}")
        file(MAKE_DIRECTORY "${ARGS_META_GEN_OUT_DIR}")
    endif()

    set(GENERATED_META_FILES "")
    set(GENERATED_META_HASHS "")
    foreach(meta_file IN LISTS ARGS_METAS)
        # e.g. if meta_file= Foo.hpp => meta_filename=Foo
        get_filename_component(meta_filename "${meta_file}" NAME_WE)
        file(SHA256 "${meta_file}" meta_hash)
        list(APPEND ${ARGS_META_IDENTITIES} ${meta_hash})

        # We'll produce two outputs:
        set(static_file  "${ARGS_META_GEN_OUT_DIR}/${meta_filename}.meta.static.hpp")
        set(dynamic_file "${ARGS_META_GEN_OUT_DIR}/${meta_filename}.meta.dynamic.cpp")

        # We record them
        list(APPEND GENERATED_META_FILES "${dynamic_file}")
        list(APPEND GENERATED_META_HASHS "${meta_hash}")

        add_custom_command(
            OUTPUT "${static_file}" "${dynamic_file}"
            COMMAND "${META_GENERATOR}"
                    "${meta_file}"                # argv[1]: The file to parse
                    "${ARGS_INCLUDE_PATH}"        # argv[2]: The "include path"
                    "${meta_hash}"                # argv[3]: The hash of the file
                    "${ARGS_META_GEN_OUT_DIR}"    # argv[3]: Output directory
                    "${ARGS_SOURCE_FILE}"         # argv[4]: A source file in compile_commands
                    "${ARGS_COMPILE_COMMANDS}"    # argv[5]: compile_commands.json
            DEPENDS "${meta_file}"
            COMMENT "Generating meta for ${meta_file}"
            VERBATIM
        )
    endforeach()

    # Create a custom target depending on all generated meta files
    add_custom_target("${target_name}" ALL
        DEPENDS ${GENERATED_META_FILES}
    )

    # Optionally store info in user-specified variable
    if(ARGS_RESULT_OUTPUT)
        set(${ARGS_RESULT_OUTPUT} "Custom target '${target_name}' created" PARENT_SCOPE)
    endif()

    if(ARGS_SOURCE_FILES_OUTPUT)
        set(${ARGS_SOURCE_FILES_OUTPUT} "${GENERATED_META_FILES}" PARENT_SCOPE)
    endif()

    if(ARGS_META_IDENTITIES)
        set(${ARGS_META_IDENTITIES} "${GENERATED_META_HASHS}" PARENT_SCOPE)
    endif()
endfunction()
