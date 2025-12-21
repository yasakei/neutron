# Extending Neutron with Native Modules

This document explains how to create native C++ modules for Neutron using the Box package manager.

## Overview

Neutron supports native C++ modules that can be loaded at runtime. These modules use the Neutron C API to expose functions to Neutron code. The Box package manager handles building and installing these modules with automatic compiler detection.

## Creating a Native Module

Native modules can be shared via the [Neutron Universal Registry (NUR)](https://github.com/neutron-modules/nur). Browse existing modules or contribute your own!

### 1. Module Structure

Create a directory for your module with a `native.cpp` file:

```bash
mkdir mymodule
cd mymodule
```

### 2. Write the Module Code

Create `native.cpp` with your module implementation:

```cpp
// Tell the header we're building to avoid dllimport
#define BUILDING_NEUTRON
#include <neutron.h>
#undef BUILDING_NEUTRON

#include <string>

// Example native function
NeutronValue* greet(NeutronVM* vm, int argCount, NeutronValue** args) {
    if (argCount != 1 || !neutron_is_string(args[0])) {
        return neutron_new_string(vm, "Error: Expected one string argument", 35);
    }
    
    size_t length;
    const char* name = neutron_get_string(args[0], &length);
    
    std::string greeting = "Hello, " + std::string(name, length) + "!";
    return neutron_new_string(vm, greeting.c_str(), greeting.length);
}

// Module initialization - required entry point
extern "C" __declspec(dllexport) void neutron_module_init(NeutronVM* vm) {
    neutron_define_native(vm, "greet", greet, 1);
}
```

### 3. Create Module Definition File (Windows)

For Windows, create `mymodule.def`:

```
EXPORTS
neutron_module_init
```

### 4. Build the Module

Use Box to build your local module:

```bash
box build native mymodule
```

This will:
- Detect your system's C++ compiler (MSVC, GCC, Clang, MinGW)
- Compile `native.cpp` with the Neutron headers
- Link against the Neutron runtime (using dynamic symbol resolution)
- Install to `.box/modules/mymodule/`

To share your module with others, contribute it to [NUR](https://github.com/neutron-modules/nur). Then users can install it with:

```bash
box install mymodule
```

### 5. Use the Module

```js
use mymodule;

say(mymodule.greet("World"));  // Output: Hello, World!
```

## Neutron C API Reference

### Type Checking

```cpp
bool neutron_is_nil(NeutronValue* value);
bool neutron_is_boolean(NeutronValue* value);
bool neutron_is_number(NeutronValue* value);
bool neutron_is_string(NeutronValue* value);
```

### Value Getters

```cpp
bool neutron_get_boolean(NeutronValue* value);
double neutron_get_number(NeutronValue* value);
const char* neutron_get_string(NeutronValue* value, size_t* length);
```

### Value Creators

```cpp
NeutronValue* neutron_new_nil();
NeutronValue* neutron_new_boolean(bool value);
NeutronValue* neutron_new_number(double value);
NeutronValue* neutron_new_string(NeutronVM* vm, const char* chars, size_t length);
```

### Function Registration

```cpp
void neutron_define_native(NeutronVM* vm, const char* name, NeutronNativeFn function, int arity);
```

## Platform-Specific Notes

### Windows

- Use `__declspec(dllexport)` on `neutron_module_init`
- Create a `.def` file to export the init function
- Box supports both MSVC and MinGW compilers

### Linux/macOS

- No special export declarations needed
- Box automatically uses `-fPIC` and `-shared` flags
- Modules are `.so` (Linux) or `.dylib` (macOS)

## Advanced Example

Here's a more complete example with multiple functions and error handling:

```cpp
#define BUILDING_NEUTRON
#include <neutron.h>
#undef BUILDING_NEUTRON

#include <string>
#include <cmath>

// Add two numbers
NeutronValue* add(NeutronVM* vm, int argCount, NeutronValue** args) {
    if (argCount != 2) {
        return neutron_new_string(vm, "Error: Expected 2 arguments", 27);
    }
    
    if (!neutron_is_number(args[0]) || !neutron_is_number(args[1])) {
        return neutron_new_string(vm, "Error: Arguments must be numbers", 32);
    }
    
    double a = neutron_get_number(args[0]);
    double b = neutron_get_number(args[1]);
    
    return neutron_new_number(a + b);
}

// Calculate square root
NeutronValue* sqrt_func(NeutronVM* vm, int argCount, NeutronValue** args) {
    if (argCount != 1 || !neutron_is_number(args[0])) {
        return neutron_new_string(vm, "Error: Expected one number", 26);
    }
    
    double value = neutron_get_number(args[0]);
    if (value < 0) {
        return neutron_new_string(vm, "Error: Cannot take sqrt of negative", 35);
    }
    
    return neutron_new_number(std::sqrt(value));
}

// Module initialization
extern "C" __declspec(dllexport) void neutron_module_init(NeutronVM* vm) {
    neutron_define_native(vm, "add", add, 2);
    neutron_define_native(vm, "sqrt", sqrt_func, 1);
}
```

## Troubleshooting

### Module Not Found

Ensure the module is in `.box/modules/<modulename>/` and the shared library has the correct name:
- Windows: `<modulename>.dll`
- Linux: `<modulename>.so`
- macOS: `<modulename>.dylib`

### Linking Errors

Box uses runtime symbol resolution, so you don't need to link against import libraries. The native shim (`nt-box/src/native_shim.cpp`) handles dynamic loading of the Neutron API.

### Compiler Not Found

Box automatically detects compilers. If detection fails:
- **Linux**: Install `gcc` or `clang`
- **macOS**: Install Xcode Command Line Tools
- **Windows**: Install Visual Studio or MSYS2 MinGW

See the [Build Guide](guides/build.md) for detailed compiler setup instructions.