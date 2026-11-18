# Multi-projection CMake integration

The reflection generator separates parsing from rendering. A parse job owns
physical and logical input paths; projections own their templates, output
roots, suffixes, and JSON fields.

```cmake
lux_add_codegen_job(
    NAME example_codegen
    MARKER luxref
    TARGET_FILES ${headers}
    LOGICAL_PATHS ${logical_headers}
)

lux_codegen_add_projection(
    JOB example_codegen
    NAME runtime
    TEMPLATE ${runtime_template}
    OUTPUT_ROOT ${CMAKE_CURRENT_BINARY_DIR}/generated
    OUTPUT_SUFFIX .type_runtime_info.cpp
)

lux_codegen_add_projection(
    JOB example_codegen
    NAME static
    TEMPLATE ${static_template}
    OUTPUT_ROOT ${CMAKE_CURRENT_BINARY_DIR}/generated/include
    OUTPUT_SUFFIX .type_static_info.hpp
)

lux_target_add_codegen(
    TARGET example
    JOB example_codegen
)
```

Each target file is parsed once for all selected projections. The logical path
is preserved below the output root and the projection suffix replaces the
source extension. CMake enumerates every output during configure and rejects
collisions before generation.

Generation renders every projection to temporary files and publishes the whole
set only after all renders succeed. Publishing preserves timestamps for
content-identical files. Projection JSON fields are isolated; there are no
job-global custom fields.

The removed `add_meta`, `meta_add_files`, and `target_add_meta` APIs are
not compatibility aliases. Consumers must use the three functions above.
