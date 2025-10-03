# Module System in Neutron

## Overview

Neutron supports two types of imports:
1. **Module imports** using `use modulename;` - for built-in and native modules
2. **File imports** using `using 'filename.nt';` - for importing other Neutron source files

## Module Imports (`use`)

### Syntax
```neutron
use modulename;
```

### Built-in Modules

The following modules are built into the Neutron runtime:

- **json** - JSON parsing and stringification
- **math** - Mathematical operations (auto-loaded)
- **sys** - System functions like input() (auto-loaded)
- **convert** - Type conversion utilities
- **time** - Time and date functions
- **http** - HTTP client operations

### Example
```neutron
use json;
use convert;

var obj = json.parse("{\"name\": \"test\"}");
say(json.stringify(obj));

var num = convert.int("42");
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

## Error Handling

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

**Remove the lib/ folder** if you're only using built-in modules. The built-in modules (json, math, sys, convert, time, http) are now loaded directly from the runtime.

If you want to keep custom `.nt` library files, you can:
- Keep them in the project root
- Create a custom directory (e.g., `modules/` or `libs/`)
- Update the search paths in `src/vm.cpp` if needed

## Best Practices

1. **Import modules at the top of your file**
   ```neutron
   use json;
   use convert;
   
   using 'utils.nt';
   using 'helpers.nt';
   
   // ... rest of your code
   ```

2. **Only import what you need** - Don't import modules you're not using

3. **Organize related functions in separate files** and use `using` to import them

4. **Use descriptive filenames** for your `.nt` files

## Module Development

To create a native module (C++ extension):
1. Create a directory in `box/` or `libs/`
2. Implement the module in C++
3. Compile it as a shared library
4. Load it with `use modulename;`

For more details, see `docs/box_modules.md`.
