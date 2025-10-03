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

Neutron supports two types of imports:

### Module Imports (`use`)
Import built-in or native modules:
```neutron
use json;
use math;
use convert;
```

### File Imports (`using`)
Import other Neutron source files:
```neutron
using 'utils.nt';
using 'lib/helpers.nt';
```

**Built-in modules include:**
- `sys` - System operations (auto-loaded)
- `math` - Mathematical functions (auto-loaded)
- `json` - JSON parsing and serialization  
- `convert` - Type conversions
- `time` - Time operations
- `http` - HTTP client

**Module Access:**
```neutron
use json;
var data = json.parse("{\"key\": \"value\"}");

using 'utils.nt';
say(greet("World"));  // Function from utils.nt
```

**Error Handling:**
If you forget to import a module, you'll get a helpful error:
```
Runtime error: Undefined variable 'json'. Did you forget to import it? Use 'use json;' at the top of your file.
```

For complete documentation, see [Module System Guide](docs/module_system.md).

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
- Added `using 'filename.nt';` syntax for importing Neutron files
- Built-in modules are now loaded on-demand with helpful error messages
- Improved error handling with suggestions for missing imports

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

**Module System Tests:**
- `test_import.nt` - Demonstrates `use` and `using` syntax
- `test_utils.nt` - Example utility file for imports
- `test_error.nt` - Error handling demonstration
- `comprehensive_module_test.nt` - Full module system test

All development tests have been verified to work with the current implementation.

## Architecture

- **Scanner**: Tokenizes source code
- **Parser**: Creates abstract syntax tree
- **Compiler**: Generates bytecode
- **Virtual Machine**: Executes bytecode
- **Runtime**: Handles values, memory management, and native functions