# Neutron

Neutron is a simple, modern, and lightweight interpreted programming language written in C++. It is designed to be easy to learn and use, with a clean and expressive syntax.

## Quick Links

- 🚀 **[Quick Start Guide](docs/QUICKSTART.md)** - Get up and running in 5 minutes
- 📖 **[Complete Build Guide](docs/BUILD.md)** - Platform-specific build instructions
- 📚 **[Language Reference](docs/language_reference.md)** - Full syntax documentation
- ✅ **[Test Suite](docs/TEST_SUITE.md)** - 21 tests covering all features

## Features

- **Dynamically Typed:** No need to declare the type of a variable.
- **C-like Syntax:** Familiar syntax for developers who have used C, C++, Java, or JavaScript.
- **Rich Standard Library:** A comprehensive standard library with modules for math, system operations, HTTP client, JSON processing, time operations, and data conversion.
- **Object-Oriented:** Supports classes, methods, and the `this` keyword.
- **Built-in Functions:** A set of useful built-in functions for string manipulation, type conversion, and console output.
- **Modular:** Supports both native C++ modules and Neutron language modules for organizing code.
- **File Imports:** Import other `.nt` files with the `using 'filename.nt';` syntax.
- **Binary Compilation:** Convert scripts to standalone executables with full module support.
- **Array Support:** Dynamic arrays with literal syntax, indexing, and built-in functions.
- **Enhanced Control Flow:** Fixed stack management issues in loops and conditionals for more reliable execution.
- **Improved Module System:** Robust module loading with proper recursive function support and helpful error messages.

## Getting Started

### Prerequisites

Neutron requires the following dependencies to build:

#### All Platforms
- **CMake** 3.15 or higher
- **C++17 compiler** (GCC 7+, Clang 5+, MSVC 2017+)
- **libcurl** - HTTP client library
- **jsoncpp** - JSON parsing library

#### Linux (Debian/Ubuntu)
```bash
sudo apt-get update
sudo apt-get install build-essential cmake pkg-config libcurl4-openssl-dev libjsoncpp-dev
```

#### Linux (Fedora/RHEL)
```bash
sudo dnf install gcc-c++ cmake pkgconfig libcurl-devel jsoncpp-devel
```

#### Linux (Arch)
```bash
sudo pacman -S base-devel cmake curl jsoncpp
```

#### macOS
```bash
# Install Homebrew if not already installed: https://brew.sh
brew install cmake curl jsoncpp
```

#### Windows

**Option 1: MSYS2 (Recommended)**
```bash
# Install MSYS2 from https://www.msys2.org/
# Then in MSYS2 MINGW64 terminal:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-curl mingw-w64-x86_64-jsoncpp make
```

**Option 2: vcpkg**
```cmd
# Install vcpkg from https://github.com/microsoft/vcpkg
vcpkg install curl jsoncpp
```

### Building the Project

Neutron uses CMake for cross-platform builds. Choose the instructions for your platform:

#### Linux / macOS

**Quick Build:**
```bash
cmake -B build -S .
cmake --build build -j$(nproc)
```

**Or step-by-step:**
```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
cd ..
```

**Clean rebuild:**
```bash
rm -rf build
cmake -B build -S .
cmake --build build -j$(nproc)
```

#### Windows (MSYS2)

**In MSYS2 MINGW64 terminal:**
```bash
cmake -B build -S . -G "MSYS Makefiles"
cmake --build build -j$(nproc)
```

**Or:**
```bash
mkdir build
cd build
cmake .. -G "MSYS Makefiles"
make -j$(nproc)
cd ..
```

#### Windows (Visual Studio)

**Using vcpkg:**
```cmd
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

**Or generate Visual Studio solution:**
```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
cd ..
```

#### Build Output

Successful builds create:
- `build/neutron` (or `neutron.exe` on Windows) - The interpreter
- `build/libneutron_runtime.so` / `.dll` / `.dylib` - Runtime library
- Copies in project root for convenience

#### Troubleshooting

**Linux:** If you get "curl not found" or "jsoncpp not found":
```bash
sudo apt-get install libcurl4-openssl-dev libjsoncpp-dev
# or use your distribution's package manager
```

**macOS:** If CMake can't find libraries:
```bash
export PKG_CONFIG_PATH="$(brew --prefix)/lib/pkgconfig"
cmake -B build -S .
```

**Windows/MSYS2:** Make sure you're using the MINGW64 terminal (not MSYS):
```bash
# The prompt should show: MINGW64
echo $MSYSTEM  # Should output: MINGW64
```

### Running the Interpreter

#### Linux / macOS

Run a Neutron script:
```bash
./neutron examples/hello.nt
# or from build directory:
./build/neutron examples/hello.nt
```

Start the REPL:
```bash
./neutron
```

#### Windows (MSYS2)

```bash
./neutron.exe examples/hello.nt
# or:
./build/neutron.exe examples/hello.nt
```

#### Windows (Visual Studio)

```cmd
neutron.exe examples\hello.nt
REM or:
build\Release\neutron.exe examples\hello.nt
```

### Running Tests

Neutron includes a comprehensive test suite with 21 tests covering all language features.

#### Linux / macOS

```bash
./run_tests.sh
```

#### Windows (MSYS2)

```bash
bash run_tests.sh
```

#### Windows (PowerShell)

```powershell
.\run_tests.ps1
```

All tests should pass with output:
```
================================
  Test Summary
================================
Total tests: 21
Passed: 21
Failed: 0

All tests passed!
```

### Binary Conversion

Neutron supports converting scripts to standalone executables:

```sh
./neutron -b script.nt [output.out]
```

This will compile the script and create a standalone executable that can be run directly. If no output file is specified, it will create `script.nt.out`.

To run the generated executable:

```sh
./script.nt.out
```

The generated executable contains the Neutron runtime and the embedded source code, making it a fully standalone program that can be distributed and run on any compatible system.

**Note:** See [Binary Conversion Documentation](docs/binary_conversion.md) for detailed information about supported features, limitations, and best practices.

## Documentation

- **[Complete Build Guide](docs/BUILD.md)** - Comprehensive building instructions for all platforms
- **[Language Reference](docs/language_reference.md)** - Complete language syntax and features
- **[Cross-Platform Guide](docs/cross_platform.md)** - Platform-specific implementation details
- **[Known Issues](docs/known_issues.md)** - Known bugs and limitations
- **[Test Suite Documentation](docs/TEST_SUITE.md)** - Test coverage and usage

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

## Documentation

For a complete reference of the Neutron language, please see the [Language Reference](docs/language_reference.md).

For known issues and limitations, please see the [Known Issues](docs/known_issues.md) documentation.

### Modules

Neutron includes several built-in modules and supports importing your own Neutron files.

**Important:** As of the latest version, all modules require explicit import using the `use` statement. Modules are lazily loaded only when needed.

**Module Import Syntax:**
```neutron
use sys;            // Import built-in module
use json;           // Import built-in module
using 'utils.nt';   // Import Neutron source file
```

**Built-in Modules:**
- **[Sys Module](docs/modules/sys_module.md)** - Complete file I/O, directory operations, environment access, and process control
- **[Math Module](docs/modules/math_module.md)** - Mathematical operations and functions  
- **[HTTP Module](docs/modules/http_module.md)** - HTTP client functionality
- **[JSON Module](docs/modules/json_module.md)** - JSON parsing and generation
- **[Time Module](docs/modules/time_module.md)** - Time and date functions
- **[Convert Module](docs/modules/convert_module.md)** - Data type conversion utilities

**Module Loading:**
All modules use lazy loading - they're only initialized when explicitly imported with `use`. This provides:
- Faster startup time
- Explicit dependencies
- Better memory management
- Clear module boundaries

For comprehensive module system documentation, see [Module System Guide](docs/module_system.md).

### External Modules

Neutron supports external modules that can be built using the `--build-box` command:

```sh
./neutron --build-box {module_name}
```

This command will build the specified C++ module from the `box/` directory and create a shared library that can be imported in Neutron code. The module must contain a `native.cpp` file with the appropriate C++ implementation.

Note: Currently, the `--build-box` command requires that the module follows the C++ module structure and that the Makefile is properly configured to build individual modules. For pure Neutron modules (`.nt` files), simply place them in the `box/` directory and they can be imported directly.

## Credits

This project was created and developed by **yasakei**.