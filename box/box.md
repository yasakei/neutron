# Box Modules

This directory contains box modules for the Neutron programming language.

## Available Modules

1. [test_module](test_module/) - Simple test module for demonstrating the build process

## Adding New Modules

To create a new box module:

1. For C++ modules: Create a new directory with your module name and implement using the C++ API
2. For pure Neutron modules: Create a `.nt` file directly in this directory
3. Include a README.md with documentation
4. Add your module to this list

## Building C++ Modules

To build a specific C++ box module, use the `--build-box` command:

```bash
./neutron --build-box {module_name}
```

This command will build the specified module from the `box/` directory and create a shared library that can be imported in Neutron code. The module must contain a `native.cpp` file with the appropriate C++ implementation.

Note: Currently, the `--build-box` command requires that the module follows the C++ module structure and that the Makefile is properly configured to build individual modules. For pure Neutron modules (`.nt` files), simply place them in the `box/` directory and they can be imported directly.

## Module Structure

Each C++ box module should contain:
- Implementation files (C++ source code)
- Header files
- native.cpp (Neutron integration)
- README.md (Documentation)
- LICENSE (License information)

Pure Neutron modules are simply `.nt` files placed directly in this directory.
