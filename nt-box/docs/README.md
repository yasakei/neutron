# Box Package Manager Documentation

Complete documentation for Box, the package manager for Neutron.

## Quick Links

- **[Box Guide](BOX_GUIDE.md)** - Comprehensive usage guide
- **[Commands Reference](COMMANDS.md)** - Detailed command documentation  
- **[Module Development](MODULE_DEVELOPMENT.md)** - Creating native modules with C API
- **[Cross-Platform Guide](CROSS_PLATFORM.md)** - Platform-specific build notes
- **[MINGW64 Support](MINGW64_SUPPORT.md)** - Building with GCC on Windows

## Overview

Box is Neutron's official package manager providing:
- Module installation from NUR (Neutron Universe Registry)
- Native module building with automatic compiler detection
- Cross-platform support (Linux, macOS, Windows)
- Version management
- Module search and discovery

## Supported Platforms

| Platform | Compilers | Library Extension |
|----------|-----------|-------------------|
| Linux | GCC, Clang | `.so` |
| macOS | Clang | `.dylib` |
| Windows | MSVC, MINGW64 | `.dll` |

## Quick Start

### Install Box

```bash
cd nt-box
cmake -B build && cmake --build build
sudo cmake --install build
```

### Install a Module

```bash
box install base64
```

### Use in Neutron

```neutron
use base64;
say(base64.encode("Hello!"));
```

### Build a Module

```bash
cd my-module
box build
```

## Documentation Structure

```
docs/
├── README.md                      # This file
├── BOX_GUIDE.md                   # Complete usage guide
├── COMMANDS.md                    # All commands with examples
├── MODULE_DEVELOPMENT.md          # C API and module creation
├── CROSS_PLATFORM.md              # Platform-specific notes
├── MINGW64_SUPPORT.md             # Windows GCC support
├── IMPLEMENTATION_MINGW64.md      # Technical implementation details
├── ARCHITECTURE.md                # Box internals
└── BUILD.md                       # Building Box from source
```

## Getting Help

- **Commands:** See [COMMANDS.md](COMMANDS.md)
- **Module Creation:** See [MODULE_DEVELOPMENT.md](MODULE_DEVELOPMENT.md)
- **Platform Issues:** See [CROSS_PLATFORM.md](CROSS_PLATFORM.md)
- **Windows/GCC:** See [MINGW64_SUPPORT.md](MINGW64_SUPPORT.md)
- **Issues:** https://github.com/yasakei/nt-box/issues

## Key Features

### Automatic Compiler Detection

Box detects your compiler automatically:

```bash
# Linux - uses g++ or clang++
box build

# macOS - uses clang++
box build

# Windows (Visual Studio) - uses cl
box build

# Windows (MINGW64) - uses g++
box build
```

### Version Management

Install specific versions:

```bash
box install base64@1.0.0
box install base64@1.0.1
```

### NUR Integration

Access modules from the Neutron Universe Registry:

```bash
box search json
box info websocket
box install http
```

### Local Installation

Modules install to `.box/modules/` in your project:

```
.box/
└── modules/
    └── base64/
        ├── base64.so      # or .dll/.dylib
        └── metadata.json
```

Neutron automatically finds and loads them with `use base64;`

## Common Tasks

### Search for Modules

```bash
box search              # List all
box search json         # Search by name
```

### Install Module

```bash
box install base64      # Latest version
box install base64@1.0.0  # Specific version
```

### List Installed

```bash
box list
```

### Remove Module

```bash
box remove base64
```

### Get Module Info

```bash
box info base64
```

### Build Module

```bash
cd my-module
box build
```

## Environment Variables

- `NEUTRON_HOME` - Neutron installation directory
- `BOX_MODULES_DIR` - Module installation directory (default: `.box/modules`)
- `BOX_REGISTRY_URL` - Custom NUR registry URL
- `MSYSTEM` - Windows: Forces MINGW64 mode if set

## Examples

### Example 1: Simple Module

```cpp
#include <neutron/capi.h>

extern "C" {

void hello(NeutronVM* vm, int argc, NeutronValue* args) {
    neutron_return(vm, neutron_new_string(vm, "Hello, World!"));
}

void neutron_module_init(NeutronVM* vm) {
    neutron_define_native(vm, "hello", hello);
}

}
```

### Example 2: Math Module

```cpp
#include <neutron/capi.h>
#include <cmath>

extern "C" {

void sqrt_func(NeutronVM* vm, int argc, NeutronValue* args) {
    if (argc != 1 || !neutron_is_number(args[0])) {
        neutron_error(vm, "Expected 1 number argument");
        return;
    }
    
    double value = neutron_get_number(args[0]);
    neutron_return(vm, neutron_new_number(vm, std::sqrt(value)));
}

void neutron_module_init(NeutronVM* vm) {
    neutron_define_native(vm, "sqrt", sqrt_func);
}

}
```

## Links

- **NUR Registry:** https://github.com/neutron-modules/nur
- **Neutron:** https://github.com/yasakei/neutron
- **C API Reference:** ../../include/capi.h

## Contributing

Contributions welcome! See the main Neutron repository for contribution guidelines.

## License

Box is part of the Neutron project and shares the same license. See LICENSE in the root directory.
