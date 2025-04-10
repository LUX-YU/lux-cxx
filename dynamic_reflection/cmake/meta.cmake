# ===========================================
# Include guard
# ===========================================
if(_COMPONENT_META_TOOLS_INCLUDED_)
  return()
endif()
set(_COMPONENT_META_TOOLS_INCLUDED_ TRUE)

#
# add_meta(
#     NAME       <MetaObjectName>
#     [TARGET_FILES  file1.hpp file2.hpp ...]
#     [META_GEN_OUT_DIR          <path>]
#     [COMPILE_COMMANDS          <path>]
#     [META_GENERATOR            <path>]
#     [REGISTER_FUNCTION_HEADER_NAME  <file>]
#     [REGISTER_FUNCTION_NAME_PREFIX  <prefix>]
#     [ECHO]
# )
#
# 说明：
#   - 将原先的 METAS 改名为 TARGET_FILES，以匹配你所说的语义更清晰。
#   - 其他逻辑保持一致。
#
function(add_meta)
    set(one_value_args
        NAME
        META_GEN_OUT_DIR
        COMPILE_COMMANDS
        META_GENERATOR
        REGISTER_FUNCTION_HEADER_NAME
        REGISTER_FUNCTION_NAME
    )
    # 注意：这里改成了 TARGET_FILES
    set(multi_value_args
        TARGET_FILES
    )
    set(optional_args ECHO)

    cmake_parse_arguments(
        ARGS
        "${optional_args}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    if(NOT ARGS_NAME)
        message(FATAL_ERROR "[add_meta] 必须提供 NAME 参数")
    endif()

    set(_meta_name "${ARGS_NAME}")

    if(TARGET ${_meta_name})
        message(WARNING "[add_meta] Target '${_meta_name}' 已存在，示例中将直接覆盖其属性。")
    else()
        add_custom_target(${_meta_name} ALL
            COMMENT "[add_meta] Placeholder target for meta object '${_meta_name}'."
        )
    endif()

    # 设置默认值
    if(NOT ARGS_META_GEN_OUT_DIR)
        set(ARGS_META_GEN_OUT_DIR "${CMAKE_BINARY_DIR}/metagen")
    endif()
    if(NOT ARGS_COMPILE_COMMANDS)
        set(ARGS_COMPILE_COMMANDS "${CMAKE_BINARY_DIR}/compile_commands.json")
    endif()
    if(NOT ARGS_REGISTER_FUNCTION_HEADER_NAME)
        set(ARGS_REGISTER_FUNCTION_HEADER_NAME "register_all_dynamic_meta.hpp")
    endif()
    if(NOT ARGS_REGISTER_FUNCTION_NAME)
        set(ARGS_REGISTER_FUNCTION_NAME "register_reflections")
    endif()

    # 把配置信息存到目标属性
    set_target_properties(${_meta_name} PROPERTIES
        META_GEN_OUT_DIR                   "${ARGS_META_GEN_OUT_DIR}"
        META_COMPILE_COMMANDS              "${ARGS_COMPILE_COMMANDS}"
        META_GENERATOR                     "${ARGS_META_GENERATOR}" 
        META_REGISTER_FUNCTION_HEADER_NAME "${ARGS_REGISTER_FUNCTION_HEADER_NAME}"
        META_REGISTER_FUNCTION_NAME_PREFIX "${ARGS_REGISTER_FUNCTION_NAME}"
        META_ECHO                          "${ARGS_ECHO}"
    )

    # 存储文件列表到 TARGET_FILES 属性，使用 ';' 分隔
    if(ARGS_TARGET_FILES)
        list(REMOVE_DUPLICATES ARGS_TARGET_FILES)
        set_target_properties(${_meta_name} PROPERTIES
            TARGET_FILES "${ARGS_TARGET_FILES}"
        )
    else()
        set_target_properties(${_meta_name} PROPERTIES
            TARGET_FILES ""
        )
    endif()
endfunction()


#
# meta_add_files(<MetaObjectName> TARGET_FILES file1.hpp file2.hpp ...)
#
# 将更多文件追加到 <MetaObjectName> 的 TARGET_FILES 属性。
# （原先名称是 meta_add_files / METAS，现在改为 TARGET_FILES）
#

function(meta_add_files)
    if("${ARGV0}" STREQUAL "")
        message(FATAL_ERROR "[meta_add_files] 第一个参数必须是 MetaObjectName")
    endif()
    set(_meta_name "${ARGV0}")

    # 解析 TARGET_FILES
    list(REMOVE_AT ARGV 0)
    set(one_value_args "")
    set(multi_value_args TARGET_FILES)
    cmake_parse_arguments(ARGS "" "${one_value_args}" "${multi_value_args}" ${ARGV})

    if(NOT ARGS_TARGET_FILES)
        message(FATAL_ERROR "[meta_add_files] 需要至少一个 TARGET_FILES 参数")
    endif()

    get_target_property(_existing_files ${_meta_name} TARGET_FILES)
    if(NOT _existing_files)
        set(_existing_files "")
    endif()

    list(APPEND _existing_files ${ARGS_TARGET_FILES})
    list(REMOVE_DUPLICATES _existing_files)

    set_target_properties(${_meta_name} PROPERTIES
        TARGET_FILES "${_existing_files}"
    )
endfunction()



#
# target_add_meta(
#   NAME    <MetaObjectName>
#   TARGET  <YourTarget>
#   [ALWAYS_REGENERATE]
#   [ECHO]
# )
#
# 逻辑：一次性将 <MetaObjectName> 的全部 TARGET_FILES 写到 JSON 的 "target_files" 数组中，
#       然后调用 meta_generator <config.json> 产生 2*N 个输出文件，最后把它们加到 <YourTarget> 中。
#
function(target_add_meta)
    set(one_value_args
        NAME
        TARGET
    )
    set(multi_value_args "")
    set(optional_args
        ALWAYS_REGENERATE
        ECHO
    )

    cmake_parse_arguments(
        ARGS
        "${optional_args}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    if(NOT ARGS_NAME)
        message(FATAL_ERROR "[target_add_meta] No NAME=<MetaObjectName>")
    endif()
    if(NOT ARGS_TARGET)
        message(FATAL_ERROR "[target_add_meta] No TARGET=<YourTarget>")
    endif()

    set(_meta_name   "${ARGS_NAME}")
    set(_target_name "${ARGS_TARGET}")

    # 从 <MetaObjectName> 目标读属性
    get_target_property(_meta_out_dir  ${_meta_name} META_GEN_OUT_DIR)
    get_target_property(_meta_cc_json  ${_meta_name} META_COMPILE_COMMANDS)
    get_target_property(_meta_gen_exe  ${_meta_name} META_GENERATOR)
    get_target_property(_meta_header   ${_meta_name} META_REGISTER_FUNCTION_HEADER_NAME)
    get_target_property(register_function_name   ${_meta_name} META_REGISTER_FUNCTION_NAME_PREFIX)
    get_target_property(_meta_echo     ${_meta_name} META_ECHO)
    # 这里是最关键的：读 TARGET_FILES
    get_target_property(_meta_files    ${_meta_name} TARGET_FILES)

    # 做一些默认处理
    if(NOT _meta_out_dir)
        set(_meta_out_dir "${CMAKE_BINARY_DIR}/metagen")
    endif()
    if(NOT _meta_cc_json)
        set(_meta_cc_json "${CMAKE_BINARY_DIR}/compile_commands.json")
    endif()
    if(NOT _meta_header)
        set(_meta_header "register_all_dynamic_meta.hpp")
    endif()
    if(NOT register_function_name)
        set(register_function_name "register_metas")
    endif()

    if(NOT _meta_gen_exe)
        find_program(LUX_META_GENERATOR lux_meta_generator)
        if(LUX_META_GENERATOR)
            set(_meta_gen_exe ${LUX_META_GENERATOR})
        endif()
    endif()
    if(NOT _meta_gen_exe)
        message(FATAL_ERROR "[target_add_meta] Can't find lux_meta_generator")
    endif()

    if(NOT EXISTS "${_meta_out_dir}")
        file(MAKE_DIRECTORY "${_meta_out_dir}")
    endif()

    # Retrieve the target's source files
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


    # 解析 ; 分割的字符串
    if(NOT _meta_files)
        set(_meta_files "")
        message(FATAL_ERROR "[target_add_meta] '${_meta_name}' 的 TARGET_FILES 为空。")
    endif()

    #
    # 生成 JSON 配置文件
    #
    set(_config_file "${_meta_out_dir}/${_meta_name}_meta_config.json")
    file(WRITE "${_config_file}" "{\n")
    file(APPEND "${_config_file}" "  \"out_dir\": \"${_meta_out_dir}\",\n")
    file(APPEND "${_config_file}" "  \"compile_commands\": \"${_meta_cc_json}\",\n")
    file(APPEND "${_config_file}" "  \"source_file\": \"${_main_src}\",\n")

    # 正确写出 target_files 数组，一行一个条目
    file(APPEND "${_config_file}" "  \"target_files\": [\n")
    list(LENGTH _meta_files _mf_len)
    math(EXPR _last_idx "${_mf_len} - 1")
    foreach(i RANGE 0 ${_last_idx})
        list(GET _meta_files ${i} mf)
        if(i LESS _last_idx)
            file(APPEND "${_config_file}" "    \"${mf}\",\n")
        else()
            file(APPEND "${_config_file}" "    \"${mf}\"\n")
        endif()
    endforeach()
    file(APPEND "${_config_file}" "  ],\n")

    # 注册信息
    file(APPEND "${_config_file}" "  \"register_header_name\": \"${_meta_header}\",\n")
    file(APPEND "${_config_file}" "  \"register_function_name\": \"${register_function_name}\"\n")
    file(APPEND "${_config_file}" "}\n")

    # 计算所有输出文件：对每个输入 .hpp -> 2 个 .hpp
    set(_all_generated_files "")
    set(_generated_source_files "")
    foreach(_mf IN LISTS _meta_files)
        get_filename_component(_base "${_mf}" NAME_WE)
        set(_dyn_file "${_meta_out_dir}/${_base}.meta.dynamic.cpp")
        set(_static_file  "${_meta_out_dir}/${_base}.meta.static.hpp")
        list(APPEND _all_generated_files "${_dyn_file}" "${_static_file}")
        list(APPEND _generated_source_files "${_dyn_file}")
    endforeach()

    if(${_meta_echo} OR ${ARGS_ECHO})
        message(STATUS "[target_add_meta] META_GEN_OUT_DIR=${_meta_out_dir}")
        message(STATUS "                 COMPILE_COMMANDS=${_meta_cc_json}")
        message(STATUS "                 META_GENERATOR=${_meta_gen_exe}")
        message(STATUS "                 REGISTER_FUNCTION_HEADER_NAME=${_meta_header}")
        message(STATUS "                 REGISTER_FUNCTION_NAME_PREFIX=${register_function_name}")
        message(STATUS "                 FILES_WAITING_FOR_GENERATING=")
        foreach(generate_meta_test IN LISTS _all_generated_files)
            message(STATUS "                 -> ${generate_meta_test}")
        endforeach()
    endif()

    # 只调用一次 generator
    add_custom_command(
        OUTPUT ${_all_generated_files}
        COMMAND "${_meta_gen_exe}" "${_config_file}"
        DEPENDS ${_meta_files_list}    # 当任一输入文件变化时，重新生成
        COMMENT "[target_add_meta] Single run -> generating meta for '${_meta_name}'"
        VERBATIM
    )

    if(ARGS_ALWAYS_REGENERATE)
        foreach(_gf IN LISTS _all_generated_files)
            set_property(SOURCE "${_gf}" PROPERTY SKIP_CACHE TRUE)
        endforeach()
    endif()

    # 自定义 target
    set(_meta_gen_target "${_meta_name}_gen")
    add_custom_target("${_meta_gen_target}"
        DEPENDS ${_all_generated_files}
    )

    # 加入编译
    target_sources("${_target_name}"
        PRIVATE
        ${_generated_source_files}
    )
    add_dependencies("${_target_name}" "${_meta_gen_target}")

    target_include_directories("${_target_name}"
        PRIVATE
        "${_meta_out_dir}"
    )

    message(STATUS "[target_add_meta] Done: '${_target_name}' will build meta from '${_meta_name}'.")
endfunction()
