# INCLUDE GUARD
if(_COMPONENT_META_TOOLS_INCLUDED_)
  return()
endif()
set(_COMPONENT_META_TOOLS_INCLUDED_ TRUE)

# KEY1             TARGET_NAME         自定义的 target 名称（必选）
# KEY2             SOURCE_FILE         源文件路径（必选），用于从 compile_commands.json 中匹配条目
# KEY3             METAS               需要生成元数据的文件列表（必选）
# KEY4             META_GEN_OUT_DIR    输出目录（默认在 ${CMAKE_BINARY_DIR}/metagen）
# KEY5             META_GENERATOR      指定元数据生成器可执行文件（可选）
# KEY6             COMPILE_COMMANDS    compile_commands.json 的路径（必选）
# KEY7             RESULT_OUTPUT       存放生成器输出信息的变量（可选）
# KEY8             GENERATED_FILES_OUTPUT 存放生成的元数据文件列表（可选）
function(add_meta)
    # 定义单值参数和多值参数
    set(_one_value_arguments
        TARGET_NAME
        SOURCE_FILE
        META_GEN_OUT_DIR
        META_GENERATOR
        COMPILE_COMMANDS
        RESULT_OUTPUT
        GENERATED_FILES_OUTPUT
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

    if(NOT ARGS_TARGET_NAME)
        message(FATAL_ERROR "TARGET_NAME argument is required!")
    endif()
    set(target_name ${ARGS_TARGET_NAME})

    if(NOT ARGS_METAS)
        message(FATAL_ERROR "No meta files specified. Please provide input files via METAS argument!")
    endif()

    if(NOT ARGS_SOURCE_FILE)
        message(FATAL_ERROR "SOURCE_FILE argument is required!")
    endif()

    # 如果没有指定生成器，则自动查找 lux_meta_generator
    if(ARGS_META_GENERATOR)
        set(META_GENERATOR ${ARGS_META_GENERATOR})
    else()
        find_program(META_GENERATOR lux_meta_generator)
    endif()
    if(NOT META_GENERATOR)
        message(FATAL_ERROR "Meta generator (lux_meta_generator) not found!")
    endif()

    # 设置输出目录，默认在 ${CMAKE_BINARY_DIR}/metagen
    if(NOT ARGS_META_GEN_OUT_DIR)
        set(ARGS_META_GEN_OUT_DIR ${CMAKE_BINARY_DIR}/metagen)
    endif()
    if(NOT EXISTS ${ARGS_META_GEN_OUT_DIR})
        file(MAKE_DIRECTORY ${ARGS_META_GEN_OUT_DIR})
    endif()

    set(GENERATED_META_FILES "")
    foreach(meta_file IN LISTS ARGS_METAS)
        # 以输入文件名（不含扩展名）作为输出文件名，加上 .meta.hpp 后缀
        get_filename_component(meta_filename ${meta_file} NAME_WE)
        set(static_output_file  ${ARGS_META_GEN_OUT_DIR}/${meta_filename}.meta.static.hpp)
        set(dynamic_output_file ${ARGS_META_GEN_OUT_DIR}/${meta_filename}.meta.dynamic.hpp)
        list(APPEND GENERATED_META_FILES ${output_file})
        list(APPEND GENERATED_META_FILES ${dynamic_output_file})

        add_custom_command(
            OUTPUT ${GENERATED_META_FILES}
            COMMAND ${META_GENERATOR} ${meta_file} ${ARGS_META_GEN_OUT_DIR} ${ARGS_SOURCE_FILE} ${ARGS_COMPILE_COMMANDS}
            DEPENDS ${meta_file}
            COMMENT "Generating meta file for ${meta_file}"
            VERBATIM
        )
    endforeach()

    add_custom_target(${target_name} ALL
        DEPENDS ${GENERATED_META_FILES}
    )

    if(ARGS_RESULT_OUTPUT)
        set(${ARGS_RESULT_OUTPUT} "Custom target ${target_name} created" PARENT_SCOPE)
    endif()

    if(ARGS_GENERATED_FILES_OUTPUT)
        set(${ARGS_GENERATED_FILES_OUTPUT} "${GENERATED_META_FILES}" PARENT_SCOPE)
    endif()
endfunction()
