# Box Modules and Binary Compilation

## Overview

Box modules (external native modules from `nt-box`) work differently than built-in modules when using binary compilation (`-b` flag).

## Module Types

### 1. Built-in Modules (Embedded)
These modules are **compiled into** the binary:
- `sys` - System operations
- `json` - JSON parsing
- `math` - Mathematical functions  
- `http` - HTTP client
- `time` - Time and date
- `fmt` - Formatting utilities
- `arrays` - Array operations
- `async` - Async operations

**Binary compilation:** ✅ Fully embedded, no external dependencies

### 2. Neutron Modules (`.nt` files - Embedded)
User-created Neutron modules from `lib/` or `box/`:
```neutron
// my_utils.nt
fun helper(x) {
    return x * 2;
}
```

**Binary compilation:** ✅ Source code embedded into binary

### 3. Box Native Modules (`.so`/`.dylib`/`.dll` - Linked)
Native C++ modules installed via `box install` or built with `box build`:
```
.box/modules/
└── base64/
    ├── base64.so (Linux)
    ├── base64.dylib (macOS)
    └── base64.dll (Windows)
```

**Binary compilation:** ✅ **Automatically linked** - the `-b` flag now detects and links box modules from `.box/modules/` into your executable!

## How Box Modules Work with Binaries

When you compile a script that uses box modules:

### Example Script
```neutron
// myapp.nt
use sys;      // Built-in - will be embedded
use base64;   // Box module - automatically linked!

var text = "Hello World";
var encoded = base64.encode(text);
sys.say(encoded);
```

### Compilation
```bash
$ ./neutron -b myapp.nt myapp
[Binary Compiler] Starting binary compilation...
[Binary Compiler] Generating embedded C++ source...
[Binary Compiler] Compiling final executable...
Compiler: g++
[Success] Executable created: myapp
Size: 0.85 MB
```

The binary compiler **automatically**:
1. Detects box modules used by your script (via `use` statements)
2. Finds the native module files in `.box/modules/`
3. Links them directly into your executable

### Distribution Structure
Your compiled binary is **standalone** - no need to distribute box modules separately!

```
your-app/
└── myapp              # Fully self-contained executable
```

The box modules are **linked** into the binary at compile time, so everything is included.

### How It Works
When you compile with `-b`:
1. Built-in modules (sys, json, etc.) are embedded in the runtime
2. Neutron `.nt` modules have their source embedded
3. **Box native modules** from `.box/modules/` are **linked** as shared libraries
   - The linker includes the `.so`/`.dylib`/`.dll` code
   - No runtime `dlopen()` needed
   - Creates a self-contained executable

## Advantages of Linked Box Modules

**Benefits of automatic linking:**
1. **Standalone binaries** - No need to distribute separate module files
2. **Simplicity** - One executable file is easier to deploy
3. **Performance** - Linked modules may have better performance than dynamically loaded ones
4. **Dependencies included** - All box modules your script uses are automatically detected and linked

**Note:** The box modules must be installed in `.box/modules/` at **compile time**. Make sure to:
```bash
./box install modulename  # Install before compiling
./neutron -b myscript.nt  # Box modules automatically linked
```

## Installation Methods

### Method 1: Install via Box (Recommended)
```bash
# Install module from NUR registry
./box install base64

# Module installed to .box/modules/base64/
```

### Method 2: Build from Source
```bash
# Build a custom native module
./box build native box/mymodule/native.cpp 1.0.0

# Creates: box/mymodule/mymodule.so
```

### Method 3: Manual Development
```bash
# For development, place in box/ directory
mkdir -p box/mymodule
# Create native.cpp
# Compile manually or use neutron --build-box mymodule
```

## Distribution Best Practices

### Simple Distribution - Standalone Binary
Since box modules are now linked at compile time, distribution is straightforward:

```bash
# Package structure
myapp-v1.0/
├── myapp           # Standalone executable with box modules linked
├── README.md
└── LICENSE
```

Just compile with `-b` and distribute the single executable!

### Option 2: Installer Script
```bash
#!/bin/bash
# install.sh

# Copy binary
cp myapp /usr/local/bin/

# Copy modules
mkdir -p ~/.neutron/modules
cp -r box/* ~/.neutron/modules/

echo "Installation complete!"
```

### Option 3: Package Manager
Create distribution packages (`.deb`, `.rpm`, Homebrew formula) that:
1. Install the binary to `/usr/local/bin/myapp`
2. Install modules to `/usr/local/lib/neutron/modules/`
3. Set up proper paths

## Module Loading Paths

The VM searches for modules in this order:

1. **Built-in modules** - Hardcoded in VM
2. `.box/modules/modulename/modulename.{so,dylib,dll}`
3. `box/modulename/modulename.{so,dylib,dll}`
4. `box/modulename/modulename.nt` (Neutron module)
5. `lib/modulename.nt` (Built-in Neutron helpers)

## Checking Module Types

To see what type of module you're using:

```bash
# Check if it's built-in
./neutron -c "use sys; sys.say('works')"  # Built-in

# Check for .nt file
ls lib/json.nt  # Neutron wrapper for built-in

# Check for native module
ls box/base64/base64.so  # External native module
ls .box/modules/base64/base64.so  # Box-installed module
```

## Development Workflow

### For App Developers
1. Develop script with `use modulename`
2. Test with interpreter: `./neutron script.nt`
3. Compile to binary: `./neutron -b script.nt myapp`
4. Package binary + required `box/` modules
5. Distribute together

### For Module Developers
1. Create native module in `box/mymodule/native.cpp`
2. Build with: `./box build native box/mymodule/native.cpp 1.0.0`
3. Test with script: `use mymodule; ...`
4. Publish to NUR registry (optional)
5. Users install via: `./box install mymodule`

## Troubleshooting

### Error: "Module 'base64' not found"
**Problem:** Binary can't find the `.so` file

**Solution:**
```bash
# Ensure module exists
ls box/base64/base64.so

# Or check .box/
ls .box/modules/base64/base64.so

# Install if missing
./box install base64
```

### Error: "missing neutron_module_init function"
**Problem:** `.so` file is not a valid Neutron module

**Solution:**
Ensure your module exports `neutron_module_init()`:
```cpp
extern "C" {
    void neutron_module_init(neutron::VM* vm) {
        // Register functions...
    }
}
```

### Binary runs on dev machine but not production
**Problem:** Module files not distributed

**Solution:**
Package and copy `box/` or `.box/modules/` directories with your binary.

## Summary

| Module Type | Location | Compilation | Distribution |
|-------------|----------|-------------|--------------|
| Built-in (sys, json, etc.) | Compiled into neutron | ✅ Embedded | N/A - in binary |
| Neutron `.nt` | `lib/`, `box/` | ✅ Embedded | N/A - in binary |
| Box Native `.so` | `.box/modules/`, `box/` | ❌ External | ⚠️ Must distribute |

**Key Point:** When using binary compilation with box modules, you're creating a **partially standalone** binary that still requires the native module `.so`/`.dylib`/`.dll` files at runtime.

For **fully standalone** binaries, stick to built-in modules only.
