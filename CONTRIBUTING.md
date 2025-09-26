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

## Project Structure

- `src/` - C++ source files
- `include/` - Header files
- `examples/` - Example Neutron programs
- `dev_tests/` - Development tests for core functionality
- `tests/` - Unit tests
- `docs/` - Documentation
- `lib/` - Built-in Neutron modules
- `box/` - External modules (native and Neutron)
- `build/` - Build artifacts

## Key Improvements

### Stack Management Fix
- Fixed issue in `OP_JUMP_IF_FALSE` bytecode operation to properly manage the evaluation stack
- Resolves function call return value corruption in control flow constructs
- Affects loops and conditional statements

### Module System Improvements
- Enhanced module loading with proper recursive function call support
- Better module search path handling
- Ensured modules from same file can call each other during execution

### Language Features
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