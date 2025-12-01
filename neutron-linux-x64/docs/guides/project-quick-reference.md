# Neutron Project System - Quick Reference

## Commands

### `./neutron init [project-name]`
Initialize a new Neutron project with `.quark` configuration

**Example:**
```bash
./neutron init my-app
```

### `./neutron run`
Run the project's entry point (must be in a project directory)

**Example:**
```bash
./neutron run
```

### `./neutron build`
Build the project to a native executable in `build/` directory

**Example:**
```bash
./neutron build
# Creates: build/my-app (or build/my-app.exe on Windows)
```

### `./neutron install box`
Install Box package manager into `deps/` directory and add to `bin/`

**Example:**
```bash
./neutron install box
export PATH="$PWD/bin:$PATH"  # Add bin to PATH
box install <module>           # Box detects .quark, installs to .box/modules/
```

## .quark Configuration

```ini
[project]
name = "my-app"
version = "1.0.0"
entry = "main.nt"
author = "Your Name"
description = "A Neutron project"

[dependencies]
# Box modules listed here
```

## Project Structure

```
my-app/
├── .quark           # Project configuration
├── main.nt          # Entry point
├── .gitignore       # Git ignore
├── bin/             # Local tools (box)
├── build/           # Compiled executables
├── .box/            # Box modules
│   └── modules/
└── deps/            # Dependencies (Box)
    └── nt-box/
```

## Workflow

```bash
# 1. Create project
./neutron init calculator

# 2. Edit code
nano main.nt

# 3. Test
./neutron run

# 4. Build
./neutron build

# 5. Run executable
./build/calculator
```

## Multi-File Projects

**main.nt:**
```js
using 'utils.nt';

greet("World");
```

**utils.nt:**
```js
fun greet(name) {
    say("Hello, ${name}!");
}
```

## Backward Compatibility

All existing commands still work:

```bash
# Direct script execution
./neutron script.nt

# REPL
./neutron

# Legacy binary compilation
./neutron -b script.nt output
```

For detailed documentation, see [Project System Guide](project-system.md).
