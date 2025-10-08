# Creating Native Modules for Neutron

> **📦 This documentation has moved!**
>
> Native module development is now managed by **Box**, Neutron's official package manager.
> 
> Please refer to the comprehensive Box documentation for creating, building, and distributing native modules.

## Box Documentation

Complete documentation for native module development is available in the `nt-box/docs/` directory:

### 📚 Essential Guides

- **[Module Development Guide](../nt-box/docs/MODULE_DEVELOPMENT.md)**
  - Complete C API reference
  - Step-by-step module creation
  - Working code examples
  - Publishing to NUR (Neutron Universe Registry)

- **[Box Commands Reference](../nt-box/docs/COMMANDS.md)**
  - `box build` - Build native modules
  - `box install` - Install modules from NUR
  - All Box commands with examples

- **[Cross-Platform Guide](../nt-box/docs/CROSS_PLATFORM.md)**
  - Building on Linux (GCC, Clang)
  - Building on macOS (Clang)
  - Building on Windows (MSVC, MINGW64)
  - Platform-specific troubleshooting

- **[MINGW64 Support](../nt-box/docs/MINGW64_SUPPORT.md)**
  - Building with GCC on Windows
  - MSYS2 installation and setup
  - Alternative to Visual Studio

### 🚀 Quick Start

#### 1. Create Module Structure

```sh
mkdir my-module
cd my-module
```

#### 2. Create `module.json`

```json
{
  "name": "mymodule",
  "version": "1.0.0",
  "description": "My first Neutron module",
  "entry": "mymodule.cpp"
}
```

#### 3. Create `mymodule.cpp`

```cpp
#include <neutron/capi.h>

extern "C" {

void hello(NeutronVM* vm, int argc, NeutronValue* args) {
    neutron_return(vm, neutron_new_string(vm, "Hello from native code!"));
}

void neutron_module_init(NeutronVM* vm) {
    neutron_define_native(vm, "hello", hello);
}

}
```

#### 4. Build with Box

```sh
box build
```

#### 5. Use in Neutron

```neutron
use mymodule;
say(mymodule.hello());  // "Hello from native code!"
```

## Box Features

- ✅ **Cross-Platform**: Automatic compiler detection (GCC, Clang, MSVC, MINGW64)
- ✅ **Zero Configuration**: Box handles all build flags automatically
- ✅ **Version Management**: Install and manage multiple module versions
- ✅ **NUR Integration**: Publish and share modules via Neutron Universe Registry
- ✅ **Local Installation**: Modules install to `.box/modules/` in your project

## Neutron C API Overview

### Core Functions

```cpp
// Module initialization (required)
void neutron_module_init(NeutronVM* vm);

// Define native functions
void neutron_define_native(NeutronVM* vm, const char* name, 
                          NeutronNativeFunction func);

// Create values
NeutronValue neutron_new_string(NeutronVM* vm, const char* str);
NeutronValue neutron_new_number(NeutronVM* vm, double value);
NeutronValue neutron_new_boolean(NeutronVM* vm, bool value);
NeutronValue neutron_new_nil(NeutronVM* vm);

// Check types
bool neutron_is_string(NeutronValue value);
bool neutron_is_number(NeutronValue value);
bool neutron_is_boolean(NeutronValue value);
bool neutron_is_nil(NeutronValue value);

// Extract values
const char* neutron_get_string(NeutronValue value);
double neutron_get_number(NeutronValue value);
bool neutron_get_boolean(NeutronValue value);

// Return values and errors
void neutron_return(NeutronVM* vm, NeutronValue value);
void neutron_error(NeutronVM* vm, const char* message);
```

For complete API documentation with examples, see:
- [Module Development Guide](../nt-box/docs/MODULE_DEVELOPMENT.md)
- [C API Header](../include/capi.h)

## Module Examples

### String Manipulation

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
    
    neutron_return(vm, neutron_new_string(vm, str.c_str()));
}

void neutron_module_init(NeutronVM* vm) {
    neutron_define_native(vm, "reverse", reverse_string);
}

}
```

### Math Operations

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

void neutron_module_init(NeutronVM* vm) {
    neutron_define_native(vm, "power", power);
}

}
```

## Platform-Specific Builds

Box automatically handles platform differences:

| Platform | Compiler | Command | Output |
|----------|----------|---------|--------|
| Linux | g++ | `box build` | `.so` |
| Linux | clang++ | `box build` | `.so` |
| macOS | clang++ | `box build` | `.dylib` |
| Windows | MSVC | `box build` | `.dll` |
| Windows | MINGW64 | `box build` | `.dll` |

No manual configuration needed!

## Publishing Modules

To share your module via NUR (Neutron Universe Registry):

1. **Build for all platforms**
2. **Create GitHub repository**
3. **Add to NUR registry** 
4. **Users install with:** `box install yourmodule`

See [Module Development Guide](../nt-box/docs/MODULE_DEVELOPMENT.md#publishing-to-nur) for complete publishing instructions.

## Migration from Old Box System

If you have existing modules in the old `box/` directory format:

1. Create `module.json` in your module directory
2. Run `box build` to compile
3. Install with `box install`

Old modules will continue to work, but we recommend migrating to the new Box system for better cross-platform support and version management.

## Getting Help

- **Documentation:** See `nt-box/docs/` directory
- **Examples:** Check existing modules in NUR
- **Issues:** Report at https://github.com/yasakei/nt-box/issues

## Legacy Information

**Note:** This file previously contained detailed C API documentation. That information has been superseded by the comprehensive guides in `nt-box/docs/`. The old box directory structure is deprecated in favor of the new Box package manager workflow.

Or in the standard library:

```
lib/
└── mymodule.nt
```

**Example Neutron module (`lib/utils.nt`):**

```neutron
// utils.nt - A utility module written in Neutron

fun double_value(x) {
    return x * 2;
}

fun greet_user(name) {
    return "Welcome, " + name + "!";
}

fun calculate_area(width, height) {
    return width * height;
}

// Module can have initialization code
say("Utils module loaded");
```

**Using a Neutron module:**

```neutron
use utils;

var doubled = utils.double_value(21);    // 42
var greeting = utils.greet_user("Alice"); // "Welcome, Alice!"
var area = utils.calculate_area(5, 3);    // 15

say(doubled);
say(greeting);
say("Area: " + area);
```

## 2. Implementation

The `native.cpp` (or `native.c`) file is the heart of your module. It contains the code that will be compiled into a shared library.

### 2.1. Required Headers

At a minimum, you will need to include `vm.h`, which provides access to the Neutron Virtual Machine and its core data structures.

```cpp
#include "vm.h"
#include <vector> // For std::vector
```

Note: Even when using C for your module implementation, you'll still need to use C++ features like `std::vector` to interact with the Neutron VM.

### 2.2. The Initialization Function

Every Box Module must export a C function named `neutron_module_init`. This function is the entry point of your module and is called by the Neutron VM when the module is loaded.

The function receives a pointer to the `VM` instance, which you can use to register your module's native functions.

```cpp
extern "C" void neutron_module_init(neutron::VM* vm) {
    // Register your native functions here.
}
```
*The `extern "C"` is crucial to prevent C++ name mangling, ensuring that the Neutron VM can find the initialization function.*

### 2.3. Defining Native Functions

Native functions are functions that can be called from Neutron code. They must have the following signature:

```cpp
neutron::Value my_native_function(std::vector<neutron::Value> args);
```

-   It takes a `std::vector<neutron::Value>` as an argument, which contains the arguments passed from the Neutron script.
-   It returns a `neutron::Value`, which is the value that will be returned to the Neutron script.

Inside your `neutron_module_init` function, you can register your native function using the module environment:

```cpp
// Create a module environment
auto env = std::make_shared<neutron::Environment>();

// Register your native function
env->define("my_function_name", neutron::Value(new neutron::NativeFn(my_native_function, arity)));

// Create the module and register it in the VM's globals
auto module = new neutron::Module("my_module", env);
vm->define_module("my_module", module);
```

-   `"my_function_name"`: The name of the function as it will be called from Neutron.
-   `my_native_function`: A pointer to your function.
-   `arity`: The number of arguments your function expects (-1 for variable arguments).

### 2.4. Working with Neutron Values

The `neutron::Value` struct is the primary way to interact with data from the Neutron VM. It can hold different types of data, such as numbers, strings, booleans, etc.

Here's how to work with `Value` objects:

-   **Checking the type**:
    ```cpp
    if (args[0].type == neutron::ValueType::NUMBER) {
        // It's a number
    }
    if (args[0].type == neutron::ValueType::STRING) {
        // It's a string
    }
    ```
-   **Getting the value**:
    ```cpp
    double number = std::get<double>(args[0].as);
    std::string str = std::get<std::string>(args[0].as);
    bool boolean = std::get<bool>(args[0].as);
    ```
-   **Creating a return value**:
    ```cpp
    return neutron::Value(123.45); // Return a number
    return neutron::Value("hello"); // Return a string
    return neutron::Value(true);    // Return a boolean
    return neutron::Value();        // Return nil
    ```

## 3. Building the Module

To compile your module, you can use the `--build-box` command of the `neutron` executable:

```sh
./neutron --build-box my_module
```

This command will compile `box/my_module/native.cpp` (or `box/my_module/native.c`) into a shared library located at `box/my_module/my_module.so`.

## 4. Using the Module in Neutron

Once your module is built, you can use it in any Neutron script with the `use` statement:

```neutron
use my_module;

// Now you can call the native functions you defined
var result = my_function_name(arg1, arg2);
say(result);
```

**Note on Binary Conversion:**

Box modules are dynamically loaded at runtime. This means that if you compile a script that uses a box module into a binary using the `-b` flag, the box module will **not** be included in the binary. The binary will fail at runtime when it tries to load the module.

To include module functionality in a standalone binary, the module must be written in Neutron (`.nt` file) and imported using the `use` statement. The source code of the `.nt` module will be embedded in the binary.

## 4.1. Built-in Modules

Neutron comes with several built-in native modules that demonstrate the Box Module system.

### Core Native Modules

These modules are compiled directly into the Neutron runtime and require explicit import with `use modulename;`:

1. **`sys`** - System operations, file I/O, environment access (FULLY IMPLEMENTED)
   - Located in: `libs/sys/native.cpp`
   - **File Operations:** `read`, `write`, `append`, `cp`, `mv`, `rm`, `exists`
   - **Directory Operations:** `mkdir`, `rmdir`
   - **System Info:** `cwd`, `chdir`, `env`, `args`, `info`
   - **User Input:** `input`
   - **Process Control:** `exit`, `exec`
   - Status: ✅ All features implemented and tested

2. **`json`** - JSON parsing and serialization
   - Located in: `libs/json/native.cpp`
   - Functions: `stringify`, `parse`, `get`

3. **`math`** - Mathematical operations
   - Located in: `libs/math/native.cpp`
   - Functions: `add`, `subtract`, `multiply`, `divide`, `pow`, `sqrt`, `abs`

4. **`http`** - HTTP client functionality
   - Located in: `libs/http/native.cpp`
   - Functions: `get`, `post`, `put`, `delete`, `head`, `patch`

5. **`time`** - Time and date operations
   - Located in: `libs/time/native.cpp`
   - Functions: `now`, `format`, `sleep`

6. **`convert`** - String/binary conversion utilities
   - Located in: `libs/convert/native.cpp`
   - Functions: `char_to_int`, `int_to_char`, `string_length`, `string_get_char_at`, etc.

**Important:** All built-in modules use lazy loading - they're only initialized when explicitly imported with `use modulename;`.

### Standard Library Modules

These are placeholder Neutron modules (`.nt` files) in the `lib/` directory. Most functionality comes from the native modules above.

**Example built-in module usage:**

```neutron
use sys;
use json;
use math;

// File operations
sys.write("data.txt", "Hello, world!");
var content = sys.read("data.txt");

// JSON operations
var data = {"name": "Alice", "age": 30};
var jsonStr = json.stringify(data);

// Math operations
var result = math.pow(2, 8);  // 256
```

## 5. Creating Modules with C

While Neutron's Box module system is designed to work with C++, you can also implement modules using C with some considerations:

1. Your implementation file should be named `native.c` instead of `native.cpp`
2. You'll still need to use C++ features like `std::vector` and `neutron::Value` to interact with the VM
3. The build system will compile your C file with a C++ compiler

Here's a simple example of a C module (`box/hello_c/native.c`):

```c
#include "vm.h"
#include <vector>
#include <string>

// The native function that returns a simple greeting
neutron::Value hello_world(std::vector<neutron::Value> args) {
    return neutron::Value(std::string("Hello, World from C module!"));
}

// The native function that takes a name and returns a personalized greeting
neutron::Value greet(std::vector<neutron::Value> args) {
    // Handle case where no arguments are provided
    if (args.size() == 0) {
        return neutron::Value(std::string("Hello, Anonymous!"));
    }
    
    // Handle case where argument is not a string
    if (args[0].type != neutron::ValueType::STRING) {
        return neutron::Value(std::string("Hello, Anonymous!"));
    }
    
    std::string name = std::get<std::string>(args[0].as);
    std::string greeting = "Hello, " + name + "!";
    return neutron::Value(std::string(greeting));
}

// The module initialization function
extern "C" void neutron_module_init(neutron::VM* vm) {
    // Create a module environment
    auto env = std::make_shared<neutron::Environment>();
    
    // Register our functions (-1 means variable number of arguments)
    env->define("hello_world", neutron::Value(new neutron::NativeFn(hello_world, 0)));
    env->define("greet", neutron::Value(new neutron::NativeFn(greet, -1)));
    
    // Create the module and register it in the VM's globals
    auto module = new neutron::Module("hello_c", env);
    vm->define_module("hello_c", module);
}
```

Note that while your implementation file uses a `.c` extension, you're still using C++ features like `std::vector` and `std::string` to interact with the Neutron VM.

Currently, the build system only supports C++ files by default. To build a C module, you'll need to:

1. Rename your file to `native.cpp` 
2. Or manually compile your C file with a C++ compiler:

```bash
g++ -std=c++17 -shared -fPIC -Iinclude -I. -Ilibs/json -Ilibs/http -Ilibs/time -Ilibs/sys -Ibox box/hello_c/native.c -o box/hello_c/hello_c.so
```

To use this C module in a Neutron script:

```neutron
use hello_c;

var greeting = hello_c.hello_world();
say(greeting);

var personal_greeting = hello_c.greet("C Developer");
say(personal_greeting);
```

## 7. Simple Example: A Hello World Module

Let's create a simple module named `hello` that provides basic greeting functions.

**1. Create the directory structure:**
```
box/
└── hello/
    └── native.cpp
```

**2. Write the C++ code (`box/hello/native.cpp`):**
```cpp
#include "vm.h"
#include <vector>
#include <string>

// The native function that returns a simple greeting
neutron::Value hello_world(std::vector<neutron::Value> args) {
    return neutron::Value(std::string("Hello, World from C module!"));
}

// The native function that takes a name and returns a personalized greeting
neutron::Value greet(std::vector<neutron::Value> args) {
    // Handle case where no arguments are provided
    if (args.size() == 0) {
        return neutron::Value(std::string("Hello, Anonymous!"));
    }
    
    // Handle case where argument is not a string
    if (args[0].type != neutron::ValueType::STRING) {
        return neutron::Value(std::string("Hello, Anonymous!"));
    }
    
    std::string name = std::get<std::string>(args[0].as);
    std::string greeting = "Hello, " + name + "!";
    return neutron::Value(std::string(greeting));
}

// The module initialization function
extern "C" void neutron_module_init(neutron::VM* vm) {
    // Create a module environment
    auto env = std::make_shared<neutron::Environment>();
    
    // Register our functions (-1 means variable number of arguments)
    env->define("hello_world", neutron::Value(new neutron::NativeFn(hello_world, 0)));
    env->define("greet", neutron::Value(new neutron::NativeFn(greet, -1)));
    
    // Create the module and register it in the VM's globals
    auto module = new neutron::Module("hello", env);
    vm->define_module("hello", module);
}
```

**3. Build the module:**
```sh
./neutron --build-box hello
```
This will create `box/hello/hello.so`.

**4. Use the module in a Neutron script (`test_hello.nt`):**
```neutron
use hello;

var greeting = hello.hello_world();
say(greeting);

var personal_greeting = hello.greet("Neutron Developer");
say(personal_greeting);
```

**5. Run the script:**
```sh
./neutron test_hello.nt
```

## 8. Module Loading System

### 8.1. Module Resolution

When you use `use module_name;`, Neutron searches for modules in this order:

1. **Built-in modules**: Modules compiled into the runtime (`sys`, `json`, `math`, etc.)
2. **Box modules**: Native shared libraries in `box/module_name/module_name.so`
3. **Neutron modules**: `.nt` files in current directory or `lib/` directory
4. **Standard library**: `.nt` files in the `lib/` directory

### 8.2. Module Initialization

**For Native Box Modules:**
- The `neutron_module_init` function is called when the module is first loaded
- The function receives a `VM*` pointer to register functions and data
- Modules are loaded once and cached for subsequent `use` statements

**For Neutron Modules:**
- The entire `.nt` file is executed when first loaded
- Functions and variables become available in the module's namespace
- Module code runs in its own environment/scope

### 8.3. Module Caching

Neutron caches loaded modules to improve performance:
- Each module is loaded only once per interpreter session
- Subsequent `use` statements for the same module return the cached version
- This applies to both native and Neutron modules

## 9. Advanced Module Features

### 9.1. Inter-Module Dependencies

Modules can depend on other modules:

**In native modules:**
```cpp
extern "C" void neutron_module_init(neutron::VM* vm) {
    // Your module can use built-in modules
    // The VM already has sys, json, math, etc. loaded
    
    auto env = std::make_shared<neutron::Environment>();
    
    // Register functions that might use other modules internally
    env->define("advanced_function", neutron::Value(new neutron::NativeFn(my_function, 1)));
    
    auto module = new neutron::Module("my_advanced_module", env);
    vm->define_module("my_advanced_module", module);
}
```

**In Neutron modules:**
```neutron
// advanced_utils.nt
use sys;
use json;
use math;

fun save_data(filename, data) {
    var jsonStr = json.stringify(data);
    sys.write(filename, jsonStr);
    return true;
}

fun load_data(filename) {
    if (sys.exists(filename)) {
        var content = sys.read(filename);
        return json.parse(content);
    }
    return nil;
}

fun calculate_stats(numbers) {
    var sum = 0;
    var count = 0;
    // Note: This is a simplified example
    // Full array iteration would require additional language features
    return {"sum": sum, "count": count};
}
```

### 9.2. Module Versioning and Compatibility

When creating modules, consider:

1. **Version compatibility**: Ensure your module works with the target Neutron version
2. **API stability**: Try to maintain backward compatibility in your module's public API
3. **Error handling**: Provide clear error messages for unsupported operations
4. **Documentation**: Include version information and compatibility notes

## 10. Best Practices

### 10.1. For Native Box Modules

1. **Error Handling**: Always validate input arguments and handle errors gracefully
2. **Memory Management**: Be careful with resource allocation and deallocation
3. **Performance**: Use native modules for computationally intensive operations
4. **Type Safety**: Check argument types before processing
5. **Arity Validation**: Use correct arity values (-1 for variable arguments)

**Example with error handling:**
```cpp
neutron::Value safe_divide(std::vector<neutron::Value> args) {
    // Validate argument count
    if (args.size() != 2) {
        throw std::runtime_error("divide() expects exactly 2 arguments");
    }
    
    // Validate argument types
    if (args[0].type != neutron::ValueType::NUMBER || 
        args[1].type != neutron::ValueType::NUMBER) {
        throw std::runtime_error("divide() arguments must be numbers");
    }
    
    double a = std::get<double>(args[0].as);
    double b = std::get<double>(args[1].as);
    
    // Check for division by zero
    if (b == 0.0) {
        throw std::runtime_error("Division by zero");
    }
    
    return neutron::Value(a / b);
}
```

### 10.2. For Neutron Modules

1. **Clear Function Names**: Use descriptive names for exported functions
2. **Documentation**: Include comments explaining function purpose and parameters
3. **Input Validation**: Check parameters before processing
4. **Consistent Style**: Follow Neutron coding conventions
5. **Module Organization**: Group related functions logically

**Example well-structured Neutron module:**
```neutron
// file_utils.nt - File utility functions

use sys;

// Read file and return lines as array (simplified)
fun read_lines(filename) {
    if (!sys.exists(filename)) {
        say("Error: File " + filename + " does not exist");
        return nil;
    }
    
    var content = sys.read(filename);
    // Note: In a real implementation, you'd split by newlines
    return content;
}

// Backup a file with timestamp
fun backup_file(filename) {
    if (!sys.exists(filename)) {
        say("Error: Cannot backup non-existent file " + filename);
        return false;
    }
    
    var backup_name = filename + ".backup";
    sys.cp(filename, backup_name);
    say("File backed up to " + backup_name);
    return true;
}

// Check if file is writable (simplified check)
fun is_writable(filename) {
    // This is a simplified implementation
    // A real implementation might try to open for writing
    return sys.exists(filename);
}
```

### 10.3. General Guidelines

1. **Testing**: Create comprehensive test scripts for your modules
2. **Documentation**: Provide clear usage examples
3. **Performance**: Profile your modules to identify bottlenecks
4. **Compatibility**: Test with different Neutron versions if applicable
5. **Security**: Validate all inputs to prevent security issues

### 10.4. Module Distribution

1. **Package Structure**: Include documentation, examples, and tests
2. **Build Instructions**: Provide clear compilation steps for native modules
3. **Dependencies**: Document any external library requirements
4. **Licensing**: Include appropriate license information

## 11. Troubleshooting

### Common Issues

1. **Module Not Found**: Check module name matches directory/file name exactly
2. **Compilation Errors**: Ensure all required headers are included
3. **Runtime Errors**: Validate function signatures and parameter types
4. **Loading Failures**: Check that `neutron_module_init` is properly exported

### Debugging Tips

1. Use `say()` statements in Neutron modules for debugging
2. Add error checking in native modules with descriptive messages
3. Test modules individually before integrating
4. Check the Neutron console output for loading errors