# Dynamic Reflection

A C++ dynamic reflection system based on **libclang**, designed for game engines and other applications that need runtime type introspection, editor tooling, and code generation.

## Architecture

```
                          ┌──────────────────┐
                          │   C++ Headers    │
                          │  LUX_META(marked)│
                          └────────┬─────────┘
                                   │
                    ┌──────────────▼──────────────┐
                    │      CxxParser (libclang)    │
                    │  Parses AST, extracts types, │
                    │  declarations, annotations   │
                    └──────┬──────────────┬────────┘
                           │              │
                  ┌────────▼────┐  ┌──────▼───────┐
                  │  MetaUnit   │  │  JSON Export  │
                  │ (in-memory) │  │  (serialize)  │
                  └────┬────────┘  └──────┬────────┘
                       │                  │
            ┌──────────▼──────┐   ┌───────▼────────┐
            │ Code Generation │   │  Runtime Load   │
            │ (inja template) │   │ MetaUnit::from  │
            │ → .meta.hpp/cpp │   │ Json() → query  │
            └─────────────────┘   └────────────────┘
```

## Features

### Parser
- **Annotation-based marking**: Use `LUX_META(symbol)` to mark declarations for reflection
- **Member exclusion**: Use `LUX_META(no_reflect)` to exclude individual fields/methods
- **Template argument parsing**: Full support for STL template types (`vector<int>`, `array<float, 3>`, `map<K,V>`, `variant<Ts...>`, nested templates)
- **Comprehensive C++ support**: Classes, structs, unions, enums, functions, inheritance, virtual methods, operators, conversion operators, and more
- **MSVC compatibility**: Auto-injects `_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH` when building on MSVC

### Code Generation
- **inja template engine**: Flexible template-based code generation
- **CMake integration**: `add_meta()` + `target_add_meta()` for seamless build system integration
- **Incremental builds**: Only regenerates when source headers change
- **Customizable output**: Any file format via templates (`.meta.hpp`, `.meta.cpp`, `.json`, etc.)

### Runtime
- **Full JSON serialization**: Round-trip serialize/deserialize of all reflection metadata
- **Type queries**: Look up types and declarations by ID
- **Visitor pattern**: `DeclVisitor` and `TypeVisitor` for traversing reflection data

## Quick Start

### 1. Annotate your headers

```cpp
#pragma once
#include <lux/cxx/dref/runtime/Marker.hpp>
#include <vector>
#include <array>

struct LUX_META(marked) TransformComponent
{
    std::array<float, 3> position;
    std::array<float, 3> scale;
    float rotation;

    LUX_META(no_reflect) int _internal_cache; // excluded from reflection

    void reset() { position = {0,0,0}; scale = {1,1,1}; rotation = 0; }
};
```

### 2. Parse at build time

```cpp
#include <lux/cxx/dref/parser/CxxParser.hpp>
using namespace lux::cxx::dref;

ParseOptions options{
    .name           = "my_game",
    .version        = "1.0",
    .marker_symbol  = "marked",       // matches LUX_META(marked)
    .exclude_symbol = "no_reflect",   // matches LUX_META(no_reflect), this is the default
    .commands       = { "-std=c++20", "-I/path/to/includes" },
};

CxxParser parser(options);
auto [result, meta] = parser.parse("components/transform.hpp");

// Export to JSON
auto json = meta.toJson();
std::ofstream("transform.meta.json") << json.dump(4);
```

### 3. Use reflection data

```cpp
// Query component fields
for (auto* record : meta.markedRecordDecls()) {
    std::cout << "Component: " << record->full_qualified_name << "\n";
    for (auto* field : record->field_decls) {
        auto* rt = dynamic_cast<RecordType*>(field->type);
        if (rt && rt->isTemplateSpecialization()) {
            std::cout << "  " << field->spelling << ": "
                      << rt->template_name << "<...>\n";
        }
    }
}
```

### 4. Or generate code via CMake

```cmake
include(meta.cmake)

add_meta(
    NAME       my_meta
    GENERATOR  $<TARGET_FILE:lux_meta_generator>
    MARKER     "marked"
    TEMPLATE   "${CMAKE_CURRENT_SOURCE_DIR}/templates/my_template.inja"
    OUT_DIR    "${CMAKE_BINARY_DIR}/generated"
    META_SUFFIX ".meta.hpp"
    TARGET_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/components/transform.hpp"
)

target_add_meta(NAME my_meta TARGET my_game_engine)
```

## Documentation

| Document | Description |
|----------|-------------|
| [Parser Features](docs/parser.md) | Supported C++ features, annotations, member exclusion, template argument parsing |
| [Code Generation](docs/codegen.md) | inja template system, template callbacks, generation workflow |
| [CMake Integration](docs/cmake-integration.md) | `add_meta()`, `target_add_meta()`, build system setup |
| [JSON Schema Reference](docs/json-schema.md) | Complete reference for the JSON metadata format |

## Supported C++ Features

| Feature | Status | Notes |
|---------|--------|-------|
| Classes / Structs / Unions | ✅ | Tag kind detection |
| Inheritance (single & multiple) | ✅ | Base class chain resolution |
| Virtual / Abstract classes | ✅ | `is_abstract`, `is_virtual` |
| Fields with offsets | ✅ | Byte offset in layout |
| Methods (static, const, volatile) | ✅ | All qualifiers detected |
| Constructors / Destructors | ✅ | Including virtual destructors |
| Conversion operators | ✅ | `operator int()`, `operator bool()` |
| Operator overloads | ✅ | `operator+`, `operator==`, etc. |
| Enums (scoped & unscoped) | ✅ | Underlying type, enumerator values |
| Free functions | ✅ | Including `extern "C"` |
| Variadic functions | ✅ | `is_variadic` flag |
| Noexcept | ✅ | Parsed in method signatures |
| Templates (STL containers) | ✅ | `vector`, `array`, `map`, `variant`, `optional`, etc. |
| Nested templates | ✅ | e.g. `vector<vector<float>>` |
| Non-type template args | ✅ | e.g. `array<float, 3>` → integral value `3` |
| Member exclusion | ✅ | `LUX_META(no_reflect)` |
| Namespaces (deep nesting) | ✅ | Full qualified names |
| Anonymous structs/unions | ✅ | `is_anonymous` flag |
| Pointer types | ✅ | Object, function, member pointers |
| Reference types | ✅ | Lvalue & rvalue references |
| Const / Volatile qualifiers | ✅ | On types, fields, and methods |
| Default parameters | ✅ | Parameter types parsed (not default values) |

## Dependencies

- **libclang** — C++ AST parsing (via vcpkg: `llvm[clang]`)
- **nlohmann/json** — JSON serialization
- **inja** — Template rendering engine
- **C++20** — Required standard
