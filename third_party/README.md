# third_party — vendored dependencies

These libraries are bundled in-tree so lux-cxx needs no external `find_package`
for them. Both licenses permit redistribution.

| Library | Version | License | Layout | CMake target |
|---|---|---|---|---|
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | MIT | `nlohmann_json/include/nlohmann/` (header-only) | `lux::cxx::nlohmann_json` (INTERFACE) |
| [tinyxml2](https://github.com/leethomason/tinyxml2) | 11.0.0 | zlib | `tinyxml2/include/tinyxml2.h` + `tinyxml2/src/tinyxml2.cpp` | `lux::cxx::tinyxml2` (STATIC) |

Usage: `#include <nlohmann/json.hpp>` / `#include <tinyxml2.h>` after linking the
corresponding target. License files live next to each library
(`nlohmann_json/LICENSE.MIT`, `tinyxml2/LICENSE.txt`).

## Updating

- **nlohmann_json**: copy the `include/nlohmann/` tree from the desired release.
- **tinyxml2**: copy `tinyxml2.h` → `include/`, `tinyxml2.cpp` → `src/`, refresh `LICENSE.txt`.

Keep the bundled nlohmann_json version in sync with the one `inja` (used only by
`lux_meta_generator` at build time) expects, to avoid ODR issues in that one tool.
