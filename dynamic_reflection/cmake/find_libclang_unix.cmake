find_package(Clang QUIET)

if(Clang_FOUND)
    return()
endif()

if(NOT UNIX)
    message(FATAL_MESSAGE "Couldn't find libclang")
endif()

file(GLOB CLANG_SEARCH_PATH
    "/usr/lib/llvm-*" "/usr/lib/llvm-*/lib" "/usr/lib/llvm-*/include"
)

message("LIBCLANG SEARCH PATH : ${CLANG_SEARCH_PATH}")
find_path(
    LIBCLANG_INCLUDE_DIR 
    clang-c/Index.h 
    HINTS ${CLANG_SEARCH_PATH} REQUIRED
)
find_library(
    LIBCLANG_LIBRARY
    clang
    HINTS ${CLANG_SEARCH_PATH} REQUIRED
)

add_library(
    libclang
    INTERFACE
    IMPORTED
)

target_include_directories(
    libclang
    INTERFACE
    ${LIBCLANG_INCLUDE_DIR}
)

target_link_libraries(
    libclang
    INTERFACE
    ${LIBCLANG_LIBRARY}
)

message("---- Find libclang include directory:${LIBCLANG_INCLUDE_DIR}")
message("---- Find libclang library:${LIBCLANG_LIBRARY}")