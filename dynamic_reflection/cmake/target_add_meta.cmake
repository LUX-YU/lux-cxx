# ===========================================
# Include guard
# ===========================================
if(_COMPONENT_META_TOOLS_INCLUDED_)
  return()
endif()
set(_COMPONENT_META_TOOLS_INCLUDED_ TRUE)

function(target_add_meta)
    # Usage:
    #   target_add_meta(
    #       TARGET            MyLibrary
    #       METAS             fileA.hpp fileB.hpp
    #       META_GEN_OUT_DIR  "path/to/out"
    #       [COMPILE_COMMANDS "path/to/compile_commands.json"]
    #       [META_GENERATOR   "path/to/lux_meta_generator"]
    #       [TARGET_OUTPUT    varNameToStoreMsg]
    #       [ECHO]
    #       [REGISTER_FUNCTION_HEADER_NAME "${META_GEN_OUT_DIR}/generated_header.hpp"]    # New: specify the aggregated header file path
    #       [REGISTER_FUNCTION_NAME_PREFIX "register_reflections_"]   # New: specify the function name prefix
    #   )
    # For each meta_file in METAS:
    #   1) compute a .meta.static.hpp and .meta.dynamic.cpp in META_GEN_OUT_DIR
    #   2) pick one .cpp from the target's SOURCES
    #   3) call the generator with five arguments:
    #         argv[1]: <meta_file>   (the .hpp to parse)
    #         argv[2]: <out_dir>     (the meta file output dir)
    #         argv[3]: <source_file> (the .cpp from the target)
    #         argv[4]: <compile_commands.json>
    #         argv[5]: <file_hash>   (sha256)
    #   4) attach these files to the specified TARGET via target_sources
    #   5) add META_GEN_OUT_DIR to that target's include paths
    #   6) produce a custom target that depends on the generated files

    set(one_value_args
        TARGET
        META_GEN_OUT_DIR
        COMPILE_COMMANDS
        META_GENERATOR
        TARGET_OUTPUT
        REGISTER_FUNCTION_HEADER_NAME
        REGISTER_FUNCTION_NAME_PREFIX
    )
    set(multi_value_args
        METAS
    )
    cmake_parse_arguments(
        ARGS
        "ECHO"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    if(NOT ARGS_TARGET)
        message(FATAL_ERROR "[target_add_meta] Missing required argument: TARGET")
    endif()

    if(NOT ARGS_METAS)
        message(FATAL_ERROR "[target_add_meta] Missing required argument: METAS")
    endif()

    set(_target_name ${ARGS_TARGET})

    # 1) Locate the meta generator
    if(ARGS_META_GENERATOR)
        set(_meta_gen_exe ${ARGS_META_GENERATOR})
    else()
        find_program(_meta_gen_exe lux_meta_generator)
    endif()
    if(NOT _meta_gen_exe)
        message(FATAL_ERROR "[target_add_meta] Could not find 'lux_meta_generator'!")
    endif()

    # 2) Default output directory
    if(NOT ARGS_META_GEN_OUT_DIR)
        set(ARGS_META_GEN_OUT_DIR "${CMAKE_BINARY_DIR}/metagen")
    endif()
    if(NOT EXISTS "${ARGS_META_GEN_OUT_DIR}")
        file(MAKE_DIRECTORY "${ARGS_META_GEN_OUT_DIR}")
    endif()

    # 3) Determine compile_commands.json path
    set(_compile_cmds "")
    if(ARGS_COMPILE_COMMANDS)
        set(_compile_cmds "${ARGS_COMPILE_COMMANDS}")
    else()
        # Attempt default: <binary_dir>/compile_commands.json
        set(_compile_cmds "${CMAKE_BINARY_DIR}/compile_commands.json")
    endif()

    # 4) Retrieve the target's source files
    get_target_property(_srcs "${_target_name}" SOURCES)
    if(_srcs STREQUAL "NOTFOUND" OR NOT _srcs)
        message(WARNING "[target_add_meta] Target '${_target_name}' has no sources. Will pass empty source_file.")
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
        if(_main_src STREQUAL "")
            message(WARNING "[target_add_meta] No .cpp found in target '${_target_name}'. Passing empty for 'source_file'.")
        endif()
    endif()

    # 5) Create a custom target to hold generated meta files
    set(_meta_custom_target "${_target_name}_meta")
    set(_generated_files "")
    set(_generated_source_files "")
    set(_reg_fn_names "")  # New: list to store register function names

    # For each METAS file => produce static/dynamic
    foreach(meta_file IN LISTS ARGS_METAS)
        get_filename_component(_meta_filename "${meta_file}" NAME_WE)

        file(SHA256 "${meta_file}" _file_hash)

        set(_static_file  "${ARGS_META_GEN_OUT_DIR}/${_meta_filename}.meta.static.hpp")
        set(_dynamic_file "${ARGS_META_GEN_OUT_DIR}/${_meta_filename}.meta.dynamic.cpp")

        list(APPEND _generated_files "${_static_file}" "${_dynamic_file}")
        list(APPEND _generated_source_files "${_dynamic_file}")

        # Construct the register function name using the file hash and optional prefix
        if(ARGS_REGISTER_FUNCTION_NAME_PREFIX)
            set(_fn_prefix "${ARGS_REGISTER_FUNCTION_NAME_PREFIX}")
        else()
            set(_fn_prefix "register_reflections_")
        endif()
        set(_reg_fn_name "${_fn_prefix}${_file_hash}")
        list(APPEND _reg_fn_names "${_reg_fn_name}")

        if(ARGS_ECHO)
            message(STATUS "[target_add_meta] Will execute: ${_meta_gen_exe} ${meta_file} ${ARGS_META_GEN_OUT_DIR} ${_main_src} ${_compile_cmds} ${_file_hash}")
        endif()

        add_custom_command(
            OUTPUT  "${_static_file}" "${_dynamic_file}"
            COMMAND "${_meta_gen_exe}"
                # argv[1] => meta_file
                "${meta_file}"
                # argv[2] => out_dir
                "${ARGS_META_GEN_OUT_DIR}"
                # argv[3] => source_file
                "${_main_src}"
                # argv[4] => compile_commands.json
                "${_compile_cmds}"
                # argv[5] => file_hash
                "${_file_hash}"
            DEPENDS "${meta_file}"
            COMMENT "[target_add_meta] Generating meta for ${meta_file} -> hash=${_file_hash}"
            VERBATIM
        )
    endforeach()

    add_custom_target("${_meta_custom_target}" ALL
        DEPENDS ${_generated_files}
    )

    # Attach .meta.* files to the main target for IDE listing & compilation,
    # only if there are generated source files.
    if(_generated_source_files)
        target_sources(${_target_name}
            PRIVATE
            ${_generated_source_files}
        )
    endif()

    # Add the out_dir to includes
    target_include_directories(${_target_name}
        PRIVATE
        "${ARGS_META_GEN_OUT_DIR}"
    )

    # Ensure main target depends on meta generation
    add_dependencies(${_target_name} "${_meta_custom_target}")

    # New: Generate a header file that aggregates all reflection registration functions
    if(NOT ARGS_REGISTER_FUNCTION_HEADER_NAME)
        set(ARGS_REGISTER_FUNCTION_HEADER_NAME "register_all_dynamic_meta.hpp")
    endif()

    set(ARGS_REGISTER_FUNCTION_HEADER_PATH "${ARGS_META_GEN_OUT_DIR}/${ARGS_REGISTER_FUNCTION_HEADER_NAME}")

    # Construct the declarations and function calls strings
    set(_register_function_decls "")
    set(_register_function_calls "")
    foreach(fn IN LISTS _reg_fn_names)
        set(_decl "extern void ${fn}(lux::cxx::dref::runtime::RuntimeRegistry& registry)\\;")
        set(_call "    ${fn}(registry)\\;")
        list(APPEND _register_function_decls "${_decl}")
        list(APPEND _register_function_calls "${_call}")
    endforeach()
    list(JOIN _register_function_decls "\n" _register_function_decls_str)
    list(JOIN _register_function_calls "\n" _register_function_calls_str)

    # Pass the generated strings to the template via cache variables for configure_file
    set(REGISTER_FUNCTION_DECLARATIONS "${_register_function_decls_str}")
    set(REGISTER_FUNCTION_CALLS "${_register_function_calls_str}")

    # Configure the header template file.
    # The template file is expected to be located relative to this CMake file.
    set(_template_file "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/reflection_header_template.hpp.in")
    configure_file(
        "${_template_file}"
        "${ARGS_REGISTER_FUNCTION_HEADER_PATH}"
        @ONLY
    )

    # Optionally output a message to the user.
    message(STATUS "[target_add_meta] Generated aggregated header: ${ARGS_REGISTER_FUNCTION_HEADER_PATH}")

    # If user wants info in a variable
    if(ARGS_TARGET_OUTPUT)
        set(${ARGS_TARGET_OUTPUT} "[target_add_meta] Created custom target '${_meta_custom_target}' for '${_target_name}'" PARENT_SCOPE)
    endif()
endfunction()
