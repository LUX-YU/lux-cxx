# stacktrace module.
#   C++23      : lux::cxx::stacktrace aliases std::stacktrace (header-only path);
#                the compiled component is empty and needs no dependency.
#   pre-C++23  : backed by cpptrace, hidden inside stacktrace.cpp (the public header
#                exposes no cpptrace type). cpptrace is provisioned find_package-first,
#                else fetched with -DLUX_FETCH_DEPS=ON.
# SHARED so cpptrace (PRIVATE-linked, multi-file) is baked INTO the stacktrace DLL
# and never reaches the consumer's link line — full hiding for a dep we can't
# single-.cpp-bundle like tinyxml2. WINDOWS_EXPORT_ALL_SYMBOLS exports our own API
# without per-symbol annotations (consumers still don't need cpptrace).
add_component(
    COMPONENT_NAME stacktrace
    NAMESPACE      lux::cxx
    SHARED
    SOURCE_FILES   ${CMAKE_CURRENT_SOURCE_DIR}/src/stacktrace.cpp
)
set_target_properties(stacktrace PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

component_include_directories(
    stacktrace
    BUILD_TIME_EXPORT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    INSTALL_TIME
        include
)

if(CMAKE_CXX_STANDARD LESS 23)
    find_package(cpptrace CONFIG QUIET)
    if(NOT TARGET cpptrace::cpptrace)
        if(LUX_FETCH_DEPS)
            set(LUX_CPPTRACE_VERSION "1.0.4" CACHE STRING "cpptrace version to fetch")
            message(STATUS "[stacktrace] fetching cpptrace ${LUX_CPPTRACE_VERSION}")
            include(FetchContent)
            FetchContent_Declare(cpptrace
                GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
                GIT_TAG        v${LUX_CPPTRACE_VERSION}
                GIT_SHALLOW    TRUE)
            FetchContent_MakeAvailable(cpptrace)
        else()
            message(FATAL_ERROR
                "[stacktrace] needs cpptrace on pre-C++23. Install it, configure with "
                "-DLUX_FETCH_DEPS=ON to fetch it, or build with C++23 (uses std::stacktrace).")
        endif()
    endif()
    # PRIVATE: cpptrace is an implementation detail of the backport. (Fully hiding it
    # from a STATIC archive would need bundling/object-merge — a follow-up; for now a
    # consumer links cpptrace transitively, obtained the same find-or-fetch way.)
    target_link_libraries(stacktrace PRIVATE cpptrace::cpptrace)
endif()

set(ENABLE_STACKTRACE_TEST TRUE CACHE BOOL "Enable stacktrace tests")
if(ENABLE_STACKTRACE_TEST)
    add_subdirectory(test)
endif()
