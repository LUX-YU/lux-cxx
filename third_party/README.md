# third_party — vendored dependencies

These libraries are bundled in-tree so lux-cxx needs no external `find_package`
for them. Their licenses permit redistribution.

| Library | Version | License | Layout | CMake target |
|---|---|---|---|---|
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | MIT | `nlohmann_json/include/nlohmann/` (header-only) | `lux::cxx::nlohmann_json` (INTERFACE) |
| [tinyxml2](https://github.com/leethomason/tinyxml2) | 11.0.0 | zlib | `tinyxml2/include/tinyxml2.h` + `tinyxml2/src/tinyxml2.cpp` | compiled into `serialization_xml` |
| [rapidyaml](https://github.com/biojppm/rapidyaml) | 0.16.0 | MIT | `rapidyaml/include/rapidyaml/rapidyaml.hpp` (official amalgamation) | compiled into `serialization_yaml` |

The rapidyaml amalgamation is pinned by SHA-256:
`0D0B8076174CF62F034406B03529FDA542EBC9A17506D3BD6D949AEDC4BFA6AB`.

Only nlohmann_json has an internal CMake target. tinyxml2 and rapidyaml are
private backend implementation details and must not be included by consumers.
License files live next to each library.

## Updating

- **nlohmann_json**: copy the `include/nlohmann/` tree from the desired release.
- **tinyxml2**: copy `tinyxml2.h` → `include/`, `tinyxml2.cpp` → `src/`, refresh `LICENSE.txt`.
- **rapidyaml**: copy the official `singlehdr.hpp` release asset to
  `include/rapidyaml/rapidyaml.hpp`, refresh `LICENSE.txt`, and update the pinned
  SHA-256 documented by the serialization backend.

Keep the bundled nlohmann_json version in sync with the one `inja` (used only by
`lux_meta_generator` at build time) expects, to avoid ODR issues in that one tool.
