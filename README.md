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

**Quick Setup:** See [Quick Start Guide](docs/QUICKSTART.md) for a 5-minute setup.

**Detailed Instructions:** See [Complete Build Guide](docs/BUILD.md) for comprehensive platform-specific instructions.

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

See [Test Suite Documentation](docs/TEST_SUITE.md) for details.

## Documentation

- **[Quick Start Guide](docs/QUICKSTART.md)** - Get up and running in 5 minutes
- **[Complete Build Guide](docs/BUILD.md)** - Comprehensive building instructions for all platforms
- **[Language Reference](docs/language_reference.md)** - Complete language syntax and features
- **[Test Suite](docs/TEST_SUITE.md)** - 21 tests covering all features
- **[Cross-Platform Guide](docs/cross_platform.md)** - Platform-specific implementation details
- **[Known Issues](docs/known_issues.md)** - Known bugs and limitations
- **[Module System](docs/module_system.md)** - Module loading and usage

### Binary Conversion

Neutron supports converting scripts to standalone executables:

```sh
./neutron -b script.nt [output.out]
```

See [Binary Conversion Documentation](docs/binary_conversion.md) for detailed information.

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

For comprehensive documentation, see [Module System Guide](docs/module_system.md).

## Credits

This project was created and developed by **yasakei**.