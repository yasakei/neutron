# Box Integration Specification for Neutron Projects

## Overview

Box package manager should integrate seamlessly with Neutron projects by automatically detecting `.quark` configuration files and installing modules into the project's local environment.

## Project Detection

Box should check for a `.quark` file in the current directory or parent directories to determine if it's running within a Neutron project context.

### Detection Algorithm

```
1. Start from current working directory
2. Check if .quark exists in current directory
3. If not found, move up to parent directory
4. Repeat until .quark is found or reached filesystem root
5. If .quark found → Use project-local installation
6. If no .quark found → Use global/user installation
```

## Installation Behavior

### Inside a Neutron Project (`.quark` detected)

When Box detects a `.quark` file:

- **Install Location**: `.box/modules/` relative to project root
- **Module Registration**: Update `.quark` file's `[dependencies]` section
- **Search Path**: Neutron runtime should check project's `.box/modules/` first
- **Isolation**: Each project has its own module versions

**Example:**
```bash
# User is in /home/user/my-app/ which has .quark
$ box install base64@1.2.0

# Box should:
# 1. Install to: /home/user/my-app/.box/modules/base64/
# 2. Update .quark:
#    [dependencies]
#    base64 = "1.2.0"
```

### Outside a Neutron Project (No `.quark`)

When Box doesn't detect a project:

- **Install Location**: User-global directory (e.g., `~/.neutron/modules/`)
- **Global Usage**: Modules available to all scripts
- **Fallback**: Neutron checks global modules if not found in project

## Module Resolution Order

Neutron should resolve `use <module>` statements in this order:

1. **Built-in modules** (sys, json, http, etc.)
2. **Project-local modules** (`.box/modules/` if in project)
3. **Global modules** (`~/.neutron/modules/` or system-wide)

## `.quark` Dependency Tracking

Box should automatically update the project's `.quark` file when installing/removing modules:

**Before:**
```ini
[project]
name = "my-app"
version = "1.0.0"
entry = "main.nt"

[dependencies]
```

**After `box install base64@1.2.0`:**
```ini
[project]
name = "my-app"
version = "1.0.0"
entry = "main.nt"

[dependencies]
base64 = "1.2.0"
```

## Commands with Project Context

### `box install <module>[@version]`

- In project: Install to `.box/modules/`, update `.quark`
- Outside: Install globally

### `box remove <module>`

- In project: Remove from `.box/modules/`, update `.quark`
- Outside: Remove from global location

### `box list`

- In project: Show project-local + global modules (mark which is which)
- Outside: Show only global modules

### `box update`

- In project: Update all modules listed in `.quark` dependencies
- Outside: Update all global modules

## Implementation Requirements for Box

### 1. Project Root Detection

```cpp
// Pseudo-code
string findProjectRoot(string currentPath) {
    path current = absolute(currentPath);
    while (true) {
        if (exists(current / ".quark")) {
            return current;
        }
        if (!current.hasParent()) {
            return "";  // Not in project
        }
        current = current.parent();
    }
}
```

### 2. Config File Parsing

Box needs to:
- Read `.quark` files
- Parse `[dependencies]` section
- Update dependency entries
- Preserve comments and formatting

### 3. Module Installation

```cpp
// Pseudo-code
void installModule(string moduleName, string version) {
    string projectRoot = findProjectRoot(getCwd());
    
    if (!projectRoot.empty()) {
        // Project-local installation
        string installPath = projectRoot + "/.box/modules/" + moduleName;
        downloadAndInstall(moduleName, version, installPath);
        updateQuarkDependencies(projectRoot + "/.quark", moduleName, version);
        cout << "Installed " << moduleName << "@" << version << " to project\n";
    } else {
        // Global installation
        string installPath = getUserHome() + "/.neutron/modules/" + moduleName;
        downloadAndInstall(moduleName, version, installPath);
        cout << "Installed " << moduleName << "@" << version << " globally\n";
    }
}
```

## User Experience

### Typical Workflow

```bash
# 1. Create project
./neutron init my-app
cd my-app

# 2. Install Box
./neutron install box
export PATH="$PWD/bin:$PATH"

# 3. Install modules (automatically goes to project)
box install base64
box install http-client@2.1.0

# 4. Modules are in .box/modules/ and tracked in .quark
```

### Project Structure After Installing Modules

```
my-app/
├── .quark           # Updated with dependencies
├── main.nt
├── bin/
│   └── box
├── build/
├── .box/
│   └── modules/
│       ├── base64/
│       │   ├── base64.so (or .dylib/.dll)
│       │   └── metadata.json
│       └── http-client/
│           ├── http-client.so
│           └── metadata.json
└── deps/
    └── nt-box/
```

### Updated `.quark`

```ini
[project]
name = "my-app"
version = "1.0.0"
entry = "main.nt"

[dependencies]
base64 = "1.2.0"
http-client = "2.1.0"
```

## Benefits

1. **Isolation** - Each project has its own module versions
2. **Reproducibility** - `.quark` tracks exact versions
3. **Portability** - Share project, others run `box install` to get dependencies
4. **No Conflicts** - Different projects can use different versions
5. **Simple Workflow** - Just run `box install` in project directory

## Neutron Runtime Changes Needed

The Neutron VM/module loader should:

1. **Check for project context** when loading modules
2. **Search `.box/modules/` first** if in a project
3. **Fall back to global** if module not found locally
4. **Support both paths** simultaneously

```cpp
// Pseudo-code for module loading
Module* loadModule(string moduleName) {
    // 1. Check built-in modules
    if (isBuiltIn(moduleName)) {
        return loadBuiltIn(moduleName);
    }
    
    // 2. Check project-local modules
    string projectRoot = findProjectRoot(getCurrentFile());
    if (!projectRoot.empty()) {
        string localPath = projectRoot + "/.box/modules/" + moduleName;
        if (exists(localPath)) {
            return loadFromPath(localPath);
        }
    }
    
    // 3. Check global modules
    string globalPath = getUserHome() + "/.neutron/modules/" + moduleName;
    if (exists(globalPath)) {
        return loadFromPath(globalPath);
    }
    
    // 4. Not found
    error("Module not found: " + moduleName);
}
```

## Migration Path

For existing Box installations without this feature:

1. Update Box to latest version with project detection
2. Run `box install` in existing projects to migrate modules
3. Old global installations continue to work as fallback

## Summary

This specification ensures Box integrates seamlessly with Neutron's project system, providing:

✅ Automatic project detection via `.quark`  
✅ Local module installation per project  
✅ Dependency tracking in `.quark`  
✅ Isolation between projects  
✅ Global fallback for non-project usage  
✅ Simple, intuitive workflow  

**Implementation Status**: 🚧 To be implemented in Box repository (yasakei/nt-box)
