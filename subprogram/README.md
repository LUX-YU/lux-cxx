# Subprogram Module

Subprogram registration and management system for building modular applications.

## Overview

The Subprogram module provides a registry system for registering and dynamically invoking functions by name. This is useful for building applications with plugin architectures, command dispatching, or modular tool suites.

## Features

- **Dynamic Registration**: Register functions with string names
- **Runtime Invocation**: Call registered functions by name
- **Program Discovery**: List and query available subprograms
- **Type Safety**: Function signature validation
- **Sorted Access**: Alphabetically sorted program lists

## Quick Start

```cpp
#include <lux/cxx/subprogram/subprogram.hpp>
#include <iostream>

// Define subprogram functions
int hello_world(int argc, char* argv[]) {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}

int greet_user(int argc, char* argv[]) {
    if (argc > 1) {
        std::cout << "Hello, " << argv[1] << "!" << std::endl;
    } else {
        std::cout << "Hello, User!" << std::endl;
    }
    return 0;
}

int main(int argc, char* argv[]) {
    using namespace lux::cxx;
    
    // Register subprograms
    SubProgramRegister::registProgram("hello", hello_world);
    SubProgramRegister::registProgram("greet", greet_user);
    
    if (argc < 2) {
        std::cout << "Available commands:" << std::endl;
        auto programs = SubProgramRegister::listSortedSubPrograms();
        for (const auto& name : programs) {
            std::cout << "  " << name << std::endl;
        }
        return 1;
    }
    
    std::string command = argv[1];
    
    if (SubProgramRegister::hasSubProgram(command)) {
        // Adjust arguments for subprogram
        char** sub_argv = argv + 1;
        int sub_argc = argc - 1;
        
        return SubProgramRegister::runSubProgram(command, sub_argc, sub_argv);
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        return 1;
    }
}
```

## Detailed API

### SubProgramRegister Class

Central registry for managing subprograms.

```cpp
namespace lux::cxx {
    using SubProgramFunc = std::function<int(int, char**)>;
    
    class SubProgramRegister {
    public:
        // Register a subprogram with a name
        static void registProgram(const std::string& name, SubProgramFunc func);
        
        // Check if a subprogram exists
        static bool hasSubProgram(const std::string& name);
        
        // Run a registered subprogram
        static int runSubProgram(const std::string& name, int argc, char* argv[]);
        
        // List all registered program names
        static std::vector<std::string> listSubPrograms();
        
        // List program names in sorted order
        static std::vector<std::string> listSortedSubPrograms();
        
        // Get number of registered programs
        static size_t getSubProgramCount();
        
        // Clear all registered programs
        static void clearAllSubPrograms();
    };
}
```

## Advanced Usage

### Lambda Registration

```cpp
// Register lambda functions
SubProgramRegister::registProgram("test", [](int argc, char* argv[]) -> int {
    std::cout << "Running test with " << argc << " arguments" << std::endl;
    for (int i = 0; i < argc; ++i) {
        std::cout << "  argv[" << i << "] = " << argv[i] << std::endl;
    }
    return 0;
});
```

### Class Method Registration

```cpp
class CommandProcessor {
public:
    int processData(int argc, char* argv[]) {
        std::cout << "Processing data..." << std::endl;
        return 0;
    }
    
    void registerCommands() {
        // Bind member function
        SubProgramRegister::registProgram("process", 
            [this](int argc, char* argv[]) -> int {
                return this->processData(argc, argv);
            });
    }
};
```

### Error Handling

```cpp
int safe_command(int argc, char* argv[]) {
    try {
        // Command implementation
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Command failed: " << e.what() << std::endl;
        return 1;
    }
}

// Register with error handling
SubProgramRegister::registProgram("safe", safe_command);
```

### Help System Integration

```cpp
class HelpSystem {
    struct CommandInfo {
        std::string description;
        std::vector<std::string> examples;
    };
    
    static std::map<std::string, CommandInfo> command_help_;
    
public:
    static void registerWithHelp(const std::string& name, 
                                SubProgramFunc func,
                                const std::string& description,
                                const std::vector<std::string>& examples = {}) {
        SubProgramRegister::registProgram(name, func);
        command_help_[name] = {description, examples};
    }
    
    static void showHelp(const std::string& command = "") {
        if (command.empty()) {
            // Show all commands
            auto programs = SubProgramRegister::listSortedSubPrograms();
            std::cout << "Available commands:" << std::endl;
            for (const auto& name : programs) {
                auto it = command_help_.find(name);
                if (it != command_help_.end()) {
                    std::cout << "  " << name << " - " << it->second.description << std::endl;
                } else {
                    std::cout << "  " << name << std::endl;
                }
            }
        } else {
            // Show specific command help
            auto it = command_help_.find(command);
            if (it != command_help_.end()) {
                std::cout << "Command: " << command << std::endl;
                std::cout << "Description: " << it->second.description << std::endl;
                if (!it->second.examples.empty()) {
                    std::cout << "Examples:" << std::endl;
                    for (const auto& example : it->second.examples) {
                        std::cout << "  " << example << std::endl;
                    }
                }
            } else {
                std::cout << "No help available for command: " << command << std::endl;
            }
        }
    }
};

std::map<std::string, HelpSystem::CommandInfo> HelpSystem::command_help_;
```

## Use Cases

### Multi-Tool Application

```cpp
// tools/file_tools.cpp
int copy_files(int argc, char* argv[]) {
    // File copy implementation
    return 0;
}

int list_directory(int argc, char* argv[]) {
    // Directory listing implementation  
    return 0;
}

void register_file_tools() {
    SubProgramRegister::registProgram("copy", copy_files);
    SubProgramRegister::registProgram("ls", list_directory);
}

// tools/network_tools.cpp  
int ping_host(int argc, char* argv[]) {
    // Ping implementation
    return 0;
}

int download_file(int argc, char* argv[]) {
    // Download implementation
    return 0;
}

void register_network_tools() {
    SubProgramRegister::registProgram("ping", ping_host);
    SubProgramRegister::registProgram("download", download_file);
}

// main.cpp
int main(int argc, char* argv[]) {
    // Register all tool modules
    register_file_tools();
    register_network_tools();
    
    // Handle command dispatch
    if (argc < 2) {
        show_available_commands();
        return 1;
    }
    
    return dispatch_command(argv[1], argc - 1, argv + 1);
}
```

### Plugin Architecture

```cpp
// Plugin interface
class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual void registerCommands() = 0;
    virtual std::string getName() const = 0;
};

// Plugin manager
class PluginManager {
    std::vector<std::unique_ptr<IPlugin>> plugins_;
    
public:
    void loadPlugin(std::unique_ptr<IPlugin> plugin) {
        plugin->registerCommands();
        plugins_.push_back(std::move(plugin));
    }
    
    void listPlugins() {
        for (const auto& plugin : plugins_) {
            std::cout << "Plugin: " << plugin->getName() << std::endl;
        }
    }
};

// Example plugin
class DatabasePlugin : public IPlugin {
public:
    void registerCommands() override {
        SubProgramRegister::registProgram("db-query", 
            [](int argc, char* argv[]) -> int {
                // Database query implementation
                return 0;
            });
            
        SubProgramRegister::registProgram("db-migrate", 
            [](int argc, char* argv[]) -> int {
                // Database migration implementation
                return 0;
            });
    }
    
    std::string getName() const override {
        return "Database Tools";
    }
};
```

### Test Framework Integration

```cpp
class TestRunner {
public:
    static void registerTest(const std::string& name, std::function<bool()> test_func) {
        SubProgramRegister::registProgram("test-" + name, 
            [test_func](int argc, char* argv[]) -> int {
                try {
                    bool result = test_func();
                    std::cout << (result ? "PASS" : "FAIL") << ": " << argv[0] << std::endl;
                    return result ? 0 : 1;
                } catch (const std::exception& e) {
                    std::cout << "ERROR: " << argv[0] << " - " << e.what() << std::endl;
                    return 1;
                }
            });
    }
    
    static int runAllTests(int argc, char* argv[]) {
        auto programs = SubProgramRegister::listSortedSubPrograms();
        int passed = 0, failed = 0;
        
        for (const auto& name : programs) {
            if (name.starts_with("test-")) {
                std::cout << "Running " << name << "... ";
                int result = SubProgramRegister::runSubProgram(name, 1, &argv[0]);
                if (result == 0) {
                    passed++;
                } else {
                    failed++;
                }
            }
        }
        
        std::cout << "\nTest Results: " << passed << " passed, " << failed << " failed" << std::endl;
        return (failed == 0) ? 0 : 1;
    }
};

// Register the test runner itself
void register_test_framework() {
    SubProgramRegister::registProgram("run-all-tests", TestRunner::runAllTests);
}
```

## Implementation Details

### Internal Storage

The registry uses a static map to store function mappings:

```cpp
class SubProgramRegister {
private:
    static std::unordered_map<std::string, SubProgramFunc>& getFunctionMap() {
        static std::unordered_map<std::string, SubProgramFunc> func_map;
        return func_map;
    }
    
public:
    static void registProgram(const std::string& name, SubProgramFunc func) {
        auto& map = getFunctionMap();
        if (map.find(name) == map.end()) {
            map[name] = std::move(func);
        }
        // Note: Duplicate registrations are ignored
    }
};
```

### Thread Safety

The current implementation is **not thread-safe**. For multi-threaded use:

```cpp
// Thread-safe version (not implemented)
class ThreadSafeSubProgramRegister {
private:
    static std::shared_mutex registry_mutex_;
    static std::unordered_map<std::string, SubProgramFunc> func_map_;
    
public:
    static void registProgram(const std::string& name, SubProgramFunc func) {
        std::unique_lock lock(registry_mutex_);
        func_map_[name] = std::move(func);
    }
    
    static bool hasSubProgram(const std::string& name) {
        std::shared_lock lock(registry_mutex_);
        return func_map_.find(name) != func_map_.end();
    }
    
    // Other methods with appropriate locking...
};
```

## Performance Characteristics

- **Registration**: O(1) average, O(n) worst case (hash map)
- **Lookup**: O(1) average, O(n) worst case  
- **Invocation**: O(1) + function execution time
- **List Generation**: O(n log n) for sorted list
- **Memory**: ~40 bytes per registered function

## Error Handling

### Registration Errors

```cpp
// Duplicate registration handling
void safe_register(const std::string& name, SubProgramFunc func) {
    if (SubProgramRegister::hasSubProgram(name)) {
        std::cerr << "Warning: Command '" << name << "' already registered" << std::endl;
        return;
    }
    SubProgramRegister::registProgram(name, func);
}
```

### Execution Errors

```cpp
int safe_run_subprogram(const std::string& name, int argc, char* argv[]) {
    if (!SubProgramRegister::hasSubProgram(name)) {
        std::cerr << "Error: Unknown command '" << name << "'" << std::endl;
        
        // Suggest similar commands
        auto programs = SubProgramRegister::listSubPrograms();
        auto suggestions = find_similar_strings(name, programs);
        if (!suggestions.empty()) {
            std::cerr << "Did you mean:" << std::endl;
            for (const auto& suggestion : suggestions) {
                std::cerr << "  " << suggestion << std::endl;
            }
        }
        return 1;
    }
    
    try {
        return SubProgramRegister::runSubProgram(name, argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Error executing '" << name << "': " << e.what() << std::endl;
        return 1;
    }
}
```

## Building

```cmake
target_link_libraries(your_target PRIVATE lux::cxx::subprogram)
```

## Dependencies

- **C++ Standard**: C++20 or later
- **Standard Library**: `<functional>`, `<unordered_map>`, `<vector>`, `<string>`
- **Internal**: None

## Examples

### Complete Command-Line Tool

```cpp
#include <lux/cxx/subprogram/subprogram.hpp>
#include <iostream>
#include <fstream>
#include <sstream>

// Command implementations
int cmd_version(int argc, char* argv[]) {
    std::cout << "MyTool v1.0.0" << std::endl;
    return 0;
}

int cmd_echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::cout << argv[i];
        if (i < argc - 1) std::cout << " ";
    }
    std::cout << std::endl;
    return 0;
}

int cmd_count_lines(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: count-lines <file>" << std::endl;
        return 1;
    }
    
    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "Error: Cannot open file " << argv[1] << std::endl;
        return 1;
    }
    
    int line_count = 0;
    std::string line;
    while (std::getline(file, line)) {
        line_count++;
    }
    
    std::cout << line_count << " lines in " << argv[1] << std::endl;
    return 0;
}

int cmd_help(int argc, char* argv[]) {
    std::cout << "Available commands:" << std::endl;
    auto programs = lux::cxx::SubProgramRegister::listSortedSubPrograms();
    for (const auto& name : programs) {
        std::cout << "  " << name << std::endl;
    }
    return 0;
}

int main(int argc, char* argv[]) {
    using namespace lux::cxx;
    
    // Register all commands
    SubProgramRegister::registProgram("version", cmd_version);
    SubProgramRegister::registProgram("echo", cmd_echo);
    SubProgramRegister::registProgram("count-lines", cmd_count_lines);
    SubProgramRegister::registProgram("help", cmd_help);
    
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <command> [args...]" << std::endl;
        std::cout << "Run '" << argv[0] << " help' for available commands." << std::endl;
        return 1;
    }
    
    std::string command = argv[1];
    
    if (SubProgramRegister::hasSubProgram(command)) {
        return SubProgramRegister::runSubProgram(command, argc - 1, argv + 1);
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        std::cerr << "Run '" << argv[0] << " help' for available commands." << std::endl;
        return 1;
    }
}
```

## Future Enhancements

- [ ] Thread-safe registry implementation
- [ ] Command aliasing support
- [ ] Built-in help system integration
- [ ] Command completion support
- [ ] Configuration file integration
- [ ] Command history and logging
- [ ] Plugin hot-loading support