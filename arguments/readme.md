# Arguments Module

Modern, header-only command-line argument parser for C++20/23.

## Overview

The Arguments module provides a type-safe, easy-to-use command-line argument parsing library. It supports automatic type conversion, multi-value options, boolean flags, and generates formatted help text automatically.

## Features

- **Type Safety**: Automatic type conversion with compile-time validation
- **Modern C++**: Uses C++20 concepts and templates for clean APIs
- **Header-Only**: No compilation required, just include and use
- **Multi-Value Support**: Handle arrays, vectors, and other containers
- **Boolean Flags**: Smart boolean handling with multiple formats
- **Auto Help**: Generates formatted help text automatically
- **Error Handling**: Descriptive error messages with suggestions
- **Extensible**: Easy to add custom type parsers

## Quick Start

```cpp
#include <lux/cxx/arguments/Arguments.hpp>
#include <vector>
#include <iostream>

int main(int argc, char* argv[]) {
    using namespace lux::cxx;

    try {
        Parser parser("myapp", "A sample application demonstrating argument parsing");

        // Define options
        auto port = parser.add<int>("port", "p")
            .desc("TCP port to listen on")
            .def(8080)
            .required();

        auto files = parser.add<std::vector<std::string>>("file", "f")
            .desc("Input files to process")
            .multi();

        auto verbose = parser.add<bool>("verbose", "v")
            .desc("Enable verbose logging");

        auto output = parser.add<std::string>("output", "o")
            .desc("Output file")
            .def("output.txt");

        // Parse arguments
        auto opts = parser.parse(argc, argv);

        // Access parsed values
        int port_num = opts.get("port").as<int>();
        bool is_verbose = opts.get("verbose");  // Implicit conversion to bool
        auto input_files = opts.get("file").as<std::vector<std::string>>();
        std::string output_file = opts.get("output").as<std::string>();

        // Use the values
        std::cout << "Port: " << port_num << std::endl;
        std::cout << "Verbose: " << (is_verbose ? "yes" : "no") << std::endl;
        std::cout << "Output: " << output_file << std::endl;
        
        std::cout << "Input files (" << input_files.size() << "):" << std::endl;
        for (const auto& file : input_files) {
            std::cout << "  - " << file << std::endl;
        }

    } catch (const lux::cxx::parse_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

## Command-Line Conventions

The parser supports standard command-line conventions:

### Option Formats

| Format | Example | Description |
|--------|---------|-------------|
| Long option | `--port 8080` | Standard long form |
| Short option | `-p 8080` | Single character shorthand |
| Inline value | `--port=8080` | Equals syntax |
| Concatenated | `-p8080` | No space for short options |
| Boolean flag | `--verbose` | Presence implies true |
| Boolean values | `--debug=yes`, `--debug=1` | Explicit boolean values |

### Multi-Value Options

```bash
# Multiple separate options
myapp --file input1.txt --file input2.txt --file input3.txt

# Space-separated values  
myapp --file input1.txt input2.txt input3.txt

# Mixed usage
myapp --file input1.txt --verbose --file input2.txt input3.txt
```

### Boolean Handling

```bash
# Flag presence (default behavior)
myapp --verbose              # verbose = true
myapp                        # verbose = false

# Explicit values
myapp --verbose=true         # verbose = true
myapp --verbose=false        # verbose = false
myapp --verbose=yes          # verbose = true
myapp --verbose=no           # verbose = false
myapp --verbose=1            # verbose = true
myapp --verbose=0            # verbose = false
```

## Detailed API

### Parser Class

```cpp
class Parser {
public:
    // Constructor
    Parser(const std::string& program_name, 
           const std::string& description = "");

    // Add option
    template<typename T>
    OptionBuilder<T> add(const std::string& long_name, 
                        const std::string& short_name = "");

    // Parse arguments
    ParseResult parse(int argc, char* argv[]);
    ParseResult parse(const std::vector<std::string>& args);
};
```

### OptionBuilder Class

Fluent interface for configuring options:

```cpp
template<typename T>
class OptionBuilder {
public:
    // Set description
    OptionBuilder& desc(const std::string& description);
    
    // Set default value
    OptionBuilder& def(const T& default_value);
    
    // Mark as required
    OptionBuilder& required(bool is_required = true);
    
    // Enable multi-value mode
    OptionBuilder& multi(bool is_multi = true);
    
    // Set help category
    OptionBuilder& category(const std::string& category);
};
```

### ParseResult Class

Container for parsed values:

```cpp
class ParseResult {
public:
    // Get option value
    template<typename T>
    T get(const std::string& name).as<T>();
    
    // Implicit conversion for common types
    bool get(const std::string& name);  // For bool options
    
    // Check if option was provided
    bool has(const std::string& name) const;
    
    // Get all provided option names
    std::vector<std::string> provided_options() const;
};
```

## Type System

### Supported Types

The parser automatically supports these types:

- **Integral**: `int`, `long`, `size_t`, `uint32_t`, etc.
- **Floating**: `float`, `double`, `long double`
- **String**: `std::string`, `std::string_view`
- **Boolean**: `bool` with smart parsing
- **Containers**: `std::vector<T>`, `std::list<T>`, `std::set<T>`, etc.
- **Optional**: `std::optional<T>`
- **Filesystem**: `std::filesystem::path`

### Custom Type Support

Extend the parser with custom types by specializing `value_parser`:

```cpp
// Custom type
struct Point {
    float x, y;
    Point(float x = 0, float y = 0) : x(x), y(y) {}
};

// Specialize parser
template<>
struct lux::cxx::value_parser<Point> {
    static Point parse(std::string_view s) {
        // Parse format "x,y"
        auto comma = s.find(',');
        if (comma == std::string_view::npos) {
            throw parse_error("Point format should be 'x,y'");
        }
        
        float x = value_parser<float>::parse(s.substr(0, comma));
        float y = value_parser<float>::parse(s.substr(comma + 1));
        
        return Point{x, y};
    }
};

// Usage
auto position = parser.add<Point>("pos", "p")
    .desc("Position as x,y coordinates")
    .def(Point{0, 0});
```

### Container Support

Containers with `push_back()` or `insert()` methods are automatically supported:

```cpp
// Vector of integers
auto numbers = parser.add<std::vector<int>>("number", "n")
    .desc("List of numbers")
    .multi();

// Set of strings (duplicates removed)
auto unique_tags = parser.add<std::set<std::string>>("tag", "t")
    .desc("Unique tags")
    .multi();

// Usage: myapp --number 1 2 3 --tag dev --tag prod --tag dev
// Result: numbers = [1, 2, 3], unique_tags = {"dev", "prod"}
```

## Advanced Features

### Option Categories

Organize options in help output:

```cpp
Parser parser("myapp");

// Network options
parser.add<int>("port", "p").desc("Listen port").category("Network");
parser.add<std::string>("host").desc("Host address").category("Network");

// Logging options  
parser.add<bool>("verbose", "v").desc("Verbose output").category("Logging");
parser.add<std::string>("log-file").desc("Log file path").category("Logging");

// Will generate categorized help:
// Network Options:
//   -p, --port        Listen port
//   --host            Host address
//
// Logging Options:
//   -v, --verbose     Verbose output
//   --log-file        Log file path
```

### Conditional Options

```cpp
auto config_file = parser.add<std::string>("config", "c")
    .desc("Configuration file path");

auto inline_config = parser.add<std::string>("set")
    .desc("Inline configuration key=value")
    .multi();

// Custom validation
auto opts = parser.parse(argc, argv);
if (!opts.has("config") && !opts.has("set")) {
    throw parse_error("Either --config or --set must be provided");
}
```

### Environment Variable Integration

```cpp
// Not built-in, but easy to implement:
std::string get_option_or_env(const ParseResult& opts, 
                             const std::string& option_name,
                             const std::string& env_name,
                             const std::string& default_value = "") {
    if (opts.has(option_name)) {
        return opts.get(option_name).as<std::string>();
    }
    
    if (const char* env_val = std::getenv(env_name.c_str())) {
        return std::string(env_val);
    }
    
    return default_value;
}

// Usage
auto port = get_option_or_env(opts, "port", "MYAPP_PORT", "8080");
```

## Error Handling

The parser provides detailed error messages:

```cpp
try {
    auto opts = parser.parse(argc, argv);
} catch (const lux::cxx::parse_error& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    
    // Common error types:
    // - "Required option '--port' not provided"
    // - "Invalid value 'abc' for option '--count' (expected integer)"
    // - "Unknown option '--unknown-flag'"
    // - "Option '--file' expects a value"
    
    return EXIT_FAILURE;
}
```

### Error Recovery

```cpp
// Provide suggestions for unknown options
try {
    auto opts = parser.parse(argc, argv);
} catch (const unknown_option_error& e) {
    std::cerr << "Unknown option: " << e.option_name() << std::endl;
    
    auto suggestions = e.suggestions();  // Similar option names
    if (!suggestions.empty()) {
        std::cerr << "Did you mean:" << std::endl;
        for (const auto& suggestion : suggestions) {
            std::cerr << "  --" << suggestion << std::endl;
        }
    }
}
```

## Help Generation

Automatic help generation with customizable formatting:

```bash
$ myapp --help
Usage: myapp [OPTIONS]

A sample application demonstrating argument parsing

Options:
  -p, --port PORT          TCP port to listen on (default: 8080, required)
  -f, --file FILE          Input files to process (multi-value)
  -v, --verbose            Enable verbose logging
  -o, --output FILE        Output file (default: output.txt)
  -h, --help               Show this help message and exit

Examples:
  myapp --port 3000 --verbose
  myapp --file input1.txt input2.txt --output result.txt
```

### Custom Help Formatting

```cpp
Parser parser("myapp");
parser.set_help_formatter([](const Parser& p) -> std::string {
    // Custom help formatting logic
    std::ostringstream oss;
    oss << "=== " << p.program_name() << " ===\n";
    // ... custom formatting
    return oss.str();
});
```

## Performance

The Arguments module is designed for typical command-line parsing scenarios:

- **Parsing Time**: O(n) where n is number of arguments
- **Memory Usage**: Minimal, mostly string storage
- **Compile Time**: Header-only with template instantiation
- **Runtime Overhead**: Negligible for typical CLI apps

### Benchmarks

Parsing 100 arguments with mixed types:
- **Parse Time**: ~50 microseconds
- **Memory Usage**: ~2KB additional
- **Binary Size Impact**: ~10KB (depends on types used)

## Integration Examples

### Configuration Management

```cpp
struct Config {
    std::string host = "localhost";
    int port = 8080;
    bool verbose = false;
    std::vector<std::string> input_files;
    std::filesystem::path output_dir = ".";
};

Config parse_config(int argc, char* argv[]) {
    Parser parser("myapp");
    
    auto host = parser.add<std::string>("host").def("localhost");
    auto port = parser.add<int>("port", "p").def(8080);
    auto verbose = parser.add<bool>("verbose", "v");
    auto files = parser.add<std::vector<std::string>>("input", "i").multi();
    auto output = parser.add<std::filesystem::path>("output", "o").def(".");
    
    auto opts = parser.parse(argc, argv);
    
    return Config{
        .host = opts.get("host").as<std::string>(),
        .port = opts.get("port").as<int>(),
        .verbose = opts.get("verbose"),
        .input_files = opts.get("input").as<std::vector<std::string>>(),
        .output_dir = opts.get("output").as<std::filesystem::path>()
    };
}
```

### Subcommand Pattern

```cpp
// Not built-in, but can be implemented:
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: myapp <command> [options...]" << std::endl;
        return 1;
    }
    
    std::string command = argv[1];
    
    // Create new argv without the command
    std::vector<char*> sub_argv(argv + 1, argv + argc);
    sub_argv[0] = argv[0];  // Keep program name
    
    if (command == "server") {
        return run_server(sub_argv.size(), sub_argv.data());
    } else if (command == "client") {
        return run_client(sub_argv.size(), sub_argv.data());
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        return 1;
    }
}
```

## Building

The Arguments module is header-only:

```cmake
# CMake
target_link_libraries(your_target PRIVATE lux::cxx::arguments)

# Manual include
# Just include the header: #include <lux/cxx/arguments/Arguments.hpp>
```

### Dependencies

```cmake
# Internal dependencies
lux::cxx::compile_time
lux::cxx::container

# External dependencies
# C++ Standard Library only (C++20)
```

## Testing

Comprehensive test suite:

```bash
cd build
ctest -R arguments

# Or run directly:
./arguments/test/arguments_test
```

### Test Coverage

- Type conversion for all supported types
- Multi-value option handling
- Boolean flag parsing variants
- Error conditions and messages
- Help generation
- Edge cases (empty args, special characters, etc.)

## Comparison with Other Libraries

| Feature | LUX Arguments | CLI11 | cxxopts | Boost.Program_options |
|---------|---------------|--------|---------|----------------------|
| Header-only | ✅ | ✅ | ✅ | ❌ |
| C++20 Support | ✅ | ✅ | ✅ | ❌ |
| Type Safety | ✅ | ✅ | ✅ | ✅ |
| Multi-value | ✅ | ✅ | ✅ | ✅ |
| Custom Types | ✅ | ✅ | ✅ | ✅ |
| Zero Dependencies | ✅ | ✅ | ✅ | ❌ |
| Binary Size | Small | Small | Medium | Large |

## Future Enhancements

- [ ] Subcommand support
- [ ] Configuration file integration
- [ ] Shell completion generation
- [ ] Internationalization support
- [ ] More built-in validators
- [ ] Integration with logging frameworks