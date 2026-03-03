# Code Generation

The code generation pipeline transforms parsed reflection metadata (JSON) into C++ source files using the **inja** template engine. This document covers the `lux_meta_generator` CLI, template callbacks, the built-in template, and how to write custom templates.

## Pipeline Overview

```
  Header files (.hpp)
        │
        ▼
  ┌─────────────┐     ┌──────────────┐     ┌──────────────────┐
  │  CxxParser   │────▶│   MetaUnit   │────▶│  JSON (.json)    │
  │  (libclang)  │     │  (in-memory) │     │  (optional dump) │
  └─────────────┘     └──────────────┘     └──────────────────┘
                                                    │
                                                    ▼
                                           ┌──────────────────┐
                                           │  inja Template   │
                                           │  (.template.inja)│
                                           └──────────────────┘
                                                    │
                                                    ▼
                                           ┌──────────────────┐
                                           │  Generated Code  │
                                           │  (.meta.cpp/hpp) │
                                           └──────────────────┘
```

## lux_meta_generator CLI

The generator executable reads a JSON configuration file and produces one output file per input header:

```bash
lux_meta_generator <config.json>
```

### Configuration File Format

```json
{
    "marker": "reflect",
    "template_path": "/path/to/template.inja",
    "out_dir": "/path/to/output",
    "compile_commands": "/path/to/compile_commands.json",
    "source_file": "/path/to/any_source.cpp",
    "target_files": [
        "/path/to/header1.hpp",
        "/path/to/header2.hpp"
    ],
    "meta_suffix": ".meta.cpp",
    "extra_compile_options": ["-std=c++20", "-DSOME_DEFINE"],
    "serial_meta": true,
    "dry_run": false,
    "cxx_standard": "c++20",
    "preprocessor_defines": ["MY_DEFINE=1"],
    "custom_fields_json": [
        "{\"my_key\": \"my_value\"}"
    ]
}
```

| Field | Required | Description |
|-------|----------|-------------|
| `marker` | Yes | Annotation marker matching `LUX_META(<marker>)` |
| `template_path` | Yes | Path to the inja template file |
| `out_dir` | Yes | Directory for generated output files |
| `compile_commands` | Yes | Path to `compile_commands.json` for include resolution |
| `source_file` | Yes | Any `.cpp` in compile_commands; used to extract `-I` paths |
| `target_files` | Yes | List of header files to parse |
| `meta_suffix` | No | Output suffix (default: `.meta.cpp`) |
| `extra_compile_options` | No | Additional Clang flags |
| `serial_meta` | No | Dump JSON to `<stem>.json` alongside output (default: `true`) |
| `dry_run` | No | Parse only, no output generation (default: `false`) |
| `cxx_standard` | No | C++ standard for parsing (default: `c++20`) |
| `preprocessor_defines` | No | Additional preprocessor defines (added as `-D<value>`) |
| `custom_fields_json` | No | Extra JSON key-value pairs merged into the template data |

### Processing Steps

1. **Validate** — Check all input files exist
2. **Build compile options** — Extract `-I` paths from `compile_commands.json`, add standard & defines
3. **Parse** — For each target file, run `CxxParser::parse()` → `MetaUnit` → JSON
4. **(Optional) Serialize** — If `serial_meta` is `true`, write `<stem>.json` per file
5. **Render** — For each JSON, render the inja template → write `<stem><meta_suffix>`

## Template Callbacks

The generator registers four callbacks available inside inja templates:

### `decl_from_id(id)`

Look up a declaration by its unique `id` string and return the full JSON object.

```
{% set decl = decl_from_id(some_id) %}
{{ decl.name }}  // e.g., "MyStruct"
{{ decl.fq_name }}  // e.g., "my_ns::MyStruct"
```

### `decl_from_index(index)`

Look up a declaration by its integer array index in the `declarations` array.

```
{% set decl = decl_from_index(0) %}
{{ decl.__kind }}  // e.g., "CXXRecordDecl"
```

### `type_from_id(type_id)`

Look up a type by its unique `type_id` string and return the full JSON object.

```
{% set type = type_from_id(decl.type_id) %}
{{ type.size }}   // e.g., 24
{{ type.align }}  // e.g., 8
```

### `parent_chain(decl_id, target_name)`

For a `CXXRecordDecl`, find the inheritance chain from the declaration to a named ancestor. Returns an array of `{id, hash}` objects along the path.

```
{% set chain = parent_chain(derived_id, "BaseClass") %}
{% for node in chain %}
  {{ node.id }} (hash: {{ node.hash }})
{% endfor %}
```

This is useful for generating virtual dispatch tables, RTTI chains, or component hierarchies.

## Template Data Schema

Each template receives a JSON object with the following top-level fields:

```json
{
    "name": "header_name",
    "version": "1.0.0",
    "types": [ ... ],
    "declarations": [ ... ],
    "marked_record_decls": [0, 2, 5],
    "marked_enum_decls": [1, 3],
    "marked_function_decls": [4],
    "source_path": "/abs/path/to/header.hpp",
    "source_parent": "/abs/path/to",
    "include_dir": "relative/path/to/header.hpp",
    "parser_compile_options": ["-std=c++20", "-I..."]
}
```

| Field | Description |
|-------|-------------|
| `types` | Array of all types referenced by declarations |
| `declarations` | Array of all parsed declarations |
| `marked_record_decls` | Indices into `declarations` for marked structs/classes |
| `marked_enum_decls` | Indices for marked enums |
| `marked_function_decls` | Indices for marked free functions |
| `source_path` | Absolute path of the parsed header |
| `source_parent` | Parent directory of the header |
| `include_dir` | Relative include path (from one of the `-I` search dirs) |
| `parser_compile_options` | Compile options used during parsing |

Custom fields from `custom_fields_json` are merged into this object.

## Built-in Template

The repository includes a reference template at `templates/static_meta.template.inja`:

```cpp
#pragma once
#include <array>
#include <cstddef>
#include <tuple>
#include <string_view>
#include <type_traits>

namespace lux::cxx::dref{
    template<typename T> class type_meta;

    {% for decl_index in marked_record_decls %}
    {% set decl=decl_from_index(decl_index) %}
    {% set type=type_from_id(decl.type_id) %}
    {% if decl.__kind=="CXXRecordDecl" %}
    template<>
    class type_meta<{{ decl.fq_name }}>
    {
    public:
        using type = {{ decl.fq_name }};
        static constexpr size_t size = {{ type.size }};
        static constexpr size_t align = {{ type.align }};
        static constexpr std::string_view full_qualified_name = "{{ decl.fq_name }}";
        static constexpr std::string_view name = "{{ decl.name }}";
        static constexpr bool is_abstract = {{ decl.is_abstract }};

        using field_types = std::tuple<
            {% for field_id in decl.field_decls %}
            {% set field_decl=decl_from_id(field_id) %}
            {{ field_decl.type_id }} {% if not loop.is_last %},{% endif %}
            {% endfor %}
        >;
    };
    {% endif %}
    {% endfor %}
}
```

This generates a compile-time `type_meta<T>` specialization for each reflected struct/class, exposing name, size, alignment, and field types as a `std::tuple`.

## Writing Custom Templates

### inja Syntax Quick Reference

| Syntax | Description |
|--------|-------------|
| `{{ expr }}` | Output the value of an expression |
| `{% for x in list %}...{% endfor %}` | Loop over an array |
| `{% if cond %}...{% elif %}...{% else %}...{% endif %}` | Conditional |
| `{% set var = expr %}` | Assign a variable |
| `{{ callback(arg1, arg2) }}` | Call a registered callback |
| `{{ loop.index }}` | Current loop index (0-based) |
| `{{ loop.is_last }}` | True on the last iteration |

### Example: Property Inspector

A template that generates runtime property metadata for a game engine editor:

```
// Auto-generated property metadata
#pragma once
#include "property_system.h"

{% for decl_index in marked_record_decls %}
{% set decl = decl_from_index(decl_index) %}
{% if decl.__kind == "CXXRecordDecl" %}
template<>
void register_properties<{{ decl.fq_name }}>() {
    {% for field_id in decl.field_decls %}
    {% set field = decl_from_id(field_id) %}
    {% set ftype = type_from_id(field.type_id) %}
    PropertyRegistry::add<{{ decl.fq_name }}>(
        "{{ field.name }}",
        offsetof({{ decl.fq_name }}, {{ field.name }}),
        sizeof({{ ftype.spelling }}),
        "{{ ftype.spelling }}"
    );
    {% endfor %}
}
{% endif %}
{% endfor %}
```

### Example: Serialization Bindings

```
{% for decl_index in marked_record_decls %}
{% set decl = decl_from_index(decl_index) %}
{% if decl.__kind == "CXXRecordDecl" %}
void serialize(Archive& ar, {{ decl.fq_name }}& obj) {
    {% for field_id in decl.field_decls %}
    {% set field = decl_from_id(field_id) %}
    ar.field("{{ field.name }}", obj.{{ field.name }});
    {% endfor %}
}
{% endif %}
{% endfor %}
```

### Accessing Template Arguments

For fields with STL container types, you can inspect the template arguments in the type data:

```
{% set ftype = type_from_id(field.type_id) %}
{% if ftype.template_name is defined %}
  // This is a template: {{ ftype.template_name }}
  {% for targ in ftype.template_arguments %}
    // arg {{ loop.index }}: kind={{ targ.kind }}, spelling={{ targ.spelling }}
  {% endfor %}
{% endif %}
```
