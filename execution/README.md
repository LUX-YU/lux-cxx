# Execution

`lux::cxx::execution::ManualContext` provides a stdexec scheduler backed by
concurrentqueue. Producers submit work through the scheduler and one consumer
calls `run_ready(budget)` to deliver completions. The budget is a node count.
See the [concurrency and lifetime contract](docs/CONTRACT.zh-CN.md) before
sharing a scheduler across threads or closing a context.

## Build and test

Enable the optional component with `LUX_CXX_ENABLE_EXECUTION=ON`. It requires
CMake 3.24+, C++20, and the `STDEXEC::stdexec` and
`concurrentqueue::concurrentqueue` CMake targets. Use the project toolchain or
`CMAKE_PREFIX_PATH` to locate the dependency packages. The versions used for
development are listed in [Third-party notices](THIRD_PARTY_NOTICES.md).
`LUX_EXECUTION_FETCH_DEPS=ON` can fetch the pinned headers for source builds.

```sh
cmake -S . -B build/execution -DCMAKE_BUILD_TYPE=Release -DLUX_CXX_ENABLE_EXECUTION=ON -DLUX_CXX_BUILD_TESTS=ON -DLUX_EXECUTION_BUILD_EXAMPLES=ON -DLUX_EXECUTION_BUILD_BENCHMARKS=ON
cmake --build build/execution --config Release
ctest --test-dir build/execution -C Release -R "^execution[.]" --output-on-failure
```

The benchmark accepts the producer count, operations per producer, and pump
budget. This example runs a single-config build from a Unix-like shell.

```sh
./build/execution/execution/lux_execution_benchmark 4 250000 128
```

It measures connect, start, and pump throughput with explicit producer tokens
and preallocated operation storage. The timed path excludes stdexec spawning,
I/O, and OS wakeups.

## Use an installed component

Make the stdexec and concurrentqueue CMake packages available to the consumer.

```cmake
find_package(lux-cxx CONFIG REQUIRED COMPONENTS execution)
target_link_libraries(my_target PRIVATE lux::cxx::execution)
```
