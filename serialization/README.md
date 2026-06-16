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
- **Or zero annotations:** reflect a third-party type you can't touch with
  `LUX_REFLECT_EXTERNAL(serializable, ::ext::Type)` written in your own code.
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

## External types (`LUX_REFLECT_EXTERNAL`)

When you cannot put `LUX_META` on a type — a third-party header you do not own —
register it **non-intrusively from your own code**. The generator follows the
registration to the real declaration and produces the same `meta_info<T>` /
`enum_meta<E>` as the intrusive path, so every backend works unchanged.

```cpp
// foreign.hpp (cannot modify):
//   namespace ext { struct Color { float r, g, b; };
//                    enum class Mode { Auto, Manual, Off }; }

// your own header, at global / namespace scope:
#include <lux/cxx/reflection/runtime/Marker.hpp>
#include "foreign.hpp"

LUX_REFLECT_EXTERNAL(serializable, ::ext::Color)       // reflect all PUBLIC members
LUX_REFLECT_EXTERNAL_ENUM(serializable, ::ext::Mode)   // reflect the enumerators
```

Point `lux_target_add_serialization(HEADERS ...)` at the header holding the
`LUX_REFLECT_EXTERNAL` lines. Only **public** members are reflected (a member
pointer to a private member cannot be formed from outside the class). For a type
with no reflectable members — e.g. `Eigen` / `Sophus`, whose data is private —
write a hand-written `serializer<T>` (see [below](#custom-types-non-intrusive))
instead.

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
result<T>     from_json<T>(std::string_view);               // one-shot: fresh parser per call

JsonParser    parser;                                       // keep one, parse many documents
result<T>     from_json<T>(JsonParser&, std::string_view);  // amortizes the parse buffers
```
The backend is selected at configure time — `-DLUX_SERIALIZATION_JSON=nlohmann` (default)
or `=simdjson` — and is fully hidden either way, so this header pulls in no third-party
type. For a high-throughput loop, hold one `JsonParser` and call `from_json(parser, text)`
per message: with the simdjson backend its tape and scratch buffer grow once and are then
reused, removing the per-message parser/padding allocations (a fixed ~constant per call).
A bare `parser.parse()` returns a cursor valid only until the next `parse()` on it;
`from_json(parser, text)` is the safe form — it loads into a `T` that owns its data.

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

`find_package(lux-cxx)` exposes the backends and the codegen helpers. The vendored
**tinyxml2 and nlohmann_json are internal** — you never request, link, or even see
them; they are compiled inside the backends.

```cmake
# The codegen helpers (include_component_cmake_scripts / add_meta / ...) are
# provided by lux-cmake-toolset. A lux-based project usually already finds it; a
# standalone consumer should add this line:
find_package(LUX-CMAKE-TOOLSET CONFIG REQUIRED)

find_package(lux-cxx REQUIRED COMPONENTS
    reflection_generator        # codegen helpers: add_meta / target_add_meta
    serialization               # core + lux_target_add_serialization
    serialization_json          # JSON backend  (request only the backends you use)
    serialization_xml)          # XML backend

# Pass the NAMESPACED component name. The bare name (e.g. "serialization") only
# exists inside lux-cxx's own build tree, never in a consumer project.
include_component_cmake_scripts(lux::cxx::reflection_generator)  # add_meta / target_add_meta
include_component_cmake_scripts(lux::cxx::serialization)         # lux_target_add_serialization

add_executable(app main.cpp)
target_link_libraries(app PRIVATE
    lux::cxx::serialization_json
    lux::cxx::serialization_xml)

# Generate <header-stem>.serialize.hpp from the annotated headers and attach it.
# No GENERATOR needed — the installed lux_meta_generator is located automatically
# (it is also exposed as the imported target lux::cxx::lux_meta_generator).
lux_target_add_serialization(
    TARGET  app
    HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/config.hpp
)
# In code: #include "config.hpp"  then  #include "config.serialize.hpp"
```

`lux_target_add_serialization(TARGET <t> HEADERS <h...> [NAME ..] [MARKER ..]
[META_SUFFIX ..] [GENERATOR ..] [OUT_DIR ..] [ECHO] [ALWAYS_REGENERATE])` runs
`lux_meta_generator` with the bundled `serializable.template.inja` and adds the
generated headers' directory to the target's include path. `GENERATOR` is
optional — omit it and the installed generator is found for you.

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
