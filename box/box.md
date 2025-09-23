# Box Modules

This directory contains box modules for the Neutron programming language.

## Available Modules

1. [base64](base64/) - Base64 encoding and decoding (C++ implementation)
2. [base64_nt](base64_nt/) - Base64 encoding and decoding (C++ with Neutron patterns)

## Pure Neutron Modules

The following are examples of pure Neutron modules (implemented entirely in Neutron):

1. [simple.nt](simple.nt) - Simple test module
2. [simple_vars.nt](simple_vars.nt) - Simple module with variables
3. [math_nt.nt](math_nt.nt) - Math module
4. [test.nt](test.nt) - Simple test module

## Adding New Modules

To create a new box module:

1. For C++ modules: Create a new directory with your module name and implement using the C++ API
2. For pure Neutron modules: Create a `.nt` file directly in this directory
3. Include a README.md with documentation
4. Add your module to this list

## Building C++ Modules

To build all C++ box modules, use the shared_libs target:

```bash
make shared_libs
```

For example:
```bash
make shared_libs
```

This will create shared libraries (.so files) that are automatically loaded when the modules are imported in Neutron code.

Alternatively, you can manually build a specific module:

```bash
g++ -std=c++17 -Wall -Wextra -O2 -shared -fPIC -Iinclude -I. -Ilibs/json -Ilibs/http -Ilibs/time -Ilibs/sys -Ibox box/module_name/native.cpp -o box/module_name/module_name.so
```

## Module Structure

Each C++ box module should contain:
- Implementation files (C++ source code)
- Header files
- native.cpp (Neutron integration)
- README.md (Documentation)
- LICENSE (License information)

Pure Neutron modules are simply `.nt` files placed directly in this directory.