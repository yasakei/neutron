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

Box is Neutron's official package manager for managing native modules:

```sh
# Install a module from NUR
box install base64

# Use in your code
use base64;
say(base64.encode("Hello!"));
```

### Creating Native Modules

To create your own native C++ module:

1. **Set up module structure:**
   ```sh
   mkdir my-module
   cd my-module
   ```

2. **Create module.json:**
   ```json
   {
     "name": "mymodule",
     "version": "1.0.0",
     "description": "My custom module",
     "entry": "mymodule.cpp"
   }
   ```

3. **Write C++ code using Neutron C API:**
   ```cpp
   #include <neutron/capi.h>
   
   extern "C" {
   void hello(NeutronVM* vm, int argc, NeutronValue* args) {
       neutron_return(vm, neutron_new_string(vm, "Hello!"));
   }
   
   void neutron_module_init(NeutronVM* vm) {
       neutron_define_native(vm, "hello", hello);
   }
   }
   ```

4. **Build the module:**
   ```sh
   box build
   ```

### Comprehensive Documentation

For complete module development documentation, see the Box documentation:

- **[Module Development Guide](../nt-box/docs/MODULE_DEVELOPMENT.md)** - Complete C API reference and examples
- **[Box Commands](../nt-box/docs/COMMANDS.md)** - All Box commands
- **[Cross-Platform Guide](../nt-box/docs/CROSS_PLATFORM.md)** - Building on Linux, macOS, Windows
- **[MINGW64 Support](../nt-box/docs/MINGW64_SUPPORT.md)** - Windows GCC toolchain

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
