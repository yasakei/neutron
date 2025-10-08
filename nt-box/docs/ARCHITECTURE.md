# Box - Neutron Package Manager

**Box** is the official package manager for Neutron, designed to manage native modules from the Neutron User Repository (NUR).

## Overview

Box fetches and installs native modules (shared libraries) from the NUR registry at:
`https://raw.githubusercontent.com/neutron-modules/nur/refs/heads/main/nur.json`

Each module provides platform-specific binaries (.so, .dll, .dylib) that are loaded by the Neutron runtime.

## Architecture

```
Box
├── Fetches nur.json (registry index)
├── Downloads module metadata (e.g., base64.json)
├── Installs platform-specific binaries
└── Manages dependencies
```

## Commands

### Installation
```bash
box install <module>          # Install a module
box install base64            # Example: Install base64 module
box uninstall <module>        # Remove a module
box update <module>           # Update a module
box list                      # List installed modules
```

### Building Native Modules
```bash
box build native <module>     # Build native module for current platform
box build nt <module>         # Build Neutron source module (future)
```

### Information
```bash
box search <query>            # Search NUR for modules
box info <module>             # Show module information
box outdated                  # Check for outdated modules
```

## Module Structure

### NUR Registry (nur.json)
```json
{
  "version": "1.0",
  "modules": {
    "base64": "./modules/base64.json",
    "crypto": "./modules/crypto.json",
    "sqlite": "./modules/sqlite.json"
  }
}
```

### Module Manifest (base64.json)
```json
{
  "name": "base64",
  "version": "1.0.0",
  "entry-linux": "https://raw.githubusercontent.com/neutron-modules/base64/refs/heads/main/bin/1.0.1/base64.so",
  "entry-win": "https://raw.githubusercontent.com/neutron-modules/base64/refs/heads/main/bin/1.0.1/base64.dll",
  "entry-mac": "https://raw.githubusercontent.com/neutron-modules/base64/refs/heads/main/bin/1.0.1/base64.dylib",
  "deps": {
    "neutron": ">=1.0.0"
  }
}
```

## Installation Process

1. **Fetch NUR Index**: Download nur.json from registry
2. **Resolve Module**: Find module metadata URL
3. **Download Metadata**: Get module.json with platform-specific URLs
4. **Detect Platform**: Determine OS (Linux/Windows/macOS)
5. **Download Binary**: Fetch appropriate shared library
6. **Install**: Place in box modules directory
7. **Verify**: Check dependencies and compatibility

## Directory Structure

```
~/.box/                       # Box home directory
├── config.json              # Configuration
├── cache/                   # Cached downloads
│   └── nur.json
└── modules/                 # Installed modules
    ├── base64/
    │   ├── base64.so       # Linux
    │   ├── base64.dll      # Windows
    │   └── base64.dylib    # macOS
    └── crypto/
        └── crypto.so

<project>/box/               # Project-local modules
├── base64.so
└── crypto.so
```

## Cross-Platform Support

Box detects the platform and downloads the appropriate binary:

- **Linux**: `.so` (entry-linux)
- **Windows**: `.dll` (entry-win)
- **macOS**: `.dylib` (entry-mac)

Platform detection uses the same implementation as the Neutron runtime.

## Building Native Modules

The `box build native` command compiles native modules for the current platform:

```bash
box build native base64
```

This will:
1. Compile the C++ source code
2. Link against Neutron runtime
3. Create platform-specific shared library
4. Output to `bin/<version>/<module>.[so|dll|dylib]`

## Dependencies

Box automatically resolves and installs dependencies specified in `deps`:

```json
{
  "deps": {
    "neutron": ">=1.0.0",
    "crypto": "^2.0.0"
  }
}
```

## Configuration

Box configuration is stored in `~/.box/config.json`:

```json
{
  "registry": "https://raw.githubusercontent.com/neutron-modules/nur/refs/heads/main",
  "cache_dir": "~/.box/cache",
  "modules_dir": "~/.box/modules"
}
```

## Development Status

- ✅ Project structure created
- 🔄 NUR registry fetching
- 🔄 Platform detection
- 🔄 Module installation
- 🔄 Dependency resolution
- 🔄 Native module building
- ⏳ Neutron source modules (future)

## Integration with Neutron

When Neutron runtime loads a module with `use base64;`, it searches:
1. `./box/base64.[so|dll|dylib]` (project-local)
2. `~/.box/modules/base64/base64.[so|dll|dylib]` (global)

Box ensures modules are available in these locations.
