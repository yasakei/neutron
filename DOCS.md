# Neutron Programming Language

Neutron is a modern scripting language designed for simplicity and extensibility.

## Features

- **Simple Syntax**: C-style syntax that's easy to learn and read
- **Dynamic Typing**: Variables can hold any type of value
- **First-class Functions**: Functions can be assigned to variables and passed as arguments
- **Object-oriented Programming**: Classes with inheritance and encapsulation
- **Built-in Modules**: Standard library modules for common operations
- **Extensible**: Native modules can be written in C++

## Core Language Features

### Variables and Types
- `var name = value;` - variable declarations
- Types: nil, boolean, number, string, array, object, function

### Control Flow
- `if/else` statements
- `while` and `for` loops
- `return` statements

### Functions
- Function declarations: `fun name(params) { body }`
- First-class functions

### Classes
- Class declarations: `class Name { ... }`
- Methods and properties
- Instance creation with `Class()`

## Module System

Neutron supports modules through the `use` statement:
- `use module_name;` - imports a module
- Access module functions: `module_name.function()`

Built-in modules include:
- `sys` - System operations
- `json` - JSON parsing and serialization  
- `convert` - Type conversions
- `math` - Mathematical functions
- `time` - Time operations

## Key Fixes and Improvements

### Stack Management Fix
- Fixed issue in `OP_JUMP_IF_FALSE` bytecode operation
- Correctly pops condition values from evaluation stack
- Eliminates stack corruption in control flow constructs
- Resolves function call return value issues inside loops and conditionals

### Module Loading Enhancements
- Added proper module search path support
- Supports loading modules from current directory and standard paths
- Recursive function calls within modules now work properly

### Native Function Integration
- Proper registration of built-in functions and modules
- Both global access and module access patterns supported
- Examples: `str(42)` and `convert.str(42)` both work

## Building and Running

To build Neutron:
```bash
make all
```

To run a Neutron script:
```bash
./neutron script.nt
```

## Development Tests

The project includes development tests in the `dev_tests/` directory:
- `feature_test.nt` - Core language features
- `module_test.nt` - Module system functionality
- `class_test.nt` - Object-oriented features
- `binary_conversion_feature_test.nt` - Comprehensive feature test

All development tests have been verified to work with the current implementation.

## Architecture

- **Scanner**: Tokenizes source code
- **Parser**: Creates abstract syntax tree
- **Compiler**: Generates bytecode
- **Virtual Machine**: Executes bytecode
- **Runtime**: Handles values, memory management, and native functions