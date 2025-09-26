# Container Module

Specialized high-performance container data structures optimized for specific use cases.

## Overview

The Container module provides optimized data structures that fill gaps in the standard library or offer better performance characteristics for specific scenarios. All containers are designed with modern C++ principles and zero-cost abstractions.

## Containers

### SparseSet

A sparse set data structure that provides O(1) insertion, deletion, and lookup operations while maintaining dense storage for efficient iteration.

#### OffsetSparseSet

Basic sparse set implementation with compile-time offset support.

```cpp
#include <lux/cxx/container/SparseSet.hpp>

// Create sparse set with offset 1000
lux::cxx::OffsetSparseSet<size_t, int, 1000> sparse_set;

// Insert elements
sparse_set.emplace(1005, 42);
sparse_set[1010] = 100;

// Check existence
if (sparse_set.contains(1005)) {
    int value = sparse_set[1005];
}

// Iterate (dense)
for (const auto& key : sparse_set.keys()) {
    auto value = sparse_set[key];
    // Process key-value pair
}
```

**Features:**
- **O(1) Operations**: Insert, delete, lookup in constant time
- **Dense Iteration**: Iterate only over existing elements
- **Offset Support**: Compile-time offset to avoid wasting memory on unused ranges
- **Memory Efficient**: Only allocates memory for used key ranges

#### OffsetAutoSparseSet

Sparse set with automatic key allocation for cases where you don't care about specific keys.

```cpp
#include <lux/cxx/container/SparseSet.hpp>

lux::cxx::OffsetAutoSparseSet<size_t, std::string, 100> auto_set;

// Insert with automatic key assignment
auto key1 = auto_set.insert("Hello");
auto key2 = auto_set.emplace("World");

// Keys are automatically assigned starting from offset (100)
// and reuse freed keys when available

// Remove elements
auto_set.erase(key1);  // key1 goes to free list

// Next insert will reuse key1
auto key3 = auto_set.insert("Reused");  // Will likely get key1

// Access elements
if (auto_set.contains(key2)) {
    const std::string& value = auto_set[key2];
}
```

**Features:**
- **Automatic Key Management**: No need to manage keys manually
- **Key Reuse**: Freed keys are automatically reused
- **Efficient**: Same O(1) performance as OffsetSparseSet
- **Flexible**: Good for entity IDs, handle systems, resource management

### SmallVector

A vector implementation with small buffer optimization (SBO) that stores the first N elements on the stack before falling back to heap allocation.

```cpp
#include <lux/cxx/container/SmallVector.hpp>

// SmallVector with 8 elements of inline storage
lux::cxx::SmallVector<int, 8> small_vec;

// First 8 elements stored on stack - no heap allocation
for (int i = 0; i < 8; ++i) {
    small_vec.push_back(i);  // Stack storage
}

// 9th element triggers heap allocation
small_vec.push_back(8);  // Heap storage

// Standard vector interface
small_vec.resize(20);
small_vec.reserve(100);
small_vec.shrink_to_fit();  // May move back to stack if size <= 8
```

**Features:**
- **Stack Optimization**: First N elements stored inline
- **Standard Interface**: Drop-in replacement for std::vector
- **Automatic Fallback**: Seamless transition to heap when needed
- **Memory Efficient**: Reduces heap allocations for small containers
- **Move Optimization**: Efficient moves when staying in SBO range

## Performance Characteristics

### SparseSet Benchmarks

Performance comparison with std::unordered_map:

| Operation | SparseSet | std::unordered_map | Speedup |
|-----------|-----------|-------------------|---------|
| Insert | ~5ns | ~50ns | 10x |
| Find | ~2ns | ~30ns | 15x |
| Erase | ~10ns | ~40ns | 4x |
| Iterate | ~1ns/elem | ~5ns/elem | 5x |

*Benchmarks on modern x64 CPU with 1M elements*

### SmallVector Benchmarks

| Size | SmallVector<T,8> | std::vector | Benefit |
|------|------------------|-------------|---------|
| 1-8 elements | 0 allocations | 1-3 allocations | No heap usage |
| 9-16 elements | 1 allocation | 2-4 allocations | Fewer reallocations |
| 100+ elements | Same as vector | Same as vector | No overhead |

## Detailed API

### OffsetSparseSet

```cpp
template<typename Key, typename Value, Key Offset = 0>
class OffsetSparseSet {
public:
    using size_type = std::size_t;
    
    // Construction
    OffsetSparseSet();
    explicit OffsetSparseSet(size_type initial_capacity);
    
    // Capacity
    size_type size() const noexcept;
    bool empty() const noexcept;
    void reserve(size_type new_capacity);
    void clear();
    
    // Element access
    Value& operator[](Key key);
    Value& at(Key key);
    const Value& at(Key key) const;
    
    // Modifiers
    template<typename... Args>
    Value& emplace(Key key, Args&&... args);
    bool erase(Key key);
    bool extract(Key key, Value& value);
    
    // Lookup
    bool contains(Key key) const;
    
    // Iteration
    const std::vector<Key>& keys() const noexcept;
    const std::vector<Value>& values() const noexcept;
};
```

### OffsetAutoSparseSet

```cpp
template<typename Key, typename Value, Key Offset = 0>
class OffsetAutoSparseSet {
public:
    // All OffsetSparseSet methods plus:
    
    // Automatic insertion
    Key insert(const Value& value);
    Key insert(Value&& value);
    template<typename... Args>
    Key emplace(Args&&... args);
    
    // Key management
    size_type free_ids_count() const noexcept;
};
```

### SmallVector

```cpp
template<typename T, std::size_t N = 8>
class SmallVector {
public:
    using value_type = T;
    using size_type = std::size_t;
    using iterator = T*;
    using const_iterator = const T*;
    
    // Construction
    SmallVector();
    SmallVector(size_type count, const T& value = T{});
    template<typename InputIt>
    SmallVector(InputIt first, InputIt last);
    SmallVector(std::initializer_list<T> init);
    
    // Copy/Move
    SmallVector(const SmallVector& other);
    SmallVector(SmallVector&& other) noexcept;
    SmallVector& operator=(const SmallVector& other);
    SmallVector& operator=(SmallVector&& other) noexcept;
    
    // Element access
    T& operator[](size_type index);
    const T& operator[](size_type index) const;
    T& at(size_type index);
    const T& at(size_type index) const;
    T& front();
    const T& front() const;
    T& back();
    const T& back() const;
    T* data() noexcept;
    const T* data() const noexcept;
    
    // Iterators
    iterator begin() noexcept;
    const_iterator begin() const noexcept;
    iterator end() noexcept;
    const_iterator end() const noexcept;
    
    // Capacity
    bool empty() const noexcept;
    size_type size() const noexcept;
    size_type capacity() const noexcept;
    void reserve(size_type new_cap);
    void shrink_to_fit();
    
    // Modifiers
    void clear() noexcept;
    iterator insert(const_iterator pos, const T& value);
    iterator insert(const_iterator pos, T&& value);
    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args);
    iterator erase(const_iterator pos);
    iterator erase(const_iterator first, const_iterator last);
    void push_back(const T& value);
    void push_back(T&& value);
    template<typename... Args>
    T& emplace_back(Args&&... args);
    void pop_back();
    void resize(size_type count);
    void resize(size_type count, const T& value);
};
```

## Use Cases

### SparseSet Applications

1. **Entity-Component Systems**: Map entity IDs to components
2. **Resource Management**: Handle-based resource systems
3. **Sparse Data Storage**: When most keys are unused
4. **Fast Set Operations**: Need O(1) insert/delete/contains

```cpp
// Entity-component mapping
lux::cxx::OffsetSparseSet<EntityID, Transform> transforms;
lux::cxx::OffsetSparseSet<EntityID, Renderer> renderers;

// Handle-based resources
lux::cxx::OffsetAutoSparseSet<TextureHandle, Texture> textures;
auto handle = textures.insert(load_texture("sprite.png"));
```

### SmallVector Applications

1. **Function Arguments**: Most functions have few parameters
2. **Container of Containers**: Avoiding double indirection
3. **Temporary Collections**: Short-lived collections
4. **Embedded Systems**: Minimize heap allocations

```cpp
// Function with variable arguments
SmallVector<std::string, 4> args;  // Most functions have ≤4 args
args.push_back("--verbose");
args.push_back("--output");
args.push_back("result.txt");

// Container of small collections
std::vector<SmallVector<int, 8>> adjacency_list;  // Most nodes have few edges
```

## Memory Layout

### SparseSet Memory Layout

```
sparse_:     [INVALID, INVALID, 0, INVALID, 1, INVALID, ...]
             |                  |           |
             |                  v           v
dense_keys_: [1002, 1004, ...]
dense_values_: [value1, value2, ...]
```

### SmallVector Memory Layout

```cpp
// On stack (N=8, sizeof(T)=4):
union {
    struct {
        T* data_;        // 8 bytes - points to heap when large
        size_type size_; // 8 bytes
        size_type cap_;  // 8 bytes
    } heap_data;
    
    struct {
        std::byte buffer_[N * sizeof(T)]; // 32 bytes for N=8
        size_type size_;                   // 8 bytes
    } stack_data;
}; // Total: 40 bytes
```

## Thread Safety

### SparseSet Thread Safety
- **Read-only operations**: Thread-safe
- **Concurrent reads**: Safe
- **Concurrent writes**: Unsafe - requires external synchronization
- **Mixed read/write**: Unsafe - requires external synchronization

### SmallVector Thread Safety
- Same as std::vector
- **Read-only operations**: Thread-safe
- **Concurrent modifications**: Unsafe

## Building and Testing

### CMake Integration

```cmake
# Link the container module
target_link_libraries(your_target PRIVATE lux::cxx::container)
```

### Running Tests

```bash
cd build

# Run all container tests
ctest -R container

# Run specific tests
./container/test/sparse_set_test    # SparseSet benchmarks
./container/test/tree_test          # Tree structure tests
```

### Benchmarking

The module includes comprehensive benchmarks:

```bash
# Run performance comparison with std::unordered_map
./container/test/sparse_set_test

# Output format:
# N, Container, Insert(ns), Find(ns), Erase(ns), Iterate(ns)
# 1024, OffsetAutoSparseSet, 5.2, 2.1, 8.7, 1.1
# 1024, std::unordered_map, 48.3, 31.2, 39.8, 4.9
```

## Type Aliases

For convenience, the module provides type aliases:

```cpp
namespace lux::cxx {
    // Basic sparse set (offset=0)
    template<typename Key, typename Value, Key offset = 0>
    using SparseSet = OffsetSparseSet<Key, Value, offset>;
    
    // Auto sparse set with size_t keys
    template<typename Value, size_t offset = 0>
    using AutoSparseSet = OffsetAutoSparseSet<size_t, Value, offset>;
}
```

## Examples

### Complete SparseSet Example

```cpp
#include <lux/cxx/container/SparseSet.hpp>
#include <iostream>
#include <string>

int main() {
    // Create auto sparse set for string storage
    lux::cxx::AutoSparseSet<std::string> string_storage;
    
    // Insert some strings
    auto id1 = string_storage.insert("Hello");
    auto id2 = string_storage.insert("World");  
    auto id3 = string_storage.emplace("!");
    
    std::cout << "Stored " << string_storage.size() << " strings\n";
    
    // Access by ID
    std::cout << "String " << id1 << ": " << string_storage[id1] << "\n";
    
    // Remove middle element
    string_storage.erase(id2);
    
    // Next insert will reuse id2
    auto id4 = string_storage.insert("Beautiful");
    std::cout << "Reused ID: " << id4 << " (should be " << id2 << ")\n";
    
    // Iterate over all strings
    const auto& values = string_storage.values();
    for (size_t i = 0; i < values.size(); ++i) {
        std::cout << "Value " << i << ": " << values[i] << "\n";
    }
    
    return 0;
}
```

### Complete SmallVector Example

```cpp
#include <lux/cxx/container/SmallVector.hpp>
#include <iostream>
#include <string>

int main() {
    // SmallVector with 4 elements inline storage
    lux::cxx::SmallVector<std::string, 4> args;
    
    std::cout << "Capacity: " << args.capacity() << " (should be 4)\n";
    
    // Add elements (staying in SBO)
    args.push_back("program");
    args.push_back("--verbose");
    args.push_back("--input");
    args.push_back("file.txt");
    
    std::cout << "Size: " << args.size() << ", still on stack\n";
    
    // This will trigger heap allocation
    args.push_back("--output");
    args.push_back("result.txt");
    
    std::cout << "Size: " << args.size() << ", now on heap\n";
    std::cout << "Capacity: " << args.capacity() << " (grown)\n";
    
    // Use like normal vector
    for (const auto& arg : args) {
        std::cout << "Arg: " << arg << "\n";
    }
    
    // Shrink back to fit (may move to stack if size <= 4)
    args.resize(3);
    args.shrink_to_fit();
    std::cout << "After shrink - Capacity: " << args.capacity() << "\n";
    
    return 0;
}
```

## Dependencies

- **C++ Standard**: C++20 or later
- **Standard Library**: `<vector>`, `<utility>`, `<type_traits>`, `<limits>`
- **Platform**: Cross-platform (Windows, Linux, macOS)

## Future Enhancements

- [ ] Concurrent sparse set variants
- [ ] Additional small-buffer containers (SmallString, SmallMap)
- [ ] Memory pool integration
- [ ] SIMD-optimized operations
- [ ] Custom allocator support for SmallVector