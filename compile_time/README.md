# Compile Time Module

Compile-time computation and metaprogramming utilities for advanced template programming.

## Overview

The Compile Time module provides powerful compile-time computation tools that enable complex metaprogramming scenarios. These utilities allow you to perform computations during compilation, reducing runtime overhead and enabling sophisticated type manipulations.

## Features

- **Sequence Sorting**: Sort integer sequences at compile time
- **Dependency Resolution**: Compile-time topological sorting for dependency graphs
- **Computation Pipelines**: Build complex computation networks resolved at compile time
- **Type Manipulations**: Advanced template metaprogramming utilities
- **Zero Runtime Cost**: All computations performed during compilation

## Core Components

### Sequence Sorting

Compile-time sorting of `std::integer_sequence` using quicksort algorithm.

```cpp
#include <lux/cxx/compile_time/sequence_sort.hpp>

// Define an unsorted sequence
using unsorted = std::integer_sequence<int, 3, 1, 4, 1, 5, 9, 2, 6>;

// Sort at compile time
using sorted = lux::cxx::quick_sort_sequence<unsorted>::type;
// Result: std::integer_sequence<int, 1, 1, 2, 3, 4, 5, 6, 9>

// Use in template parameters
template<int... Values>
void process_sorted_values() {
    ((std::cout << Values << " "), ...);
    std::cout << std::endl;
}

// Expand sorted sequence
std::apply([](auto... values) {
    process_sorted_values<values...>();
}, sorted{});
```

**Performance**: O(n log n) compile-time complexity, zero runtime cost.

### Node Dependency Sorting

Compile-time topological sorting for dependency graphs, useful for initialization order and build systems.

```cpp
#include <lux/cxx/compile_time/computer_node_sort.hpp>

// Define nodes with dependencies
struct DatabaseNode {};
struct ConfigNode {};
struct LoggingNode {};
struct ServerNode {};

// Define dependency relationships
using dependencies = std::tuple<
    dependency_map<ConfigNode, 0, DatabaseNode, 0>,      // Database depends on Config
    dependency_map<ConfigNode, 0, LoggingNode, 0>,       // Logging depends on Config  
    dependency_map<DatabaseNode, 0, ServerNode, 0>,      // Server depends on Database
    dependency_map<LoggingNode, 0, ServerNode, 0>        // Server depends on Logging
>;

using nodes = std::tuple<ConfigNode, DatabaseNode, LoggingNode, ServerNode>;

// Perform topological sort at compile time
using sorted_nodes = lux::cxx::topological_sort<nodes, dependencies>::type;
// Result: std::tuple<ConfigNode, DatabaseNode, LoggingNode, ServerNode>

// Or layered sort for parallel execution
using layered_nodes = lux::cxx::layered_topological_sort<nodes, dependencies>::type;
// Result: std::tuple<
//     std::tuple<ConfigNode>,                    // Layer 0: No dependencies
//     std::tuple<DatabaseNode, LoggingNode>,     // Layer 1: Depend on layer 0
//     std::tuple<ServerNode>                     // Layer 2: Depend on layer 1
// >
```

### Computation Pipeline

Build complex computation pipelines that are resolved at compile time.

```cpp
#include <lux/cxx/compile_time/computer_pipeline.hpp>

// Define computation nodes
template<typename Input>
struct AddOneNode {
    using input_type = Input;
    using output_type = std::integral_constant<int, Input::value + 1>;
    static constexpr int input_location = 0;
    static constexpr int output_location = 0;
};

template<typename Input>
struct MultiplyTwoNode {
    using input_type = Input;
    using output_type = std::integral_constant<int, Input::value * 2>;
    static constexpr int input_location = 0;
    static constexpr int output_location = 0;
};

// Define pipeline: input -> AddOne -> MultiplyTwo -> output
using input_value = std::integral_constant<int, 5>;

using pipeline = lux::cxx::computation_pipeline<
    input_value,
    std::tuple<AddOneNode<input_value>, MultiplyTwoNode</* connected automatically */>>,
    std::tuple</* dependency mappings */>
>;

// Result computed at compile time: ((5 + 1) * 2) = 12
constexpr int result = pipeline::result::value;
```

## Advanced Features

### Type Filtering and Manipulation

```cpp
#include <lux/cxx/compile_time/computer_node_sort.hpp>

// Check if type is in tuple
using types = std::tuple<int, float, double, std::string>;
constexpr bool has_int = lux::cxx::in_tuple_v<int, types>;        // true
constexpr bool has_char = lux::cxx::in_tuple_v<char, types>;      // false

// Filter tuple based on predicate
template<typename T>
struct is_arithmetic : std::is_arithmetic<T> {};

using numeric_types = lux::cxx::filter_tuple<types, is_arithmetic>::type;
// Result: std::tuple<int, float, double>
```

### Compile-Time Sorting Utilities

```cpp
// Prepend value to sequence
using seq = std::integer_sequence<int, 2, 3, 4>;
using prepended = lux::cxx::sort_prepend<seq, int, 1>::type;
// Result: std::integer_sequence<int, 1, 2, 3, 4>

// Filter sequences
using numbers = std::integer_sequence<int, 1, 5, 3, 8, 2, 7>;
using less_than_5 = lux::cxx::sort_filter_less_helper<numbers, int, 5>::type;
// Result: std::integer_sequence<int, 1, 3, 2>

using greater_equal_5 = lux::cxx::sort_filter_greater_equal_helper<numbers, int, 5>::type;  
// Result: std::integer_sequence<int, 5, 8, 7>

// Concatenate sequences
using seq1 = std::integer_sequence<int, 1, 2>;
using seq2 = std::integer_sequence<int, 3, 4>;
using combined = lux::cxx::concat_sequences<seq1, seq2>::type;
// Result: std::integer_sequence<int, 1, 2, 3, 4>
```

## Use Cases

### Initialization Order Resolution

```cpp
// Define subsystems with dependencies
struct FileSystemInit { /* ... */ };
struct NetworkInit { /* ... */ };  
struct DatabaseInit { /* ... */ };
struct GameEngineInit { /* ... */ };

// Define initialization dependencies
using init_deps = std::tuple<
    dependency_map<FileSystemInit, 0, DatabaseInit, 0>,
    dependency_map<NetworkInit, 0, DatabaseInit, 0>,
    dependency_map<DatabaseInit, 0, GameEngineInit, 0>
>;

using init_order = lux::cxx::topological_sort<
    std::tuple<FileSystemInit, NetworkInit, DatabaseInit, GameEngineInit>,
    init_deps
>::type;

// Generate initialization code
template<typename InitTuple>
void initialize_systems() {
    std::apply([](auto... init_types) {
        (init_types.initialize(), ...);
    }, InitTuple{});
}

// Usage
initialize_systems<init_order>();
```

### Build System Dependencies

```cpp
// Define build targets
struct SourceFiles {};
struct ObjectFiles {};
struct StaticLibrary {};
struct Executable {};

// Define build dependencies  
using build_deps = std::tuple<
    dependency_map<SourceFiles, 0, ObjectFiles, 0>,
    dependency_map<ObjectFiles, 0, StaticLibrary, 0>,
    dependency_map<StaticLibrary, 0, Executable, 0>
>;

// Get build order
using build_order = lux::cxx::topological_sort<
    std::tuple<SourceFiles, ObjectFiles, StaticLibrary, Executable>,
    build_deps
>::type;

// Or parallel build layers
using build_layers = lux::cxx::layered_topological_sort<
    std::tuple<SourceFiles, ObjectFiles, StaticLibrary, Executable>,
    build_deps
>::type;
```

### Compile-Time Configuration

```cpp
// Sort configuration priorities at compile time
using config_priorities = std::integer_sequence<int, 100, 50, 200, 10, 300>;
using sorted_priorities = lux::cxx::quick_sort_sequence<config_priorities>::type;

template<int Priority>
struct ConfigOption {
    static constexpr int priority = Priority;
    // Configuration data...
};

// Apply configurations in priority order
template<int... Priorities>
void apply_configurations(std::integer_sequence<int, Priorities...>) {
    (ConfigOption<Priorities>::apply(), ...);
}

// Usage
apply_configurations(sorted_priorities{});
```

## Implementation Details

### Sorting Algorithm

The quicksort implementation uses template specialization for different sequence sizes:

```cpp
// Base cases
template<typename T>
struct quick_sort_sequence<std::integer_sequence<T>> {
    using type = std::integer_sequence<T>;  // Empty sequence
};

template<typename T, T x>
struct quick_sort_sequence<std::integer_sequence<T, x>> {
    using type = std::integer_sequence<T, x>;  // Single element
};

// Recursive case
template<typename T, T pivot, T next, T... Rest>
struct quick_sort_sequence<std::integer_sequence<T, pivot, next, Rest...>> {
    // Partition around pivot
    using left_seq = typename sort_filter_less_helper<...>::type;
    using right_seq = typename sort_filter_greater_equal_helper<...>::type;
    
    // Recursively sort partitions
    using sorted_left = typename quick_sort_sequence<left_seq>::type;
    using sorted_right = typename quick_sort_sequence<right_seq>::type;
    
    // Combine results
    using type = typename concat_sequences<
        sorted_left, 
        std::integer_sequence<T, pivot>,
        sorted_right
    >::type;
};
```

### Dependency Resolution

The topological sort uses SFINAE and template recursion:

```cpp
template<typename Sorted, typename Remaining, typename AllDeps>
struct topo_sort_impl {
    // Find nodes with satisfied dependencies
    using satisfied = find_satisfied<Remaining, Sorted, AllDeps>;
    
    // Remove satisfied node from remaining
    using new_remaining = remove_from_tuple<satisfied, Remaining>;
    
    // Add to sorted list
    using new_sorted = append_to_tuple<satisfied, Sorted>;
    
    // Recurse
    using type = typename topo_sort_impl<new_sorted, new_remaining, AllDeps>::type;
};
```

## Compile-Time Performance

### Complexity Analysis

| Algorithm | Time Complexity | Space Complexity |
|-----------|----------------|-----------------|
| Sequence Sort | O(n log n) | O(n) |
| Topological Sort | O(V + E) | O(V) |
| Type Filtering | O(n) | O(n) |

### Compilation Impact

- **Template Instantiation Depth**: Moderate (proportional to input size)
- **Compile Time**: Linear to polynomial depending on algorithm
- **Memory Usage**: Template instantiation overhead
- **Binary Size**: Zero runtime code generated

### Compiler Limits

Different compilers have different template instantiation limits:

```cpp
// Typical limits
constexpr size_t MAX_SEQUENCE_SIZE = 1000;    // GCC/Clang
constexpr size_t MAX_DEPENDENCY_DEPTH = 500;  // MSVC may be lower
```

## Error Handling

Compile-time errors provide diagnostic information:

```cpp
// Circular dependency detection
using circular_deps = std::tuple<
    dependency_map<A, 0, B, 0>,
    dependency_map<B, 0, A, 0>  // Circular!
>;

// Will produce compile error with diagnostic
using result = lux::cxx::topological_sort<std::tuple<A, B>, circular_deps>::type;
// Error: Circular dependency detected between A and B
```

## Testing

The module includes comprehensive compile-time tests:

```cpp
// Compile-time assertions
static_assert(std::is_same_v<
    lux::cxx::quick_sort_sequence<std::integer_sequence<int, 3, 1, 2>>::type,
    std::integer_sequence<int, 1, 2, 3>
>);

static_assert(lux::cxx::in_tuple_v<int, std::tuple<int, float, double>>);
```

## Integration with Other Modules

### With Container Module

```cpp
// Generate container sizes at compile time
using sizes = std::integer_sequence<size_t, 100, 50, 200, 10>;
using sorted_sizes = lux::cxx::quick_sort_sequence<sizes>::type;

template<size_t... Sizes>
auto create_containers(std::integer_sequence<size_t, Sizes...>) {
    return std::make_tuple(lux::cxx::SmallVector<int, Sizes>{}...);
}

auto containers = create_containers(sorted_sizes{});
```

### With Archtype Module

```cpp
// Sort component type IDs at compile time for efficient storage
template<typename... Components>
struct ComponentStorage {
    using component_ids = std::integer_sequence<size_t, 
        TypeRegistry::getTypeID<Components>()...>;
    
    using sorted_ids = lux::cxx::quick_sort_sequence<component_ids>::type;
    
    // Use sorted IDs for efficient access patterns
};
```

## Examples

### Complete Dependency Resolution Example

```cpp
#include <lux/cxx/compile_time/computer_node_sort.hpp>
#include <iostream>
#include <string>

// Define system components
struct Logger { 
    static void initialize() { std::cout << "Logger initialized\n"; }
};

struct Config { 
    static void initialize() { std::cout << "Config initialized\n"; }
};

struct Database { 
    static void initialize() { std::cout << "Database initialized\n"; }
};

struct Server { 
    static void initialize() { std::cout << "Server initialized\n"; }
};

// Define dependencies: Logger <- Config <- Database <- Server  
using deps = std::tuple<
    lux::cxx::dependency_map<Logger, 0, Config, 0>,
    lux::cxx::dependency_map<Config, 0, Database, 0>,
    lux::cxx::dependency_map<Database, 0, Server, 0>
>;

using all_systems = std::tuple<Server, Database, Config, Logger>;

// Sort dependencies at compile time
using initialization_order = lux::cxx::topological_sort<all_systems, deps>::type;

template<typename Systems>
void initialize_all();

template<typename... Systems>
void initialize_all<std::tuple<Systems...>>() {
    (Systems::initialize(), ...);
}

int main() {
    std::cout << "Initializing systems in dependency order:\n";
    initialize_all<initialization_order>();
    return 0;
}

// Output:
// Initializing systems in dependency order:
// Logger initialized
// Config initialized  
// Database initialized
// Server initialized
```

### Complete Sequence Processing Example

```cpp
#include <lux/cxx/compile_time/sequence_sort.hpp>
#include <iostream>
#include <array>

// Process data in sorted order at compile time
template<int... Values>
constexpr auto create_sorted_array() {
    using unsorted = std::integer_sequence<int, Values...>;
    using sorted = lux::cxx::quick_sort_sequence<unsorted>::type;
    
    return []<int... SortedValues>(std::integer_sequence<int, SortedValues...>) {
        return std::array<int, sizeof...(SortedValues)>{SortedValues...};
    }(sorted{});
}

int main() {
    // Create sorted array at compile time
    constexpr auto sorted_data = create_sorted_array<5, 2, 8, 1, 9, 3>();
    
    std::cout << "Sorted data: ";
    for (int value : sorted_data) {
        std::cout << value << " ";
    }
    std::cout << std::endl;
    
    return 0;
}

// Output: Sorted data: 1 2 3 5 8 9
```

## Building

The Compile Time module is header-only:

```cmake
target_link_libraries(your_target PRIVATE lux::cxx::compile_time)
```

## Dependencies

- **C++ Standard**: C++20 or later (requires concepts and template improvements)
- **Standard Library**: `<type_traits>`, `<utility>`, `<tuple>`
- **Internal**: None

## Future Enhancements

- [ ] Compile-time string sorting and manipulation
- [ ] Graph algorithms (shortest path, minimum spanning tree)
- [ ] Compile-time regular expressions
- [ ] Mathematical computations (matrix operations, etc.)
- [ ] Code generation utilities
- [ ] Template debugging utilities