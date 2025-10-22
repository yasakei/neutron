# Contributing to Neutron

We're excited that you're interested in contributing to Neutron! This document provides guidelines and information to help you get started.

## Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/yasakei/neutron.git`
3. Create a new branch for your feature: `git checkout -b my-feature`
4. Make your changes
5. Test your changes
6. Commit your changes: `git commit -am "Add my feature"`
7. Push to your fork: `git push origin my-feature`
8. Create a pull request

## Building the Project

Neutron uses CMake for cross-platform builds:

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

For development, you may want to do a clean rebuild:

```bash
rm -rf build
mkdir build
cd build
cmake ..
cmake --build .
```

## Project Structure

- `src/` - C++ source files
  - `compiler/` - Compiler components (scanner, parser, bytecode)
  - `runtime/` - Runtime components (native functions, debug)
  - `modules/` - Module system implementation
  - `types/` - Type implementations (json_object, json_array, return)
- `include/` - Header files
  - `types/` - Type headers (value, object, array, callable, etc.)
  - `runtime/` - Runtime headers
  - `compiler/` - Compiler headers
  - `modules/` - Module system headers
- `libs/` - Built-in native modules (sys, json, math, http, time, fmt)
- `examples/` - Example Neutron programs
- `programs/` - Sample Neutron programs
- `dev_tests/` - Development tests for core functionality
- `tests/` - Unit tests
- `docs/` - Documentation
- `lib/` - Neutron module wrappers (mostly placeholders)
- `box/` - External modules (native and Neutron)
- `build/` - Build artifacts (created by CMake)

## Key Improvements

### Recent Updates (October 2025)

**sys Module Complete Implementation:**
- Fully implemented all file operations: `read`, `write`, `append`, `cp`, `mv`, `rm`, `exists`
- Fully implemented directory operations: `mkdir`, `rmdir`
- Fully implemented system info: `cwd`, `chdir`, `env`, `args`, `info`
- Fully implemented process control: `exit`, `exec`
- Cross-platform support (Linux, macOS, Windows)

**Lazy Module Loading:**
- All built-in modules now require explicit import with `use modulename;`
- Modules are only initialized when imported
- Faster startup time, explicit dependencies, better memory management

**Object Property Access:**
- JsonObject now supports dot notation for property access
- `sys.info()` returns object with `.platform`, `.arch`, `.cwd` properties

**Build System Migration:**
- Migrated from Makefile to CMake for cross-platform builds
- Better dependency management
- Separate compilation units for better organization

### Previous Improvements

**Stack Management Fix:**
- Fixed issue in `OP_JUMP_IF_FALSE` bytecode operation to properly manage the evaluation stack
- Resolves function call return value corruption in control flow constructs
- Affects loops and conditional statements

**Module System Improvements:**
- Enhanced module loading with proper recursive function call support
- Better module search path handling
- Ensured modules from same file can call each other during execution

**Language Features:**
- Proper `this` keyword functionality for class methods
- Correct string concatenation with `+` operator
- Enhanced native function integration with dual access patterns (global and module access)

## Architecture Components

- **Scanner**: `scanner.h`/`scanner.cpp` - Tokenizes source code
- **Parser**: `parser.h`/`parser.cpp` - Creates abstract syntax tree  
- **Compiler**: `compiler.h`/`compiler.cpp` - Generates bytecode
- **Virtual Machine**: `vm.h`/`vm.cpp` - Executes bytecode
- **Runtime**: `runtime.cpp` - Handles values, objects, and native functions

## Coding Standards

- Follow C++17 standards
- Use meaningful variable and function names
- Comment your code where necessary
- Write unit tests for new functionality

## Development Tests

The `dev_tests/` directory contains comprehensive tests for core functionality:
- `feature_test.nt` - Core language features
- `module_test.nt` - Module system functionality  
- `class_test.nt` - Object-oriented features
- `binary_conversion_feature_test.nt` - Full feature test

## Testing

Run the existing tests to ensure nothing is broken:

```bash
# Run development tests
./neutron dev_tests/feature_test.nt
./neutron dev_tests/module_test.nt
./neutron dev_tests/class_test.nt
./neutron dev_tests/binary_conversion_feature_test.nt

# Build and test
make all
```

Add new tests when implementing features.

## Reporting Issues

Please use the GitHub issue tracker to report bugs or suggest features.

## Contact

For questions or discussions, please open an issue or contact the maintainers.