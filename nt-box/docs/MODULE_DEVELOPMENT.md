# Module Development Guide

Learn how to create native C++ modules for Neutron using the C API.

## Table of Contents

- [Overview](#overview)
- [Getting Started](#getting-started)
- [Neutron C API](#neutron-c-api)
- [Module Structure](#module-structure)
- [Building Modules](#building-modules)
- [Publishing to NUR](#publishing-to-nur)
- [Examples](#examples)

---

## Overview

Neutron native modules are shared libraries (.so/.dll/.dylib) written in C++ that extend the language with custom functionality. They use the Neutron C API to interact with the runtime.

### Why Create Native Modules?

- **Performance:** Implement performance-critical code in C++
- **System Integration:** Access OS-level APIs and system libraries
- **Third-Party Libraries:** Wrap existing C/C++ libraries for use in Neutron
- **Reusability:** Share modules across projects via NUR

---

## Getting Started

### Prerequisites

- C++17 or later compiler (g++, clang++, or MSVC)
- Neutron runtime installed
- Box package manager
- CMake (optional, for complex builds)

### Quick Start

1. **Create module directory:**

```sh
mkdir my-module
cd my-module
```

2. **Create `module.json`:**

```json
{
  "name": "mymodule",
  "version": "1.0.0",
  "description": "My first Neutron module",
  "entry": "mymodule.cpp",
  "include_dirs": [],
  "libraries": [],
  "compiler_flags": []
}
```

3. **Create `mymodule.cpp`:**

```cpp
#include <neutron/capi.h>

extern "C" {

void myfunction(NeutronVM* vm, int argc, NeutronValue* args) {
    const char* message = "Hello from native code!";
    NeutronValue result = neutron_new_string(vm, message);
    neutron_return(vm, result);
}

void neutron_module_init(NeutronVM* vm) {
    neutron_define_native(vm, "myfunction", myfunction);
}

}
```

4. **Build:**

```sh
box build
```

5. **Test:**

```neutron
use mymodule;
say(mymodule.myfunction());  // "Hello from native code!"
```

---

## Neutron C API

### Core Types

```cpp
// Opaque VM handle
typedef struct NeutronVM NeutronVM;

// Value type (any Neutron value)
typedef struct {
    /* implementation details */
} NeutronValue;

// Native function signature
typedef void (*NeutronNativeFunction)(NeutronVM* vm, int argc, NeutronValue* args);
```

### Module Registration

```cpp
// Define a native function in the module
void neutron_define_native(NeutronVM* vm, const char* name, NeutronNativeFunction func);

// Module entry point (required)
void neutron_module_init(NeutronVM* vm);
```

### Value Construction

```cpp
// Create string value
NeutronValue neutron_new_string(NeutronVM* vm, const char* str);

// Create number value
NeutronValue neutron_new_number(NeutronVM* vm, double value);

// Create boolean value
NeutronValue neutron_new_boolean(NeutronVM* vm, bool value);

// Create nil value
NeutronValue neutron_new_nil(NeutronVM* vm);
```

### Type Checking

```cpp
// Check if value is string
bool neutron_is_string(NeutronValue value);

// Check if value is number
bool neutron_is_number(NeutronValue value);

// Check if value is boolean
bool neutron_is_boolean(NeutronValue value);

// Check if value is nil
bool neutron_is_nil(NeutronValue value);
```

### Value Extraction

```cpp
// Get C string from Neutron string
const char* neutron_get_string(NeutronValue value);

// Get double from Neutron number
double neutron_get_number(NeutronValue value);

// Get bool from Neutron boolean
bool neutron_get_boolean(NeutronValue value);
```

### Return Values

```cpp
// Return value to Neutron
void neutron_return(NeutronVM* vm, NeutronValue value);

// Throw runtime error
void neutron_error(NeutronVM* vm, const char* message);
```

---

## Module Structure

### Directory Layout

```
my-module/
├── module.json          # Module metadata
├── mymodule.cpp         # Main source file
├── include/             # Optional: header files
│   └── mymodule.h
├── src/                 # Optional: additional sources
│   └── utils.cpp
└── bin/                 # Generated: build output
    └── v1.0.0/
        └── mymodule.so
```

### module.json Format

```json
{
  "name": "mymodule",
  "version": "1.0.0",
  "description": "Short description",
  "entry": "mymodule.cpp",
  "include_dirs": ["include", "/usr/local/include"],
  "libraries": ["curl", "ssl"],
  "compiler_flags": ["-O3", "-Wall"]
}
```

**Fields:**
- `name` - Module name (matches `use <name>;`)
- `version` - Semantic version (e.g., "1.0.0")
- `description` - Short description for NUR
- `entry` - Main source file
- `include_dirs` - Additional include directories
- `libraries` - System libraries to link against
- `compiler_flags` - Additional compiler flags

---

## Building Modules

### Using Box

The simplest way to build:

```sh
box build
```

Box automatically:
- Detects your platform (Linux/Windows/macOS)
- Chooses the correct compiler (g++/clang++/MSVC)
- Adds required flags (-std=c++17, -shared/-dynamiclib/LD, -fPIC)
- Includes Neutron headers
- Outputs to `bin/v<version>/`

### Platform-Specific Compilation

#### Linux

```sh
g++ -std=c++17 -shared -fPIC \
    -I/usr/local/include/neutron \
    -o bin/v1.0.0/mymodule.so \
    mymodule.cpp
```

#### macOS

```sh
clang++ -std=c++17 -dynamiclib -fPIC \
    -I/usr/local/include/neutron \
    -o bin/v1.0.0/mymodule.dylib \
    mymodule.cpp
```

#### Windows

**Using MSVC:**

```powershell
cl /std:c++17 /LD /MD ^
   /I"C:\Program Files\Neutron\include" ^
   /Fe:bin\v1.0.0\mymodule.dll ^
   mymodule.cpp
```

**Using MSYS2/MINGW64:**

```bash
g++ -std=c++17 -shared -fPIC \
    -I/mingw64/include/neutron \
    -o bin/v1.0.0/mymodule.dll \
    mymodule.cpp
```

Box automatically detects which compiler to use based on your environment.

### Using CMake

For complex projects, use CMake:

```cmake
cmake_minimum_required(VERSION 3.15)
project(mymodule)

set(CMAKE_CXX_STANDARD 17)

# Find Neutron headers
find_path(NEUTRON_INCLUDE_DIR neutron/capi.h
    PATHS /usr/local/include /opt/homebrew/include)

add_library(mymodule SHARED mymodule.cpp)
target_include_directories(mymodule PRIVATE ${NEUTRON_INCLUDE_DIR})

# Set output name and location
set_target_properties(mymodule PROPERTIES
    PREFIX ""
    OUTPUT_NAME "mymodule"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/v1.0.0"
)
```

---

## Publishing to NUR

### 1. Create GitHub Repository

```sh
gh repo create neutron-modules/mymodule --public
cd mymodule
git init
git add .
git commit -m "Initial commit"
git branch -M main
git remote add origin https://github.com/neutron-modules/mymodule.git
git push -u origin main
```

### 2. Build for All Platforms

Build on each platform and commit binaries:

```sh
# On Linux
box build
git add bin/v1.0.0/mymodule.so

# On macOS
box build
git add bin/v1.0.0/mymodule.dylib

# On Windows
box build
git add bin/v1.0.0/mymodule.dll

git commit -m "Add v1.0.0 binaries"
git push
```

### 3. Add to NUR Registry

Edit `neutron-modules/nur/modules/mymodule.json`:

```json
{
  "name": "mymodule",
  "description": "My custom Neutron module",
  "latest": "1.0.0",
  "versions": {
    "1.0.0": {
      "description": "Initial release",
      "entry-linux": "https://github.com/neutron-modules/mymodule/raw/main/bin/v1.0.0/mymodule.so",
      "entry-win": "https://github.com/neutron-modules/mymodule/raw/main/bin/v1.0.0/mymodule.dll",
      "entry-mac": "https://github.com/neutron-modules/mymodule/raw/main/bin/v1.0.0/mymodule.dylib",
      "deps": []
    }
  }
}
```

Update `nur.json`:

```json
{
  "modules": [
    "base64",
    "json",
    "mymodule"
  ]
}
```

### 4. Test Installation

```sh
box install mymodule
```

---

## Examples

### Example 1: String Manipulation

```cpp
#include <neutron/capi.h>
#include <algorithm>
#include <string>

extern "C" {

void reverse_string(NeutronVM* vm, int argc, NeutronValue* args) {
    if (argc != 1 || !neutron_is_string(args[0])) {
        neutron_error(vm, "Expected 1 string argument");
        return;
    }
    
    std::string str = neutron_get_string(args[0]);
    std::reverse(str.begin(), str.end());
    
    NeutronValue result = neutron_new_string(vm, str.c_str());
    neutron_return(vm, result);
}

void neutron_module_init(NeutronVM* vm) {
    neutron_define_native(vm, "reverse", reverse_string);
}

}
```

Usage:

```neutron
use mymodule;
say(mymodule.reverse("hello"));  // "olleh"
```

### Example 2: Math Operations

```cpp
#include <neutron/capi.h>
#include <cmath>

extern "C" {

void power(NeutronVM* vm, int argc, NeutronValue* args) {
    if (argc != 2 || !neutron_is_number(args[0]) || !neutron_is_number(args[1])) {
        neutron_error(vm, "Expected 2 number arguments");
        return;
    }
    
    double base = neutron_get_number(args[0]);
    double exp = neutron_get_number(args[1]);
    double result = std::pow(base, exp);
    
    neutron_return(vm, neutron_new_number(vm, result));
}

void sqrt_func(NeutronVM* vm, int argc, NeutronValue* args) {
    if (argc != 1 || !neutron_is_number(args[0])) {
        neutron_error(vm, "Expected 1 number argument");
        return;
    }
    
    double value = neutron_get_number(args[0]);
    double result = std::sqrt(value);
    
    neutron_return(vm, neutron_new_number(vm, result));
}

void neutron_module_init(NeutronVM* vm) {
    neutron_define_native(vm, "power", power);
    neutron_define_native(vm, "sqrt", sqrt_func);
}

}
```

Usage:

```neutron
use mathx;
say(mathx.power(2, 10));  // 1024
say(mathx.sqrt(144));     // 12
```

### Example 3: File Operations

```cpp
#include <neutron/capi.h>
#include <fstream>
#include <sstream>

extern "C" {

void read_file(NeutronVM* vm, int argc, NeutronValue* args) {
    if (argc != 1 || !neutron_is_string(args[0])) {
        neutron_error(vm, "Expected 1 string argument (filename)");
        return;
    }
    
    const char* filename = neutron_get_string(args[0]);
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        neutron_error(vm, "Failed to open file");
        return;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    neutron_return(vm, neutron_new_string(vm, content.c_str()));
}

void write_file(NeutronVM* vm, int argc, NeutronValue* args) {
    if (argc != 2 || !neutron_is_string(args[0]) || !neutron_is_string(args[1])) {
        neutron_error(vm, "Expected 2 string arguments (filename, content)");
        return;
    }
    
    const char* filename = neutron_get_string(args[0]);
    const char* content = neutron_get_string(args[1]);
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        neutron_error(vm, "Failed to open file for writing");
        return;
    }
    
    file << content;
    neutron_return(vm, neutron_new_boolean(vm, true));
}

void neutron_module_init(NeutronVM* vm) {
    neutron_define_native(vm, "read", read_file);
    neutron_define_native(vm, "write", write_file);
}

}
```

Usage:

```neutron
use fileio;

var content = fileio.read("data.txt");
say(content);

fileio.write("output.txt", "Hello, World!");
```

### Example 4: Error Handling

```cpp
#include <neutron/capi.h>
#include <stdexcept>

extern "C" {

void divide(NeutronVM* vm, int argc, NeutronValue* args) {
    if (argc != 2) {
        neutron_error(vm, "Expected 2 arguments");
        return;
    }
    
    if (!neutron_is_number(args[0]) || !neutron_is_number(args[1])) {
        neutron_error(vm, "Both arguments must be numbers");
        return;
    }
    
    double a = neutron_get_number(args[0]);
    double b = neutron_get_number(args[1]);
    
    if (b == 0.0) {
        neutron_error(vm, "Division by zero");
        return;
    }
    
    neutron_return(vm, neutron_new_number(vm, a / b));
}

void neutron_module_init(NeutronVM* vm) {
    neutron_define_native(vm, "divide", divide);
}

}
```

---

## Best Practices

### 1. Error Handling

Always validate arguments and handle errors gracefully:

```cpp
void my_function(NeutronVM* vm, int argc, NeutronValue* args) {
    // Validate argument count
    if (argc != 2) {
        neutron_error(vm, "Expected 2 arguments");
        return;
    }
    
    // Validate argument types
    if (!neutron_is_string(args[0])) {
        neutron_error(vm, "First argument must be a string");
        return;
    }
    
    // Your logic here
}
```

### 2. Memory Management

The Neutron VM handles memory for NeutronValue objects. Don't free them manually.

For C strings from `neutron_get_string()`, the pointer is valid only during the function call. Copy if needed:

```cpp
std::string str = neutron_get_string(args[0]);  // Safe: copies data
```

### 3. Thread Safety

Native functions should be thread-safe if Neutron adds multi-threading support. Avoid global mutable state.

### 4. Versioning

Follow semantic versioning:
- **Major:** Breaking API changes
- **Minor:** New features, backward compatible
- **Patch:** Bug fixes

### 5. Documentation

Document your module's API in a README:
- Function signatures
- Parameter types
- Return values
- Error conditions
- Usage examples

---

## Troubleshooting

### Undefined Symbol Errors

**Problem:** `undefined symbol: neutron_define_native`

**Solution:** Ensure Neutron was built with symbol export flags:
```cmake
target_link_options(neutron PRIVATE "-Wl,--export-dynamic")
```

### Compilation Errors

**Problem:** `fatal error: neutron/capi.h: No such file or directory`

**Solution:** Install Neutron headers or specify include path:
```sh
g++ -I/usr/local/include/neutron mymodule.cpp ...
```

### Module Not Found

**Problem:** `Module 'mymodule' not found`

**Solution:** Ensure module is in `.box/modules/mymodule/` with correct platform extension (.so/.dll/.dylib).

---

## See Also

- [Box Commands Reference](COMMANDS.md) - All Box commands
- [Cross-Platform Notes](CROSS_PLATFORM.md) - Platform-specific guidance
- [Neutron C API Header](https://github.com/yasakei/neutron/blob/main/include/capi.h) - Full API reference
