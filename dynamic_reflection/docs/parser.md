# Parser Features

The `CxxParser` is the core component that uses **libclang** to parse C++ source files and extract reflection metadata. This document covers all supported features, the annotation system, and how to configure parsing options.

## Annotation System

### Marker Macro

The `LUX_META(...)` macro is defined in `<lux/cxx/dref/runtime/Marker.hpp>`:

```cpp
#if defined __LUX_PARSE_TIME__
#   define LUX_META(...) __attribute__((annotate(#__VA_ARGS__)))
#else
#   define LUX_META(...)
#endif
```

At parse time, libclang sees `__attribute__((annotate("...")))`, which attaches metadata to declarations. At normal compile time, the macro expands to nothing — zero runtime overhead.

### Marking Declarations

Use `LUX_META(<marker_symbol>)` on any declaration you want to include in the reflection output:

```cpp
struct LUX_META(marked) MyComponent { ... };       // struct/class
enum class LUX_META(marked) MyEnum { ... };         // enum
void LUX_META(marked) MyFunction(int a) { ... }    // free function
```

The `marker_symbol` is configured in `ParseOptions::marker_symbol`. Only declarations with a matching annotation are included in the output.

### Excluding Members

Use `LUX_META(<exclude_symbol>)` on individual fields or methods to exclude them from reflection:

```cpp
struct LUX_META(marked) PlayerComponent
{
    float health;                              // ✅ reflected
    float speed;                               // ✅ reflected
    LUX_META(no_reflect) int _internal_state;  // ❌ excluded
    LUX_META(no_reflect) void _tick_impl();    // ❌ excluded

    void takeDamage(float amount);             // ✅ reflected
};
```

The `exclude_symbol` is configured in `ParseOptions::exclude_symbol` (default: `"no_reflect"`).

**Exclusion applies to:**
- `FieldDecl` — data members
- `CXXMethod` — non-static and static methods
- `ConversionFunction` — conversion operators (`operator int()`, etc.)

**Not excluded (always parsed if present):**
- `Constructor` — constructors are always included
- `Destructor` — destructors are always included
- `CXXBaseSpecifier` — base class relationships are always included

### Custom Annotations

Annotations are stored in the `attributes` array of each declaration. You can attach multiple annotations separated by `;`:

```cpp
LUX_META(marked; serializable; category=physics) float mass;
```

This produces `attributes: ["marked", "serializable", "category=physics"]`. Your templates or runtime code can inspect these freely.

## ParseOptions

```cpp
struct ParseOptions
{
    std::string              name;            // Library/module name
    std::string              version;         // Version string
    std::string              marker_symbol;   // Declaration marker (e.g., "marked")
    std::string              exclude_symbol;  // Member exclusion marker (default: "no_reflect")
    std::vector<std::string> commands;        // Clang compile flags
    std::string              pch_file;        // Pre-compiled header (future)
};
```

### Compile Commands

The `commands` field accepts standard Clang compile flags:

```cpp
.commands = {
    "-std=c++20",
    "-I/path/to/include",
    "-DSOME_DEFINE=1",
    "-Wno-some-warning",
}
```

> **Note**: On MSVC, the parser automatically injects `-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH` to prevent STL version check failures when libclang's reported Clang version is older than what the MSVC STL headers expect. You don't need to add this manually.

## Supported Declaration Types

### Records (Class / Struct / Union)

```cpp
struct LUX_META(marked) MyStruct { ... };  // tag_kind = Struct
class  LUX_META(marked) MyClass  { ... };  // tag_kind = Class
union  LUX_META(marked) MyUnion  { ... };  // tag_kind = Union
```

Parsed properties:
- `tag_kind` — Struct, Class, or Union
- `field_decls` — non-static data members (with offsets and visibility)
- `method_decls` — non-static methods
- `static_method_decls` — static methods
- `constructor_decls` — constructors
- `destructor_decl` — destructor (if present)
- `bases` — direct base classes
- `is_abstract` — true if any pure virtual method exists

### Inheritance

```cpp
class LUX_META(marked) Base { virtual void foo() = 0; };
class LUX_META(marked) Derived : public Base { void foo() override {} };
class LUX_META(marked) Multi : public A, public B { };
```

- Base classes are resolved recursively
- Multiple inheritance is supported
- `parent_chain()` callback available in templates for traversing inheritance hierarchies

### Methods

All method qualifiers are detected:

```cpp
struct LUX_META(marked) Example {
    void normal_method();
    void const_method() const;
    void volatile_method() volatile;
    void const_volatile() const volatile;
    virtual void virtual_method();
    static void static_method();
    void variadic(int a, ...);
};
```

Parsed flags: `is_const`, `is_volatile`, `is_virtual`, `is_static`, `is_variadic`

### Conversion Operators

```cpp
struct LUX_META(marked) Convertible {
    operator int() const;
    explicit operator bool() const;
};
```

These are parsed as `CXXConversionDecl` (kind = `CXX_CONVERSION_DECL`) and appear in `method_decls`.

### Enums

```cpp
enum class LUX_META(marked) Color : uint8_t { Red = 0, Green = 1, Blue = 2 };
enum LUX_META(marked) OldEnum { A, B, C };
```

Parsed properties:
- `is_scoped` — `true` for `enum class`
- `underlying_type` — The underlying integer type (e.g., `uint8_t`)
- `enumerators` — List of `{name, signed_value, unsigned_value}`

### Free Functions

```cpp
void LUX_META(marked) process(int a, double b);
extern "C" void LUX_META(marked) c_function(int x);
```

Parsed properties: `result_type`, `params`, `is_variadic`, `mangling`, `invoke_name`

## Template Argument Parsing

When a field type is a template specialization (e.g., `std::vector<int>`), the parser extracts structured template argument information.

### RecordType Extensions

For template specializations, `RecordType` provides:

| Field | Type | Description |
|-------|------|-------------|
| `template_name` | `string` | Template name without arguments, e.g. `"std::vector"` |
| `template_arguments` | `vector<TemplateArgument>` | Ordered list of template arguments |
| `isTemplateSpecialization()` | `bool` | Convenience check: `!template_arguments.empty()` |

### TemplateArgument

```cpp
struct TemplateArgument {
    enum class Kind { Type, Integral };

    Kind         kind;            // Type or Integral
    Type*        type;            // For Kind::Type: pointer to the argument type
    int64_t      integral_value;  // For Kind::Integral: the compile-time value
    std::string  type_id;         // Serialization: the type ID string
    std::string  spelling;        // Human-readable: "int", "float", "3", etc.
};
```

### Supported Template Patterns

| C++ Type | `template_name` | Arguments |
|----------|-----------------|-----------|
| `std::vector<int>` | `std::vector` | `[Type("int"), Type("std::allocator<int>")]` |
| `std::array<float, 3>` | `std::array` | `[Type("float"), Integral(3)]` |
| `std::map<std::string, int>` | `std::map` | `[Type("std::string"), Type("int"), ...]` |
| `std::variant<int, float, std::string>` | `std::variant` | `[Type("int"), Type("float"), Type("std::string")]` |
| `std::optional<double>` | `std::optional` | `[Type("double")]` |
| `std::vector<std::vector<float>>` | `std::vector` | `[Type("std::vector<float>"), ...]` — inner type is also a RecordType with its own template_arguments |

### Nested Template Resolution

Template arguments that are themselves template specializations are fully resolved. For example, `std::vector<std::vector<float>>`:

```
RecordType (std::vector<std::vector<float>>)
├── template_name: "std::vector"
└── template_arguments[0]:
    ├── kind: Type
    └── type → RecordType (std::vector<float>)
              ├── template_name: "std::vector"
              └── template_arguments[0]:
                  ├── kind: Type
                  └── type → BuiltinType (float)
```

### Example: Querying Template Info

```cpp
for (auto* field : record->field_decls) {
    auto* rt = dynamic_cast<RecordType*>(field->type);
    if (rt && rt->isTemplateSpecialization()) {
        std::cout << field->spelling << ": " << rt->template_name << "<";
        for (size_t i = 0; i < rt->template_arguments.size(); i++) {
            if (i > 0) std::cout << ", ";
            auto& arg = rt->template_arguments[i];
            if (arg.kind == TemplateArgument::Kind::Type)
                std::cout << arg.spelling;
            else
                std::cout << arg.integral_value;
        }
        std::cout << ">\n";
    }
}
```

## Type System

The parser recognizes and categorizes the following type kinds:

| Category | Types | `ETypeKinds` |
|----------|-------|-------------|
| Void | `void` | `Void` |
| Nullptr | `std::nullptr_t` | `Nullptr_t` |
| Boolean | `bool` | `Bool` |
| Integer | `char`, `short`, `int`, `long`, `long long` (signed & unsigned) | `Char`, `Short`, `Int`, `Long`, `LongLong`, `UnsignedChar`, etc. |
| Char | `char8_t`, `char16_t`, `char32_t`, `wchar_t` | `Char8`, `Char16`, `Char32`, `WChar` |
| Float | `float`, `double`, `long double` | `Float`, `Double`, `LongDouble` |
| Pointer | `T*`, `T C::*` | `PointerToObject`, `PointerToFunction`, `PointerToDataMember`, `PointerToMemberFunction` |
| Reference | `T&`, `T&&` | `LvalueReference`, `RvalueReference` |
| Record | `struct`, `class`, `union` | `Record`, `Class`, `Union` |
| Enum | `enum`, `enum class` | `UnscopedEnum`, `ScopedEnum` |
| Function | function types | `Function` |
| Array | `T[N]` | `Array` |

Each type carries `is_const`, `is_volatile`, `size`, and `align` information.

## Error Handling

```cpp
CxxParser parser(options);
parser.setOnParseError([](const std::string& error) {
    std::cerr << "[Reflection Error] " << error << std::endl;
});

auto [result, meta] = parser.parse("my_header.hpp");
if (result != EParseResult::SUCCESS) {
    // Handle failure
}
```

`setOnParseError` receives both libclang diagnostics and parser-level errors. The parse result is `SUCCESS`, `UNKNOWN_TYPE`, or `FAILED`.
