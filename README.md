# LUX-CXX

LUX-CXX is the core C++ library of the LUX Project, designed for applications requiring high performance and low latency. It is particularly suitable for real-time applications such as games, simulations, and other interactive applications. The library adopts modern C++20 standards and provides a complete set of infrastructure modules.

## Project Features

- **Modern C++**: Utilizes C++20/23 features, providing type-safe and high-performance interfaces
- **Modular Design**: Each module is designed independently and can be used as needed
- **Zero Dependencies**: Most modules depend only on the standard library, reducing external dependencies
- **Header-Only**: Most modules are provided as header-only libraries for easy integration
- **High Performance**: Optimized for performance-critical scenarios with memory pools, container optimizations, and more

## Core Modules

Each module is designed independently and includes comprehensive documentation. Click on the module links for detailed information.

### 🧮 [Algorithm](algorithm/README.md)
High-performance algorithm implementations including hash functions, topological sorting, and string operations.

**Key Features**: FNV-1a hashing (runtime & compile-time), string utilities, dependency resolution

### 🏗️ [Archtype](archtype/README.md)
High-performance Entity Component System (ECS) with archetype-based storage for optimal cache locality.

**Key Features**: O(1) operations, efficient queries, tightly packed memory layout, benchmarked against EnTT

### 📦 [Container](container/README.md)
Specialized container data structures optimized for specific use cases.

**Key Features**: SparseSet with O(1) operations, SmallVector with stack optimization, performance benchmarks

### ⚡ [Compile Time](compile_time/README.md)
Compile-time computation and metaprogramming tools for advanced template programming.

**Key Features**: Sequence sorting, dependency resolution, computation pipelines, zero runtime cost

### 📋 [Arguments](arguments/README.md)
Modern command-line argument parsing library with type safety and automatic help generation.

**Key Features**: Type-safe parsing, multi-value support, custom type extensions, descriptive errors

### 🔧 [Dynamic Reflection](dynamic_reflection/README.md)
Runtime reflection system based on Clang LibTooling for code analysis and metadata generation.

**Key Features**: Clang-based analysis, JSON metadata export, runtime type queries

### 🚀 [Subprogram](subprogram/README.md)
Subprogram registration and management system for building modular applications.

**Key Features**: Dynamic registration, runtime invocation, program discovery, plugin architecture support

### 🔄 [Concurrent](concurrent/README.md)
Concurrent programming infrastructure (under development).

### 🧠 [Memory](memory/README.md)
Memory management tools and optimizations (under development).

### 🔍 [Static Reflection](static_reflection/README.md)
Compile-time reflection system (under development).

## Build Requirements

- **Compiler**: Modern compiler with C++20 support (GCC 11+, Clang 13+, MSVC 2022+)
- **Build System**: CMake 3.22+
- **Platform**: Linux, macOS, Windows

## Building

```bash
# Clone the project
git clone https://github.com/LUX-YU/lux-cxx.git
cd lux-cxx

# Build
mkdir build
cd build
cmake ..
cmake --build .


## Performance Features

- **Zero-Cost Abstractions**: Modern C++ features provide zero runtime overhead
- **Memory Efficiency**: Optimized for cache-friendly data layouts
- **Compile-Time Optimization**: Extensive computations performed at compile time
- **SIMD Friendly**: Data structures designed with vectorization optimization in mind

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please check our contribution guidelines for detailed information.

---
