# Box Commands Reference

Complete reference for all Box package manager commands.

## Table of Contents

- [install](#install)
- [list](#list)
- [search](#search)
- [remove](#remove)
- [build](#build)
- [info](#info)

---

## install

Install a module from the Neutron Universe Registry (NUR).

### Syntax

```sh
box install <module>[@version]
```

### Parameters

- `<module>` - Name of the module to install
- `[@version]` - Optional version specifier (defaults to "latest")

### Examples

```sh
# Install latest version
box install base64

# Install specific version
box install base64@1.0.0

# Install multiple versions
box install base64@1.0.0
box install base64@1.0.1
```

### Behavior

1. Queries the NUR registry for module metadata
2. Downloads platform-specific binary (.so/.dll/.dylib)
3. Installs to `.box/modules/<module>/`
4. Creates `metadata.json` with version info
5. Module becomes available via `use <module>;` in Neutron

### Installation Directory

Modules are installed to `.box/modules/` in your project directory:

```
.box/
└── modules/
    └── base64/
        ├── base64.so          # Linux
        ├── base64.dll         # Windows
        ├── base64.dylib       # macOS
        └── metadata.json
```

### Exit Codes

- `0` - Success
- `1` - Module not found in registry
- `2` - Version not found
- `3` - Network error
- `4` - File system error

---

## list

List all installed modules in the current project.

### Syntax

```sh
box list
```

### Examples

```sh
box list
```

### Output Format

```
Installed modules in .box/modules/:

  base64          v1.0.1    Base64 encoding/decoding module
  json            v2.3.0    JSON parsing and serialization
  websocket       v1.2.4    WebSocket client library
```

Shows:
- Module name
- Installed version
- Short description from metadata

### Exit Codes

- `0` - Success (even if no modules installed)

---

## search

Search for modules in the NUR registry.

### Syntax

```sh
box search [query]
```

### Parameters

- `[query]` - Optional search term (searches all modules if omitted)

### Examples

```sh
# List all modules
box search

# Search for specific modules
box search base64
box search json
box search web
```

### Output Format

```
Available modules in NUR:

  base64          v1.0.1    Base64 encoding and decoding
  json            v2.3.0    JSON parser and serializer
  websocket       v1.2.4    WebSocket client implementation
  http            v3.1.0    HTTP client library
```

### Exit Codes

- `0` - Success
- `1` - Network error
- `2` - Registry unavailable

---

## remove

Remove an installed module from the current project.

### Syntax

```sh
box remove <module>
```

### Parameters

- `<module>` - Name of the module to remove

### Examples

```sh
box remove base64
```

### Behavior

1. Checks if module exists in `.box/modules/`
2. Removes entire module directory
3. Displays confirmation message

### Exit Codes

- `0` - Success
- `1` - Module not found
- `2` - File system error (permission denied, etc.)

---

## build

Build a native module from source code.

### Syntax

```sh
box build
```

### Requirements

Must be run from a directory containing:
- `module.json` - Module metadata file
- C++ source files

### module.json Format

```json
{
  "name": "mymodule",
  "version": "1.0.0",
  "description": "My custom module",
  "entry": "mymodule.cpp",
  "include_dirs": ["include"],
  "libraries": [],
  "compiler_flags": []
}
```

### Behavior

1. Reads `module.json` for build configuration
2. Detects platform and compiler:
   - **Windows:** MSVC `cl` compiler → `.dll`
   - **macOS:** `clang++` compiler → `.dylib`
   - **Linux:** `g++` or `clang++` compiler → `.so`
3. Compiles with appropriate flags:
   - Windows: `/std:c++17 /LD /MD /I...`
   - macOS: `-std=c++17 -dynamiclib -fPIC -I...`
   - Linux: `-std=c++17 -shared -fPIC -I...`
4. Outputs binary to `bin/<version>/` directory

### Examples

```sh
# Build current module
cd my-module/
box build

# Output
Building module: mymodule
Platform: Linux
Compiler: g++
Output: bin/v1.0.0/mymodule.so
Build successful!
```

### Exit Codes

- `0` - Build successful
- `1` - Missing module.json
- `2` - Compilation error
- `3` - Compiler not found

---

## info

Display detailed information about a module.

### Syntax

```sh
box info <module>
```

### Parameters

- `<module>` - Name of the module to query

### Examples

```sh
box info base64
```

### Output Format

```
Module: base64
Latest Version: 1.0.1
Description: Base64 encoding and decoding module

Available Versions:
  - 1.0.0 (2024-01-15)
  - 1.0.1 (2024-02-20) [latest]

Platform Binaries:
  Linux:   base64.so
  Windows: base64.dll
  macOS:   base64.dylib

Dependencies: none

Repository: https://github.com/neutron-modules/base64
```

### Exit Codes

- `0` - Success
- `1` - Module not found in registry
- `2` - Network error

---

## Environment Variables

### BOX_REGISTRY_URL

Override the default NUR registry URL.

```sh
export BOX_REGISTRY_URL="https://my-registry.com/nur.json"
box install mymodule
```

Default: `https://raw.githubusercontent.com/neutron-modules/nur/refs/heads/main/nur.json`

### BOX_MODULES_DIR

Override the default module installation directory.

```sh
export BOX_MODULES_DIR="/custom/path/modules"
box install base64
```

Default: `.box/modules`

---

## Exit Codes Summary

| Code | Meaning                    |
|------|----------------------------|
| 0    | Success                    |
| 1    | Not found / General error  |
| 2    | Network error              |
| 3    | File system error          |
| 4    | Compilation error          |

---

## See Also

- [Box Guide](BOX_GUIDE.md) - Comprehensive usage guide
- [Module Development](MODULE_DEVELOPMENT.md) - Creating native modules
- [Cross-Platform](CROSS_PLATFORM.md) - Platform-specific notes
