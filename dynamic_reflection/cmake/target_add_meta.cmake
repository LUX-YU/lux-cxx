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
    #   )
    #
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
    )
    set(multi_value_args
        METAS
    )
    cmake_parse_arguments(
        ARGS
        ""
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

    # For each METAS file => produce static/dynamic
    foreach(meta_file IN LISTS ARGS_METAS)
        get_filename_component(_meta_filename "${meta_file}" NAME_WE)

        file(SHA256 "${meta_file}" _file_hash)

        set(_static_file  "${ARGS_META_GEN_OUT_DIR}/${_meta_filename}.meta.static.hpp")
        set(_dynamic_file "${ARGS_META_GEN_OUT_DIR}/${_meta_filename}.meta.dynamic.cpp")

        list(APPEND _generated_files "${_static_file}" "${_dynamic_file}")

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

    # Attach .meta.* files to the main target for IDE listing & compilation
    target_sources(${_target_name}
        PRIVATE
        ${_generated_files}
    )

    # Add the out_dir to includes
    target_include_directories(${_target_name}
        PRIVATE
        "${ARGS_META_GEN_OUT_DIR}"
    )

    # Ensure main target depends on meta generation
    add_dependencies(${_target_name} "${_meta_custom_target}")

    # If user wants info in a variable
    if(ARGS_TARGET_OUTPUT)
        set(${ARGS_TARGET_OUTPUT} "[target_add_meta] Created custom target '${_meta_custom_target}' for '${_target_name}'" PARENT_SCOPE)
    endif()
endfunction()
