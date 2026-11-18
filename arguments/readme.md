# Arguments

The `lux::cxx::arguments` component parses command-line options and structured values in C++20. In the source tree, link the header-only target directly. When consuming an installed SDK, request the `arguments` component first.

```cmake
find_package(lux-cxx CONFIG REQUIRED COMPONENTS arguments)
target_link_libraries(my_app PRIVATE lux::cxx::arguments)
```

## Parse into an owning result

Declare options with `args::Option<T, "long-name", "s">`. The short name is optional and must be a single character. Check the result before reading its values. `get<"name">()` returns a reference to the option's `std::optional<T>`, and `supplied<"name">()` tells you whether the input supplied the option explicitly.

```cpp
#include <lux/cxx/arguments/Help.hpp>
#include <lux/cxx/arguments/TypedOptions.hpp>
#include <iostream>

namespace args = lux::cxx::args;

int main(int argc, char** argv)
{
    constexpr auto port = args::Option<int, "port", "p">{}
        .def(8080)
        .check(args::inRange(1, 65535))
        .desc("TCP port");

    constexpr auto verbose = args::Option<bool, "verbose", "v">{}
        .def(false)
        .desc("Enable verbose logging");

    const auto result = args::parse(argc, argv, port, verbose);
    if (!result)
    {
        if (result.error().code == args::EErrorCode::HELP_REQUESTED)
        {
            std::cout << args::usage(argv[0], port, verbose);
            return 0;
        }
        std::cerr << args::describe(result.error(), port, verbose) << '\n';
        return 1;
    }

    std::cout << "port=" << *result.get<"port">()
              << " verbose=" << *result.get<"verbose">() << '\n';
}
```

`parse(argc, argv, ...)` skips `argv[0]`. `parse(text, ...)` takes option text beginning with the first option. `parse(span, ...)` takes a `std::span<const std::string_view>` of already separated tokens and reads every element. The text lexer handles quoting and escapes. With `argv` or a span, the caller has already split the input.

Common forms include `--port 8080`, `--port=8080`, `-p8080` and `port=8080`. Boolean options accept `--verbose`, `--no-verbose` and `--verbose=false`. Short boolean flags can be grouped, as in `-abc`. Use `.positional()` to declare a positional option and `--` to end option processing. `-h` and `--help` are reserved.

Use `args::Option<int>{"port", "p"}` when the name is determined at runtime. Read it with `result.get<int>("port", &port)`, which returns an `args::EReadStatus`. Use `get<"name">(&value)` to copy a compile-time named value and `take<"name">(&value)` to move it out of a mutable result.

## Bind directly to output objects

Use `bind` with `parseInto` to write values into existing objects. An absent option leaves its destination alone unless its descriptor supplies a default. A returned `Error` converts to `true` on failure. Direct binding can leave destinations partially updated on error. Use an owning result if the application needs to validate all input before committing changes.

```cpp
#include <lux/cxx/arguments/DirectBinding.hpp>
#include <string>

namespace args = lux::cxx::args;

bool configure(int& port, std::string& label)
{
    constexpr auto port_option = args::Option<int, "port">{}.required();
    constexpr auto label_option = args::Option<std::string, "label">{};

    const auto error = args::parseInto(
        R"(port=8080 label="Development server")",
        args::bind(port_option, port),
        args::bind(label_option, label)
    );
    return !error;
}
```

For repeated parsing, pass a `Workspace` to `parseFrom` or `parseIntoFrom` along with a source. Use `TextSource` for ordinary text and `StructuredTextSource` for text containing bracketed values. Each concurrent call needs its own source, workspace and writable destinations. Input views must refer to storage that stays valid for the call and does not overlap mutable outputs.

## Containers and structured values

By default a container option reads one complete value. Include `StructuredValue.hpp` for bracketed values, or `ContainerTraits.hpp` for plain `.multi()` options.

```cpp
#include <lux/cxx/arguments/StructuredValue.hpp>
#include <lux/cxx/arguments/TypedOptions.hpp>
#include <array>
#include <string>
#include <vector>

namespace args = lux::cxx::args;

bool readOptions()
{
    constexpr auto position = args::Option<std::array<int, 3>, "position">{};
    constexpr auto inputs = args::Option<std::vector<std::string>, "input", "i">{}.multi();

    const auto result = args::parse(
        "position=[1, 2, 3] -i a.png b.png -i c.png",
        position,
        inputs
    );
    return static_cast<bool>(result);
}
```

| Value type | Example |
| --- | --- |
| Sequence or set | `[1, 2, 3]` |
| Fixed-size array, tuple or pair | `[1, true]`, with the exact declared number of elements |
| Map | `{width:1920, height:1080}` |
| Optional | `null` or a value of the wrapped type |
| Nested container | `{camera:[1, 2, 3]}` |

`.multi()` converts and accumulates individual elements. Use `.arity(min, max)` to bound the number of elements per occurrence or `.delimiter(',')` to split values. Repeated ordinary options keep the last value by default. Multi-value options append by default. Override the policy with `.onRepeat(args::ERepeat::REJECT)` or `ERepeat::FIRST`, `LAST` or `APPEND`. `APPEND` applies to multi-value options. Repeated keys inside a map are rejected by default. Use `.convert(args::structured(settings))` with `StructuredSettings` to configure duplicate keys, nesting depth and element limits.

## Validation and extension

- `.def(value)` stores a typed default in the descriptor. `.required()` still requires explicit input when a default is present.
- `.check(predicate)` validates a complete value, or each element of a `.multi()` option. `inRange` and `oneOf` provide common validators.
- `.convert(converter)` overrides conversion for one option by calling `converter(std::string_view, T&)`. Specialize `args::ValueParser<T>` and provide a static `parse(std::string_view, T&)` for a reusable type conversion. Both return `EErrorCode` or `ValueError`.
- Specialize `SequenceTraits<T>`, `MapTraits<T>` or `TupleTraits<T>` to adapt containers. `parseStructured(text, out)` also parses a standalone structured value.
- Values such as `std::string` own their data. Declare custom borrowed types with `BorrowsInput<T>` so they are rejected as outputs. Keep text passed to `.desc()` alive while using the descriptor.

## Headers and migration

| Header | API |
| --- | --- |
| `Arguments.hpp` | Includes the full module and the compatibility API |
| `TypedOptions.hpp` | `Option`, `parse` and owning results |
| `DirectBinding.hpp` | `bind` and `parseInto` |
| `ContainerTraits.hpp` | STL container adapters for multi-value options |
| `StructuredValue.hpp` | Complete container values and `parseStructured` |
| `Help.hpp` | `usage` and `describe` |

Existing `lux::cxx::Parser` callers, including the serialization backend, can continue to include `Arguments.hpp`. For new code, declare `lux::cxx::args::Option` descriptors, pass them to `parse` or `parseInto`, and use `.multi()` where the CLI accepts separate values. In the new API a text input starts at the first option, so remove the program name when migrating calls from `Parser::parse(text)`.

With the project configured for testing, build and run the argument tests with the following commands.

```sh
cmake --build <build-dir>
ctest --test-dir <build-dir> -R arguments --output-on-failure
```
