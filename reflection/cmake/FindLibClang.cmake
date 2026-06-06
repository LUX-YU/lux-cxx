#[=======================================================================[.rst:
FindLibClang
-----------

Find LibClang library and headers.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines the following :prop_tgt:`IMPORTED` targets:

``LibClang::LibClang``
  The LibClang library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``LibClang_FOUND``
  True if LibClang is found.
``LibClang_INCLUDE_DIRS``
  Include directories for LibClang headers.
``LibClang_LIBRARIES``
  Libraries to link for LibClang.
``LibClang_VERSION``
  The version of LibClang found.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may be set:

``LibClang_INCLUDE_DIR``
  The directory containing ``clang-c/Index.h``.
``LibClang_LIBRARY``
  The path to the LibClang library.

#]=======================================================================]

if(TARGET LibClang::LibClang)
    return()
endif()

# Try to find libclang using pkg-config first
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_LibClang QUIET libclang)
endif()

# Search paths for different platforms
if(UNIX AND NOT APPLE)
    # Linux: search common LLVM installation paths (prioritize newer versions)
    file(GLOB LLVM_SEARCH_PATHS_RAW
        "/usr/lib/llvm-20*"
        "/usr/lib/llvm-19*"
        "/usr/lib/llvm-18*"
        "/usr/lib/llvm-17*"
        "/usr/lib/llvm-16*"
        "/usr/lib/llvm-15*"
        "/usr/lib/llvm-14*"
        "/usr/lib/llvm-13*"
        "/usr/lib/llvm-*"
        "/usr/local/lib/llvm-*"
        "/opt/llvm-*"
    )
    
    # Expand search paths to include lib and include subdirectories
    set(LLVM_SEARCH_PATHS)
    foreach(path ${LLVM_SEARCH_PATHS_RAW})
        list(APPEND LLVM_SEARCH_PATHS "${path}")
        list(APPEND LLVM_SEARCH_PATHS "${path}/lib")
        list(APPEND LLVM_SEARCH_PATHS "${path}/include")
    endforeach()
    
    # Sort to prefer higher version numbers
    list(SORT LLVM_SEARCH_PATHS ORDER DESCENDING)
elseif(APPLE)
    # macOS: search Homebrew and system paths
    set(LLVM_SEARCH_PATHS
        "/opt/homebrew/lib"
        "/opt/homebrew/include"
        "/usr/local/lib"
        "/usr/local/include"
        "/usr/local/opt/llvm/lib"
        "/usr/local/opt/llvm/include"
        "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib"
        "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include"
    )
elseif(WIN32)
    # Windows: search LLVM installation paths
    set(LLVM_SEARCH_PATHS
        "$ENV{PROGRAMFILES}/LLVM/lib"
        "$ENV{PROGRAMFILES}/LLVM/include"
        "$ENV{PROGRAMFILES\(x86\)}/LLVM/lib"
        "$ENV{PROGRAMFILES\(x86\)}/LLVM/include"
        "C:/LLVM/lib"
        "C:/LLVM/include"
    )
endif()

# Find the include directory
find_path(LibClang_INCLUDE_DIR
    NAMES clang-c/Index.h
    HINTS 
        ${PC_LibClang_INCLUDE_DIRS}
        ${LLVM_SEARCH_PATHS}
    PATHS
        /usr/include
        /usr/local/include
        /opt/local/include
    PATH_SUFFIXES
        clang
        clang-c
    DOC "LibClang include directory"
)

# Find the library
find_library(LibClang_LIBRARY
    NAMES 
        clang
        libclang
        clang.dll
        libclang.dll
    HINTS 
        ${PC_LibClang_LIBRARY_DIRS}
        ${LLVM_SEARCH_PATHS}
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/local/lib
    PATH_SUFFIXES
        llvm
    DOC "LibClang library"
)

# Try to extract version information
if(LibClang_INCLUDE_DIR)
    # Look for version in CXString.h or Index.h
    foreach(version_file "clang-c/CXString.h" "clang-c/Index.h")
        if(EXISTS "${LibClang_INCLUDE_DIR}/${version_file}")
            file(READ "${LibClang_INCLUDE_DIR}/${version_file}" _libclang_version_header)
            # Try different version patterns
            if(_libclang_version_header MATCHES "#define CINDEX_VERSION_MAJOR ([0-9]+)")
                set(LibClang_VERSION_MAJOR "${CMAKE_MATCH_1}")
                if(_libclang_version_header MATCHES "#define CINDEX_VERSION_MINOR ([0-9]+)")
                    set(LibClang_VERSION_MINOR "${CMAKE_MATCH_1}")
                    set(LibClang_VERSION "${LibClang_VERSION_MAJOR}.${LibClang_VERSION_MINOR}")
                endif()
                break()
            elseif(_libclang_version_header MATCHES "#define CLANG_VERSION_STRING \"([0-9]+\\.[0-9]+)")
                set(LibClang_VERSION "${CMAKE_MATCH_1}")
                break()
            endif()
        endif()
    endforeach()
    
    # If version not found in headers, try to get it from the library path
    if(NOT LibClang_VERSION AND LibClang_LIBRARY)
        get_filename_component(lib_dir "${LibClang_LIBRARY}" DIRECTORY)
        get_filename_component(lib_parent "${lib_dir}" DIRECTORY)
        if(lib_parent MATCHES "llvm-([0-9]+)")
            set(LibClang_VERSION "${CMAKE_MATCH_1}")
        endif()
    endif()
endif()

# Handle the QUIETLY and REQUIRED arguments and set LibClang_FOUND
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibClang
    REQUIRED_VARS
        LibClang_LIBRARY
        LibClang_INCLUDE_DIR
    VERSION_VAR LibClang_VERSION
    HANDLE_COMPONENTS
)

if(LibClang_FOUND)
    set(LibClang_LIBRARIES ${LibClang_LIBRARY})
    set(LibClang_INCLUDE_DIRS ${LibClang_INCLUDE_DIR})

    # Print found information
    message(STATUS "Found LibClang:")
    message(STATUS "  Version: ${LibClang_VERSION}")
    message(STATUS "  Include directory: ${LibClang_INCLUDE_DIR}")
    message(STATUS "  Library: ${LibClang_LIBRARY}")

    # Create imported target
    if(NOT TARGET LibClang::LibClang)
        add_library(LibClang::LibClang UNKNOWN IMPORTED)
        set_target_properties(LibClang::LibClang PROPERTIES
            IMPORTED_LOCATION "${LibClang_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LibClang_INCLUDE_DIR}"
        )
        
        # Add any required system libraries
        if(UNIX AND NOT APPLE)
            set_property(TARGET LibClang::LibClang APPEND PROPERTY
                INTERFACE_LINK_LIBRARIES dl pthread
            )
        endif()
    endif()
endif()

# Mark variables as advanced
mark_as_advanced(
    LibClang_INCLUDE_DIR
    LibClang_LIBRARY
)
