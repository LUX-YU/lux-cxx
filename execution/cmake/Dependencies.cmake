include_guard(GLOBAL)
find_package(Threads REQUIRED)
option(LUX_EXECUTION_FETCH_DEPS "Explicitly allow fetching pinned header-only development dependencies" OFF)
set(LUX_EXECUTION_STDEXEC_REV "7d704575019a1405b2067c5e47265245c55ca66e" CACHE STRING "stdexec source revision for development fetch")
set(LUX_EXECUTION_CONCURRENTQUEUE_REV "6dd38b8a1dbaa7863aa907045f32308a56a6ff5d" CACHE STRING "concurrentqueue v1.0.4 source revision")

if(NOT TARGET STDEXEC::stdexec)
    find_package(stdexec CONFIG QUIET)
endif()
if(NOT TARGET concurrentqueue::concurrentqueue)
    find_package(concurrentqueue CONFIG QUIET)
endif()

if(LUX_EXECUTION_FETCH_DEPS)
    include(FetchContent)
    if(NOT TARGET STDEXEC::stdexec)
        # Populate only: upstream's full CMake project may fetch unrelated tools.
        # This target is a development header view, not an installed stdexec package.
        FetchContent_Declare(lux_exec_stdexec
            GIT_REPOSITORY https://github.com/NVIDIA/stdexec.git
            GIT_TAG ${LUX_EXECUTION_STDEXEC_REV}
            SOURCE_SUBDIR _lux_header_only_no_cmake_subdir)
        FetchContent_MakeAvailable(lux_exec_stdexec)
        add_library(STDEXEC::stdexec INTERFACE IMPORTED GLOBAL)
        set_target_properties(STDEXEC::stdexec PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${lux_exec_stdexec_SOURCE_DIR}/include"
            INTERFACE_INCLUDE_DIRECTORIES "${lux_exec_stdexec_SOURCE_DIR}/include"
            INTERFACE_COMPILE_OPTIONS "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/Zc:__cplusplus;/Zc:preprocessor;/Zc:externConstexpr>")
    endif()
    if(NOT TARGET concurrentqueue::concurrentqueue)
        FetchContent_Declare(lux_exec_concurrentqueue
            GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
            GIT_TAG ${LUX_EXECUTION_CONCURRENTQUEUE_REV}
            SOURCE_SUBDIR _lux_header_only_no_cmake_subdir)
        FetchContent_MakeAvailable(lux_exec_concurrentqueue)
        add_library(concurrentqueue::concurrentqueue INTERFACE IMPORTED GLOBAL)
        set_target_properties(concurrentqueue::concurrentqueue PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${lux_exec_concurrentqueue_SOURCE_DIR}"
            INTERFACE_INCLUDE_DIRECTORIES "${lux_exec_concurrentqueue_SOURCE_DIR}")
    endif()
endif()

if(NOT TARGET STDEXEC::stdexec OR NOT TARGET concurrentqueue::concurrentqueue)
    message(FATAL_ERROR "Install stdexec and concurrentqueue CMake packages, provide their targets, or explicitly set LUX_EXECUTION_FETCH_DEPS=ON. No implicit download is performed.")
endif()
