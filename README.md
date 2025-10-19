# Neutron
[![CI](https://github.com/yasakei/neutron/actions/workflows/ci.yml/badge.svg)](https://github.com/yasakei/neutron/actions/workflows/ci.yml)
[![Release](https://github.com/yasakei/neutron/actions/workflows/release.yml/badge.svg)](https://github.com/yasakei/neutron/actions/workflows/release.yml)

Neutron is a simple, modern, and lightweight interpreted programming language written in C++. It is designed to be easy to learn and use, with a clean and expressive syntax.


## Quick Links

- 🚀 **[Quick Start Guide](docs/guides/QUICKSTART.md)** - Get up and running in 5 minutes
- 📖 **[Complete Build Guide](docs/guides/BUILD.md)** - Platform-specific build instructions
- 📚 **[Language Reference](docs/reference/language_reference.md)** - Full syntax documentation
- ✅ **[Test Suite](docs/guides/TEST_SUITE.md)** - 21 tests covering all features

## Features

- **Dynamically Typed:** No need to declare the type of a variable.
- **C-like Syntax:** Familiar syntax for developers who have used C, C++, Java, or JavaScript.
- **Rich Standard Library:** A comprehensive standard library with modules for math, system operations, HTTP client, JSON processing, time operations, and data conversion.
- **📦 Box Package Manager:** Install and manage native modules with ease.
- **Comprehensive Error Handling:** Meaningful error messages with source code context, visual indicators, and helpful suggestions for fixing issues.
- **Object-Oriented:** Supports classes, methods, and the `this` keyword.
- **Built-in Functions:** A set of useful built-in functions for string manipulation, type conversion, and console output.
- **Modular:** Supports both native C++ modules and Neutron language modules for organizing code.
- **File Imports:** Import other `.nt` files with the `using 'filename.nt';` syntax.
- **Binary Compilation:** Convert scripts to standalone executables with full module support.
- **Array Support:** Dynamic arrays with literal syntax, indexing, and built-in functions.
- **Enhanced Control Flow:** Fixed stack management issues in loops and conditionals for more reliable execution.
- **Improved Module System:** Robust module loading with proper recursive function support and helpful error messages.

## Getting Started

**Quick Setup:** See [Quick Start Guide](docs/guides/QUICKSTART.md) for a 5-minute setup.

**Detailed Instructions:** See [Complete Build Guide](docs/guides/BUILD.md) for comprehensive platform-specific instructions.

### Quick Build (Linux/macOS)

```bash
# Install dependencies
sudo apt-get install build-essential cmake libcurl4-openssl-dev libjsoncpp-dev  # Debian/Ubuntu
# or: brew install cmake curl jsoncpp  # macOS

# Build
cmake -B build -S .
cmake --build build -j$(nproc)

# Run
./neutron
```

### Quick Build (Windows/MSYS2)

```bash
# Install dependencies in MSYS2 MINGW64 terminal
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-curl mingw-w64-x86_64-jsoncpp make

# Build
cmake -B build -S . -G "MSYS Makefiles"
cmake --build build

# Run
./neutron.exe
```

### Running Tests

```bash
./run_tests.sh          # Linux/macOS
.\run_tests.ps1         # Windows PowerShell
```

See [Test Suite Documentation](docs/guides/TEST_SUITE.md) for details.

## Documentation

📖 **[Full Documentation Index](docs/README.md)**

### Getting Started
- **[Quick Start Guide](docs/guides/QUICKSTART.md)** - Get up and running in 5 minutes
- **[Complete Build Guide](docs/guides/BUILD.md)** - Comprehensive building instructions
- **[Test Suite](docs/guides/TEST_SUITE.md)** - 21 tests covering all features

### Reference
- **[Language Reference](docs/reference/language_reference.md)** - Complete language syntax
- **[Module System](docs/reference/module_system.md)** - Module loading and usage
- **[Error Handling](docs/error_handling/)** - Comprehensive error reporting system

### Implementation
- **[Cross-Platform Guide](docs/reference/cross_platform.md)** - Platform-specific details
- **[Known Issues](docs/implementation/known_issues.md)** - Known bugs and limitations
- **[Roadmap](docs/implementation/ROADMAP.md)** - Future plans

### Binary Conversion

Neutron supports converting scripts to standalone executables:

```sh
./neutron -b script.nt [output.out]
```

See [Binary Conversion Documentation](docs/reference/binary_conversion.md) for detailed information.

## 📦 Box Package Manager

Box is Neutron's official package manager for installing and managing native modules. It provides seamless cross-platform support and integrates with the Neutron Universe Registry (NUR).

### Quick Start

```sh
# Install Box (from nt-box directory)
cmake -B build && cmake --build build
sudo cmake --install build

# Install a module
box install base64

# Use the module in your code
use base64;
say(base64.encode("Hello, World!"));
```

### Features

- **Cross-Platform:** Supports Linux (GCC/Clang), macOS (Clang), Windows (MSVC/MINGW64)
- **Automatic Compiler Detection:** Detects and uses appropriate compiler for your platform
- **Version Management:** Install specific module versions with `module@version` syntax
- **Local Installation:** Modules install to `.box/modules/` in your project directory
- **NUR Integration:** Access modules from the Neutron Universe Registry
- **Native Module Building:** Build C++ modules with automatic platform-specific flags

### Available Commands

```sh
box install <module>[@version]  # Install a module
box list                        # List installed modules
box search [query]              # Search available modules
box remove <module>             # Remove a module
box build                       # Build a native module
box info <module>               # Show module information
```

### Documentation

Complete Box documentation is available in the `nt-box/docs/` directory:

- **[Box Guide](nt-box/docs/BOX_GUIDE.md)** - Comprehensive usage guide
- **[Commands Reference](nt-box/docs/COMMANDS.md)** - Detailed command documentation
- **[Module Development](nt-box/docs/MODULE_DEVELOPMENT.md)** - Creating native modules with C API
- **[Cross-Platform Guide](nt-box/docs/CROSS_PLATFORM.md)** - Platform-specific build notes
- **[MINGW64 Support](nt-box/docs/MINGW64_SUPPORT.md)** - Building with GCC on Windows

### Supported Platforms

| Platform | Compilers | Library Extension |
|----------|-----------|-------------------|
| Linux | GCC, Clang | `.so` |
| macOS | Clang | `.dylib` |
| Windows | MSVC, MINGW64 | `.dll` |

## Examples

Here are a few examples of what you can do with Neutron:

**Hello, World!**

```neutron
say("Hello, world!");
```

**Variables and Functions**

```neutron
fun greet(name) {
    return "Hello, " + name + "!";
}

var message = greet("Neutron");
say(message);
```

**Importing Other Files**

```neutron
// utils.nt
fun add(a, b) {
    return a + b;
}

// main.nt
using 'utils.nt';

say(add(5, 3));  // 8
```

**Using Modules**

```neutron
use json;

var data = {"name": "Neutron", "version": "1.0"};
var jsonStr = json.stringify(data);
say(jsonStr);  // {"name":"Neutron","version":"1.0"}
```

**Classes**

```neutron
class Person {
    var name;

    fun setName(name) {
        this.name = name;
    }

    fun greet() {
        say("Hello, my name is " + this.name);
    }
}

var person = Person();
person.setName("Neutron");
person.greet();
```

**Arrays**

```neutron
// Array literal syntax
var numbers = [1, 2, 3, 4, 5];
var fruits = ["apple", "banana", "cherry"];

// Index access and assignment
say(numbers[0]);      // 1
numbers[0] = 100;
say(numbers);         // [100, 2, 3, 4, 5]

// Array manipulation with native functions
var arr = array_new();
array_push(arr, "hello");
array_push(arr, "world");
say(array_length(arr));  // 2
say(arr);                // [hello, world]
```

## Modules

Neutron includes several built-in modules:

- **[Sys Module](docs/modules/sys_module.md)** - File I/O, directory operations, environment access, and process control
- **[Math Module](docs/modules/math_module.md)** - Mathematical operations and functions  
- **[HTTP Module](docs/modules/http_module.md)** - HTTP client functionality
- **[JSON Module](docs/modules/json_module.md)** - JSON parsing and generation
- **[Time Module](docs/modules/time_module.md)** - Time and date functions
- **[Convert Module](docs/modules/convert_module.md)** - Data type conversion utilities

**Usage:**
```neutron
use sys;            // Import built-in module
use json;           // Import built-in module
using 'utils.nt';   // Import Neutron source file
```

For comprehensive documentation, see [Module System Guide](docs/reference/module_system.md).

## Credits

This project was created and developed by **yasakei**.
