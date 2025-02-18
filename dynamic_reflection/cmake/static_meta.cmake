# INCLUDE GUARD
if(_COMPONENT_META_TOOLS_INCLUDED_)
  return()
endif()
set(_COMPONENT_META_TOOLS_INCLUDED_ TRUE)

# KEY1             COMPONENTS      <组件列表>
# KEY2             METAS           <需要生成元数据的文件列表>
# KEY3             IMPORT_DIRS     <依赖的 include 路径>
# KEY4             META_GENERATOR  指定元数据生成器可执行文件（可选）
# KEY5             META_GEN_OUT_DIR 生成输出目录（默认在 ${CMAKE_BINARY_DIR}/metagen）
# KEY6             RESULT_OUTPUT   存放生成器输出信息的变量
# KEY7             GENERATED_FILES_OUTPUT 存放生成的元数据文件列表
function(generate_static_meta)
    # 定义单值参数和多值参数
    set(_one_value_arguments
        META_GEN_OUT_DIR
        META_GENERATOR
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

    if(NOT ARGS_METAS)
        message(FATAL_ERROR "No meta files specified. Please provide input files via METAS argument!")
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
        set(output_file ${ARGS_META_GEN_OUT_DIR}/${meta_filename}.meta.hpp)

        execute_process(
            COMMAND ${META_GENERATOR} ${meta_file} ${output_file}
            RESULT_VARIABLE result
            OUTPUT_VARIABLE gen_output
            ERROR_VARIABLE gen_error
            COMMAND_ECHO STDOUT
        )
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "Meta generation failed for ${meta_file}: ${gen_error}")
        endif()
        list(APPEND GENERATED_META_FILES ${output_file})
    endforeach()

    if(ARGS_RESULT_OUTPUT)
        set(${ARGS_RESULT_OUTPUT} "${gen_output}" PARENT_SCOPE)
    endif()

    if(ARGS_GENERATED_FILES_OUTPUT)
        set(${ARGS_GENERATED_FILES_OUTPUT} "${GENERATED_META_FILES}" PARENT_SCOPE)
    endif()
endfunction()