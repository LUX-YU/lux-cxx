# lux::cxx Argument Parser

A modern, header‑only command‑line argument parser for C++20/23.

## Quick Start
```c++
#include "lux/cxx/arguments/Arguments.hpp"
int main(int argc, char* argv[])
{
    using namespace lux::cxx;

    Parser parser("mytool");

    // declare options
    auto port    = parser.add<int>("port",    "p").desc("TCP port to listen on").required();
    auto files   = parser.add<std::vector<std::string>>("file", "f").desc("input files").multi();
    auto verbose = parser.add<bool>("verbose", "v").desc("enable verbose logging");

    // parse – throws on error
    auto opts = parser.parse(argc, argv);

    int  p  = opts.get("port").as<int>();
    bool vb = opts.get("verbose");         // flag – implicit bool
    auto fs = opts.get("file").as<std::vector<std::string>>();

    ...
}
```

Compile with C++20 (or newer):
```bash
g++ -std=c++20 -Wall -Wextra -pedantic your_program.cpp -o your_program
```

## Command‑Line Conventions

|Feature | Example | Notes|
|--------|--------|--------|
|Long option | --port 8080 | |
|Short option| -p 8080 | Short names are optional|
|Inline value |--port=8080 -p8080|= or concatenation|
|Boolean flag |--verbose -v| Presence == true; accepts 0/1, yes/no|
|Multi‑value option|--file a.txt b.txt or repeat --file|Order preserved|
|Default value|declared via .def(value)|Shown in --help output|
|Required option|.required()|Missing → exception|

## Generated Help
```
$ mytool --help
Usage: mytool [options]

Options:
  -p, --port               TCP port to listen on (required)
  -f, --file               input files
  -v, --verbose            enable verbose logging
  -h, --help               Show this help and exit
```

## Extending value_parser

Special‑case types can specialize value_parser<T>:
```
template<>
struct value_parser<std::filesystem::path>
{
    static std::filesystem::path parse(std::string_view s)
    {
        return std::filesystem::path{s};
    }
};
```
Sequence types (containers with push_back) are auto‑detected.

## Error Handling

All user errors throw lux::cxx::parse_error with a descriptive message — just catch and print:
```
try {
    auto opts = parser.parse(argc, argv);
} catch (const lux::cxx::parse_error& e) {
    std::cerr << e.what() << '\n';
    return EXIT_FAILURE;
}
```

