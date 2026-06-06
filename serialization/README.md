# Serialization (`lux::cxx::ser`)

Reflection-driven serialization for C++20. **Annotate a type once** and get
JSON, XML, and command-line (Arguments) binding for free — no per-type
boilerplate, and **no third-party header leaks into your build** (nlohmann_json
and tinyxml2 are hidden inside compiled `.cpp`s, so they can never clash with
your own copies).

It is built on top of the [`reflection`](../reflection/README.md) code
generator: a `LUX_META(serializable)` annotation drives generation of a
compile-time `meta_info<T>` (field names + member pointers + options), and a
single generic traversal turns that into any supported format.

---

## At a glance

- **One annotation:** `LUX_META(serializable)` on a struct/class (and on enums).
- **Backends:** JSON · XML · Arguments (CLI). Same data model, pluggable sinks.
- **Types:** scalars, `bool`, strings, enums (by name), most std containers
  (`vector`/`list`/`deque`/`set`/`map`/`unordered_map`), `pair`/`tuple`/`array`,
  `optional`, `unique_ptr`/`shared_ptr`, nested serializable types, and
  `std::vector<std::byte>` (→ Base64).
- **Declarative options:** `name` / `skip` / `required` / `omit_empty` /
  `flatten` / `short` / `xml_attribute`.
- **Escape hatch:** specialize `lux::cxx::ser::serializer<T>` for any type the
  generator can't see.

---

## Quick start

```cpp
// config.hpp  — annotate your types
#include <lux/cxx/reflection/runtime/Marker.hpp>
#include <string>
#include <vector>
#include <optional>

enum class LUX_META(serializable) LogLevel { Error, Warn, Info, Debug };

struct LUX_META(serializable) Db {
    std::string host;
    int         port;
};

struct LUX_META(serializable) Config {
    std::string              name;
    LogLevel                 level;        // serialized by NAME ("Info")
    bool                     verbose;
    std::vector<std::string> inputs;
    std::optional<int>       limit;
    Db                       db;           // nested
};
```

```cpp
// main.cpp
#include "config.hpp"
#include "config.serialize.hpp"           // generated (see CMake below)

#include <lux/cxx/serialization/json.hpp>
#include <lux/cxx/serialization/xml.hpp>
#include <lux/cxx/serialization/arguments.hpp>
using namespace lux::cxx::ser;

int main(int argc, char** argv) {
    Config c{ "srv", LogLevel::Info, true, {"a","b"}, 100, { "localhost", 5432 } };

    std::string j = to_json(c, 2);                       // -> JSON text
    auto c2 = from_json<Config>(j);                      // -> result<Config>

    std::string x = to_xml(c, "config");                 // -> XML text
    auto c3 = from_xml<Config>(x);

    auto c4 = from_args<Config>(argc, argv);             // CLI -> struct
    std::cout << usage<Config>("app");                   // --name --level --db.port ...
}
```

All deserializers return `result<T>` = `lux::cxx::expected<T, ser::error>`.

---

## Annotations

Apply on a field as `LUX_META(serializable, <key>[, <key>=<value>...])`.

| Key             | Effect                                                            | Backends |
|-----------------|------------------------------------------------------------------|----------|
| `name="x"`      | Override the serialized key / option name                        | all      |
| `skip`          | Never (de)serialize this field                                   | all      |
| `required`      | Error if the field is missing on load                            | all      |
| `omit_empty`    | Skip emitting the field when its value is empty/default          | json/xml |
| `flatten`       | Inline a nested struct (no extra path segment)                   | all      |
| `short="p"`     | Short option name (`-p`)                                         | args     |
| `xml_attribute` | Emit a scalar as an XML attribute instead of a child element     | xml      |

The record itself is marked with `LUX_META(serializable)` (this is the generator
*marker*). Enums marked `LUX_META(serializable)` get an `enum_meta<E>` and are
serialized **by name**; unmarked enums fall back to their underlying integer.

---

## Type support

| Category        | Types                                              | Representation |
|-----------------|---------------------------------------------------|----------------|
| Scalars         | `bool`, integers, floating point                  | native |
| Strings         | `std::string` / `std::string_view` / `const char*`| string |
| Reflected       | structs/classes with `meta_info<T>`               | object |
| Enums           | `enum`/`enum class` with `enum_meta<E>`           | name (else integer) |
| Sequences       | `vector` / `list` / `deque` / `set` / …           | array |
| Maps            | `map` / `unordered_map`                            | object (string keys) or `[[k,v],…]` |
| Tuple-like      | `pair` / `tuple` / `array`                         | fixed array |
| Nullable        | `optional` / `unique_ptr` / `shared_ptr`          | value or `null` |
| Bytes           | `std::vector<std::byte>`                           | Base64 string |
| Anything else   | specialize `ser::serializer<T>`                   | user-defined |

---

## Backends

### JSON — `<lux/cxx/serialization/json.hpp>` (link `lux::cxx::serialization_json`)
```cpp
std::string   to_json(const T&, int indent = -1);
result<T>     from_json<T>(std::string_view);
```
Backed by a vendored nlohmann_json, fully hidden behind the compiled backend.

### XML — `<lux/cxx/serialization/xml.hpp>` (link `lux::cxx::serialization_xml`)
```cpp
std::string   to_xml(const T&, std::string_view root = "root", bool pretty = true);
result<T>     from_xml<T>(std::string_view);
```
Mapping: object → child elements; sequence/tuple → repeated `<item>`;
string-keyed map → elements named by key; `xml_attribute` scalars → attributes;
disengaged `optional` → element omitted. Backed by a vendored tinyxml2 (hidden).

### Arguments (CLI) — `<lux/cxx/serialization/arguments.hpp>` (link `lux::cxx::serialization_arguments`)
```cpp
result<T>        from_args<T>(int argc, char** argv, std::string prog = "app");
result<T>        from_args<T>(const std::string& cmdline, std::string prog = "app");
std::string      usage<T>(std::string prog);
lux::cxx::Parser make_arg_parser<T>(std::string prog);
```
Nested structs flatten into dotted options (`--db.host`). Reuses the
[`arguments`](../arguments/readme.md) module's `value_parser<T>` for conversions.
Supported leaves: scalars, `bool` flags, strings, enums (by name), `optional<scalar>`,
and `push_back` sequences (`--inputs a b`).

---

## CMake integration

```cmake
find_package(lux-cxx REQUIRED COMPONENTS
    reflection_generator serialization serialization_json)

include_component_cmake_scripts(reflection_generator)   # add_meta / target_add_meta
include_component_cmake_scripts(serialization)          # lux_target_add_serialization

add_executable(app main.cpp)
target_link_libraries(app PRIVATE lux::cxx::serialization_json)

# Generate <header-stem>.serialize.hpp from the annotated headers and attach it.
lux_target_add_serialization(
    TARGET  app
    HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/config.hpp
)
# In code: #include "config.hpp"  then  #include "config.serialize.hpp"
```

`lux_target_add_serialization(TARGET <t> HEADERS <h...> [NAME ..] [MARKER ..]
[META_SUFFIX ..] [GENERATOR ..] [OUT_DIR ..] [ECHO] [ALWAYS_REGENERATE])`
runs `lux_meta_generator` with the bundled `serializable.template.inja` and adds
the generated headers' directory to the target's include path.

---

## Custom types (non-intrusive)

For third-party types or special encodings, specialize `serializer<T>`:

```cpp
template <>
struct lux::cxx::ser::serializer<glm::vec3> {
    static constexpr bool is_specialized = true;
    template <class Ar> static void save(Ar& ar, const glm::vec3& v) {
        ar.begin_array(3); ar.value(double(v.x)); ar.value(double(v.y)); ar.value(double(v.z)); ar.end_array();
    }
    template <class Cur> static bool load(const Cur& c, glm::vec3& v) {
        if (!c.is_array() || c.size() < 3) return false;
        double x{}, y{}, z{};
        bool ok = c.element(0).read(x) && c.element(1).read(y) && c.element(2).read(z);
        v = { float(x), float(y), float(z) }; return ok;
    }
};
```

A `serializer<T>` works across **all** backends, because backends only implement
the small archive primitive set (`begin_object`/`key`/`value`/… and a read cursor).

---

## Architecture

```
LUX_META(serializable)  ──(lux_meta_generator + serializable.template.inja)──►  meta_info<T> / enum_meta<E>
                                                                                       │
                                          core.hpp:  save(ar,v) / load(cur,out)  ◄──────┘  (the only traversal)
                                                          │  dispatches by type, recurses
                          ┌───────────────┬───────────────┼───────────────┐
                     JsonArchive      XmlArchive     ArgumentsArchive   (your serializer<T>)
                     (nlohmann)       (tinyxml2)     (lux::cxx::Parser)
                     hidden in .cpp   hidden in .cpp   header-only
```

---

## Limitations

- Maps with non-string keys serialize as `[[k,v],…]` (JSON/XML); XML string-map
  keys must be valid XML element names.
- The Arguments backend is for *flat/shallow* config; deeply nested types become
  long dotted option names. Maps/tuples/containers-of-structs have no CLI form.
- Raw pointers are not serialized by default (specialize `serializer<T>`).
- `enum_meta`/`meta_info` require running the generator (or hand-written
  specializations). Until C++26 reflection, this is the codegen step above.
