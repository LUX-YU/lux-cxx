# Static Reflection Module

Compile-time reflection system (under development).

## Overview

The Static Reflection module will provide compile-time reflection capabilities allowing you to inspect and manipulate types, members, and attributes at compile time.

## Planned Features

- **Type Inspection**: Query type properties at compile time
- **Member Enumeration**: Iterate over class/struct members
- **Attribute System**: Attach metadata to types and members
- **Serialization Support**: Automatic serialization code generation
- **Template Utilities**: Advanced template metaprogramming helpers

## Status

This module is currently under development. Check back for updates.

## Building

```cmake
target_link_libraries(your_target PRIVATE lux::cxx::static_reflection)
```

## Dependencies

- **C++ Standard**: C++20 or later (requires concepts and consteval)
- **Standard Library**: `<type_traits>`, `<utility>`
- **Internal**: `lux::cxx::compile_time`