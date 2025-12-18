# Module System in Neutron

## Overview

Neutron supports two types of imports:
1. **Module imports** using `use modulename;` - for built-in and native modules
2. **File imports** using `using 'filename.nt';` - for importing other Neutron source files

**Important:** As of the latest version, all built-in modules use **lazy loading** and must be explicitly imported with `use modulename;` before use.

## Module Imports (`use`)

### Syntax
```neutron
use modulename;
```

### Built-in Modules

The following modules are built into the Neutron runtime and require explicit import:

- **sys** - File I/O, directory operations, environment access, process control (fully implemented)
- **json** - JSON parsing and stringification
- **math** - Mathematical operations
- **fmt** - Type conversion and formatting utilities
- **time** - Time and date functions
- **http** - HTTP client operations
- **async** - Asynchronous operations and multi-threading

**Note:** Modules are lazily loaded - they're only initialized when you explicitly use `use modulename;`. This provides faster startup times and explicit dependencies.

### Example
```neutron
use sys;
use json;
use convert;

// sys module - fully functional
sys.write("data.txt", "Hello, Neutron!");
var content = sys.read("data.txt");

// json module
var obj = json.parse("{\"name\": \"test\"}");
say(json.stringify(obj));

// fmt module
var num = fmt.to_int("42");
say(num);
```

## File Imports (`using`)

### Syntax
```neutron
using 'filepath.nt';
```

### Description
Import another Neutron source file into the current scope. All functions and variables defined in the imported file become available in the current file.

### Search Paths
Files are searched in the following order:
1. Current directory (`.`)
2. `lib/` directory
3. `box/` directory

### Example

**utils.nt:**
```neutron
fun greet(name) {
    return "Hello, " + name + "!";
}

var VERSION = "1.0.0";
```

**main.nt:**
```neutron
using 'utils.nt';

say(greet("World"));  // Output: Hello, World!
say(VERSION);         // Output: 1.0.0
```

## Selective Imports

Import only specific symbols from modules or files using the `from` keyword:

### Syntax
```neutron
// From modules
use (symbol1, symbol2) = from modulename;

// From files
using (symbol1, symbol2) = from 'filepath.nt';
```

### Examples

**Import specific functions from a module:**
```neutron
use (now, sleep) = from time;

say(now());    // Available
sleep(100);    // Available
// format() is NOT available
```

**Import specific functions from a file:**

**math_helpers.nt:**
```neutron
fun add(a, b) { return a + b; }
fun subtract(a, b) { return a - b; }
fun multiply(a, b) { return a * b; }
```

**main.nt:**
```neutron
using (add, multiply) = from 'math_helpers.nt';

say(add(5, 3));      // Works: 8
say(multiply(4, 2)); // Works: 8
say(subtract(5, 2)); // Error: undefined variable
```

### Benefits

- **Namespace clarity**: Only import what you need
- **Avoid conflicts**: Prevent name collisions when importing from multiple sources
- **Better performance**: Smaller global scope
- **Explicit dependencies**: Clear which symbols come from which module

### Comparison

```neutron
// Traditional import (imports everything)
use time;
time.now();
time.sleep(100);
time.format(timestamp, "%Y-%m-%d");

// Selective import (imports only what you need)
use (now, sleep) = from time;
now();
sleep(100);
// format() is not available
```

---

If you try to use a module without importing it, you'll get a helpful error message:

```
Runtime error: Undefined variable 'json'. Did you forget to import it? Use 'use json;' at the top of your file.
```

## lib/ Folder

The `lib/` folder is **optional** and can be used to organize your Neutron library files. You can:

1. **Keep it** - Use it to store reusable `.nt` library files
2. **Remove it** - If you prefer to organize files differently

The files currently in `lib/` are wrapper files for the built-in modules. Since the modules are now built-in and can be loaded with `use`, these files are **no longer necessary** and can be removed.

### Recommendation

**Remove the lib/ folder** if you're only using built-in modules. The built-in modules (json, math, sys, fmt, time, http) are now loaded directly from the runtime.

If you want to keep custom `.nt` library files, you can:
- Keep them in the project root
- Create a custom directory (e.g., `modules/` or `libs/`)
- Update the search paths in `src/vm.cpp` if needed

## Best Practices

1. **Import modules at the top of your file**
   ```neutron
   use json;
   use fmt;
   
   using 'utils.nt';
   using 'helpers.nt';
   
   // ... rest of your code
   ```

2. **Only import what you need** - Don't import modules you're not using

3. **Organize related functions in separate files** and use `using` to import them

4. **Use descriptive filenames** for your `.nt` files

## Module Development

Neutron supports creating native C++ modules that extend the language with custom functionality.

### Using Box Package Manager

Box is Neutron's official package manager for native modules. Modules are available in the [Neutron Universal Registry (NUR)](https://github.com/neutron-modules/nur):

```sh
# Install a module from NUR (downloads and builds automatically)
box install base64

# Or build from local source files
box build native base64

# Use in your code
use base64;
say(base64.encode("Hello!"));
```

### Creating Native Modules

To create your own native C++ module:

1. **Set up module structure:**
   ```sh
   mkdir mymodule
   cd mymodule
   ```

2. **Create native.cpp:**
   ```cpp
   // Tell the header we're building Neutron to avoid dllimport
   #define BUILDING_NEUTRON
   #include <neutron.h>
   #undef BUILDING_NEUTRON
   
   #include <string>
   
   // Native function example
   NeutronValue* hello(NeutronVM* vm, int argCount, NeutronValue** args) {
       return neutron_new_string(vm, "Hello from my module!", 21);
   }
   
   // Module initialization - this is the entry point
   extern "C" __declspec(dllexport) void neutron_module_init(NeutronVM* vm) {
       neutron_define_native(vm, "hello", hello, 0);
   }
   ```

3. **Create module definition file (Windows only) - mymodule.def:**
   ```
   EXPORTS
   neutron_module_init
   ```

4. **Build the module:**
   ```sh
   box build native mymodule
   ```

### Neutron C API

The Neutron C API provides functions for creating native modules:

**Type Checking:**
- `bool neutron_is_nil(NeutronValue* value)`
- `bool neutron_is_boolean(NeutronValue* value)`
- `bool neutron_is_number(NeutronValue* value)`
- `bool neutron_is_string(NeutronValue* value)`

**Value Getters:**
- `bool neutron_get_boolean(NeutronValue* value)`
- `double neutron_get_number(NeutronValue* value)`
- `const char* neutron_get_string(NeutronValue* value, size_t* length)`

**Value Creators:**
- `NeutronValue* neutron_new_nil()`
- `NeutronValue* neutron_new_boolean(bool value)`
- `NeutronValue* neutron_new_number(double value)`
- `NeutronValue* neutron_new_string(NeutronVM* vm, const char* chars, size_t length)`

**Function Registration:**
- `void neutron_define_native(NeutronVM* vm, const char* name, NeutronNativeFn function, int arity)`

See `include/core/neutron.h` for the complete API reference.

### Module Installation

Modules install to `.box/modules/` in your project directory:

```
.box/
└── modules/
    └── mymodule/
        ├── mymodule.so       # Linux
        ├── mymodule.dll      # Windows  
        ├── mymodule.dylib    # macOS
        └── metadata.json
```

Neutron automatically searches `.box/modules/` when you use `use mymodule;`

### Supported Platforms

Box supports building modules on:
- **Linux:** GCC, Clang → `.so`
- **macOS:** Clang → `.dylib`
- **Windows:** MSVC, MINGW64 → `.dll`

Box automatically detects your compiler and generates the appropriate build commands.
