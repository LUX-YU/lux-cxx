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

### Core
Header-only language-level facilities: SBO `move_only_function`, borrowed
`function_ref`, `Delegate`, `scope_exit`, `FixedText`, `EnumFlags`, strong and
stable identifiers, schema identifiers, and checked constexpr arithmetic.
The existing lowercase `expected` API remains in `compile_time`: C++23 uses
`std::expected`, while C++20 uses the bundled external `tl::expected`.

### 🧮 [Algorithm](algorithm/README.md)
High-performance algorithm implementations including hash functions, topological sorting, and string operations.

**Key Features**: SHA-256 with incremental and constexpr paths, content IDs,
FNV-1a hashing, string utilities, dependency resolution

### Binary
Allocation-free span reader/writer and an explicit owning vector writer with
endianness policies, canonical varints and booleans, zigzag integers, bounded
strings/bytes, deterministic floating-point encoding, and frozen scalar schema IDs.

### Time and Units
Domain-typed timestamps, checked clock mappings, deterministic manual clocks,
and constexpr quantities for byte counts, frequency, and angles.

### Diagnostic and ABI
Fixed-capacity allocation-free diagnostic records plus canonical build metadata,
semantic versions, C-boundary views, and SHA-256 ABI fingerprints.

### 🏗️ [Archtype](archtype/README.md)
High-performance Entity Component System (ECS) with archetype-based storage for optimal cache locality.

**Key Features**: O(1) operations, efficient queries, tightly packed memory layout, benchmarked against EnTT

### 📦 [Container](container/README.md)
Specialized container data structures optimized for specific use cases.

**Key Features**: SparseSet with O(1) operations, allocator-aware SlotMap and
SmallVector, block-allocated StableSlotMap, performance benchmarks

### ⚡ [Compile Time](compile_time/README.md)
Compile-time computation and metaprogramming tools for advanced template programming.

**Key Features**: Sequence sorting, dependency resolution, computation pipelines, zero runtime cost

### 📋 [Arguments](arguments/README.md)
Modern command-line argument parsing library with type safety and automatic help generation.

**Key Features**: Type-safe parsing, multi-value support, custom type extensions, descriptive errors

### 🔧 [Reflection](reflection/README.md)
Reflection system based on Clang LibTooling for code analysis and metadata generation.

**Key Features**: Clang-based analysis, JSON metadata export, runtime type
queries, and an experimental flat/indexed `reflection::ir::MetaUnit` with a
bounded deterministic binary format

### 🚀 [Subprogram](subprogram/README.md)
Subprogram registration and management system for building modular applications.

**Key Features**: Dynamic registration, runtime invocation, program discovery, plugin architecture support

### 🔄 [Concurrent](concurrent/README.md)
Concrete blocking and SPSC queues with explicit result/state enums, close/drain,
timeout and stop-token support, plus allocation-free admission and budget tickets.

### 🧠 [Memory](memory/README.md)
Intrusive ownership and object pools, explicit `SharedBytes`, and PMR decorators
for allocation counting, byte budgets, and deterministic allocation failure.


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
ctest --output-on-failure
```

Installed consumers select only the components they need:

```cmake
find_package(lux-cxx CONFIG REQUIRED COMPONENTS core binary memory)
target_link_libraries(my_target PRIVATE lux::cxx::binary lux::cxx::memory)
```

The detailed API and downstream transition notes are in
[`doc/lux-cxx-improvement-migration.zh-CN.md`](doc/lux-cxx-improvement-migration.zh-CN.md).


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
