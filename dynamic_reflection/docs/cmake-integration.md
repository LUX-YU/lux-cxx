# CMake Integration

The `dynamic_reflection` module provides CMake functions that automate the parse → generate → compile pipeline. This document covers the `add_meta()`, `meta_add_files()`, and `target_add_meta()` functions defined in `cmake/meta.cmake`.

## Quick Start

```cmake
include(${CMAKE_SOURCE_DIR}/dynamic_reflection/cmake/meta.cmake)

# 1. Define a meta configuration object
add_meta(
    NAME        my_meta
    MARKER      reflect
    TEMPLATE    ${CMAKE_SOURCE_DIR}/templates/my_template.inja
    OUT_DIR     ${CMAKE_BINARY_DIR}/metagen
    TARGET_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/include/my_component.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/my_system.hpp
)

# 2. Attach to your library/executable target
target_add_meta(
    NAME   my_meta
    TARGET my_library
)
```

When `my_component.hpp` or `my_system.hpp` changes, CMake re-runs `lux_meta_generator` to regenerate the output files, which are automatically added to `my_library`'s sources.

## `add_meta()`

Defines a meta configuration object that stores all generator settings.

### Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `NAME` | String | **Yes** | — | Unique name for this meta configuration |
| `GENERATOR` | Path | No | Auto-find `lux_meta_generator` | Path to a custom generator executable |
| `MARKER` | String | **Yes** | — | Annotation marker (e.g., `"reflect"`) |
| `TEMPLATE` | Path | **Yes** | — | Path to the inja template file |
| `OUT_DIR` | Path | No | `${CMAKE_BINARY_DIR}/metagen` | Directory for generated files |
| `COMPILE_COMMANDS` | Path | No | `${CMAKE_BINARY_DIR}/compile_commands.json` | Path to compile_commands file |
| `META_SUFFIX` | String | No | `.meta.cpp` | Suffix for generated files |
| `SOURCE_FILE` | Path | No | `""` | Source file used to extract include paths from compile_commands |
| `SERIAL_META` | ON/OFF | No | `ON` | Write intermediate JSON files |
| `DRY_RUN` | ON/OFF | No | `OFF` | Parse without generating output |
| `TARGET_FILES` | List | No | — | Header files to parse |
| `EXTRA_COMPILE_OPTIONS` | List | No | — | Additional Clang flags |
| `JSON_FIELD` | List | No | — | Custom JSON fields merged into template data |

### Flags

| Flag | Description |
|------|-------------|
| `ECHO` | Print verbose configuration info |
| `ALWAYS_REGENERATE` | Force regeneration on every build |

### Example

```cmake
add_meta(
    NAME            game_meta
    MARKER          reflect
    TEMPLATE        ${CMAKE_SOURCE_DIR}/templates/static_meta.template.inja
    OUT_DIR         ${CMAKE_BINARY_DIR}/generated/meta
    META_SUFFIX     .meta.hpp
    SERIAL_META     ON
    EXTRA_COMPILE_OPTIONS
        -std=c++20
        -DGAME_ENGINE=1
    JSON_FIELD
        "{\"engine_name\": \"MyEngine\"}"
    ECHO
)
```

## `meta_add_files()`

Appends additional header files to an existing meta configuration.

```cmake
meta_add_files(game_meta
    ${CMAKE_CURRENT_SOURCE_DIR}/include/physics_component.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/render_component.hpp
)
```

This is useful when header files are defined across multiple subdirectories:

```cmake
# In physics/CMakeLists.txt
meta_add_files(game_meta
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rigid_body.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/collider.hpp
)

# In rendering/CMakeLists.txt
meta_add_files(game_meta
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mesh_renderer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/material.hpp
)
```

Duplicate entries are automatically removed.

## `target_add_meta()`

Connects a meta configuration to a CMake build target. This function:

1. Reads the stored configuration from the `add_meta()` object
2. Generates a JSON config file for the generator
3. Creates a custom command that runs `lux_meta_generator` when inputs change
4. Adds generated `.cpp` files to the target's sources
5. Adds the output directory to the target's include paths

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `NAME` | String | **Yes** | Name of the meta config object (from `add_meta()`) |
| `TARGET` | String | **Yes** | CMake target to attach generated files to |

### Flags

| Flag | Description |
|------|-------------|
| `ALWAYS_REGENERATE` | Force regeneration every build |
| `ECHO` | Print verbose info |
| `DONT_ADD_TO_SOURCE` | Don't add generated `.cpp` files to target sources |
| `DONT_INCLUDE` | Don't add output directory to include paths |

### Example

```cmake
add_library(game_engine
    src/main.cpp
    src/engine.cpp
)

target_add_meta(
    NAME   game_meta
    TARGET game_engine
    ECHO
)
```

## Build Flow

```
  CMake Configure
       │
       ▼
  add_meta(NAME game_meta ...)        ← stores config on a CMake target
       │
       ▼
  meta_add_files(game_meta ...)       ← (optional) add more headers
       │
       ▼
  target_add_meta(NAME game_meta      ← generates JSON config
                  TARGET my_lib)        writes custom command
       │
       ▼
  Build
       │
       ▼
  custom_command triggers:
    lux_meta_generator config.json
       │
       ├── Parse headers → MetaUnit → JSON
       ├── (Optional) Write .json files
       └── Render template → .meta.cpp/.meta.hpp
       │
       ▼
  Generated files compiled into my_lib
```

### Source File Resolution

`target_add_meta` automatically finds a `.cpp`/`.cxx`/`.cc` file from the target's `SOURCES` to use for extracting include paths from `compile_commands.json`. If the target has no source files, set `SOURCE_FILE` in `add_meta()` to provide one explicitly.

### Dependencies

The generated custom command depends on the `TARGET_FILES` list. When any header file changes, the generator re-runs. Use `ALWAYS_REGENERATE` if you also want regeneration when the template changes.

## Complete Example

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_game)

# Enable compile_commands.json generation
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include the meta tools
include(${CMAKE_SOURCE_DIR}/dynamic_reflection/cmake/meta.cmake)

# Define the reflection configuration
add_meta(
    NAME        game_meta
    MARKER      reflect
    TEMPLATE    ${CMAKE_SOURCE_DIR}/templates/component_registry.inja
    OUT_DIR     ${CMAKE_BINARY_DIR}/generated
    META_SUFFIX .meta.cpp
    SERIAL_META ON
    TARGET_FILES
        ${CMAKE_SOURCE_DIR}/include/components/transform.hpp
        ${CMAKE_SOURCE_DIR}/include/components/physics.hpp
        ${CMAKE_SOURCE_DIR}/include/components/renderer.hpp
    EXTRA_COMPILE_OPTIONS
        -std=c++20
)

# Define the game library
add_library(game_engine
    src/engine.cpp
    src/world.cpp
)

target_include_directories(game_engine PUBLIC include)

# Attach meta generation — generated .meta.cpp files are
# automatically added to game_engine's sources
target_add_meta(
    NAME   game_meta
    TARGET game_engine
)
```

## Generated Config File

`target_add_meta` writes a JSON config at `<OUT_DIR>/<NAME>_meta_config.json`:

```json
{
    "marker": "reflect",
    "template_path": "/abs/path/to/template.inja",
    "out_dir": "/abs/path/to/generated",
    "compile_commands": "/abs/path/to/compile_commands.json",
    "source_file": "/abs/path/to/engine.cpp",
    "target_files": [
        "/abs/path/to/transform.hpp",
        "/abs/path/to/physics.hpp"
    ],
    "meta_suffix": ".meta.cpp",
    "extra_compile_options": ["-std=c++20"],
    "serial_meta": true,
    "dry_run": false
}
```

This file is passed as the sole argument to `lux_meta_generator`.
