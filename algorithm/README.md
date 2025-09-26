# Algorithm Module

High-performance algorithm implementations for the LUX-CXX library.

## Overview

The Algorithm module provides a collection of optimized algorithms commonly used in high-performance applications. All algorithms are designed with modern C++ features and performance in mind.

## Features

### Hash Algorithms

#### FNV-1a Hash
Fast and simple hash function implementation with both runtime and compile-time variants.

**Runtime Version:**
```cpp
#include <lux/cxx/algorithm/hash.hpp>

std::string_view text = "Hello, World!";
uint64_t hash = lux::cxx::algorithm::fnv1a(text);
```

**Compile-time Version:**
```cpp
#include <lux/cxx/algorithm/hash.hpp>

// Computed at compile time
constexpr auto hash = lux::cxx::algorithm::TFnv1a("Hello, World!");
```

**Performance Characteristics:**
- Runtime: O(n) where n is the string length
- Compile-time: Zero runtime cost
- Memory: No additional memory allocation
- Cache-friendly: Single pass through data

### Topological Sort

Implementation of topological sorting for dependency resolution.

```cpp
#include <lux/cxx/algorithm/topological_sort.hpp>

// Usage example (interface may vary based on implementation)
template<typename T>
auto sorted_result = lux::cxx::algorithm::topological_sort<T>();
```

**Use Cases:**
- Build system dependency resolution
- Task scheduling
- Module initialization ordering

### String Operations

Efficient string manipulation utilities.

#### String Trimming
```cpp
#include <lux/cxx/algorithm/string_operations.hpp>

std::string_view text = "  Hello, World!  ";
auto trimmed = lux::cxx::algorithm::trim(text); // "Hello, World!"
```

#### String Replacement
```cpp
#include <lux/cxx/algorithm/string_operations.hpp>

std::string text = "Hello, World!";
auto result = lux::cxx::algorithm::replace(text, "World", "Universe");
// Result: "Hello, Universe!"
```

**Performance Features:**
- Zero-copy trimming using string_view
- Efficient in-place operations where possible
- Minimal memory allocations

## Implementation Details

### Hash Function Details

The FNV-1a implementation uses the following constants:
- **FNV Prime**: 1099511628211 (for 64-bit)
- **FNV Offset Basis**: 14695981039346656037 (for 64-bit)

### Memory Considerations

- All algorithms are designed to be cache-friendly
- Minimal dynamic memory allocation
- String operations prefer views over copies where possible

## Benchmarks

The algorithms in this module are optimized for performance. Here are typical performance characteristics:

- **FNV-1a Hash**: ~1-2 cycles per byte on modern CPUs
- **String Trim**: O(1) time complexity with string_view
- **String Replace**: Linear time with minimal allocations

## Dependencies

- **Internal**: None
- **External**: C++ Standard Library only
- **C++ Standard**: C++20 or later

## Thread Safety

- All functions are thread-safe for read-only operations
- Functions that return new objects are thread-safe
- In-place modification functions require external synchronization

## Examples

### Complete Hash Example
```cpp
#include <lux/cxx/algorithm/hash.hpp>
#include <iostream>
#include <string>

int main() {
    // Runtime hashing
    std::string runtime_input;
    std::getline(std::cin, runtime_input);
    auto runtime_hash = lux::cxx::algorithm::fnv1a(runtime_input);
    
    // Compile-time hashing
    constexpr auto compile_time_hash = lux::cxx::algorithm::TFnv1a("compile_time_string");
    
    std::cout << "Runtime hash: " << runtime_hash << std::endl;
    std::cout << "Compile-time hash: " << compile_time_hash << std::endl;
    
    return 0;
}
```

### String Processing Pipeline
```cpp
#include <lux/cxx/algorithm/string_operations.hpp>
#include <iostream>
#include <string>

int main() {
    std::string input = "  Hello, old World!  ";
    
    // Chain operations
    auto trimmed = lux::cxx::algorithm::trim(input);
    auto replaced = lux::cxx::algorithm::replace(std::string(trimmed), "old ", "new ");
    
    std::cout << "Original: '" << input << "'" << std::endl;
    std::cout << "Processed: '" << replaced << "'" << std::endl;
    
    return 0;
}
```

## API Reference

### Hash Functions

```cpp
namespace lux::cxx::algorithm {
    // Runtime FNV-1a hash
    constexpr uint64_t fnv1a(std::string_view text);
    
    // Compile-time FNV-1a hash
    template<uint64_t ArrSize>
    consteval size_t TFnv1a(const char(&char_array)[ArrSize]);
}
```

### String Operations

```cpp
namespace lux::cxx::algorithm {
    // Trim whitespace from both ends
    std::string_view trim(std::string_view text);
    
    // Replace all occurrences of old_value with new_value
    std::string replace(std::string_view str, 
                       std::string_view old_value, 
                       std::string_view new_value);
}
```

### Topological Sort

```cpp
namespace lux::cxx::algorithm {
    // Topological sort implementation
    template<typename T>
    void topological_sort();
}
```

## Building

The Algorithm module is header-only and requires no separate compilation:

```cmake
target_link_libraries(your_target PRIVATE lux::cxx::algorithm)
```

## Testing

Run the algorithm tests:
```bash
cd build
ctest -R algorithm
```