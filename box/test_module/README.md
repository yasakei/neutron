# Box Module Build Test

This directory contains a simple test module to demonstrate the `--build-box` command.

## Usage

To build this module, run:

```bash
./neutron --build-box test_module
```

Note: This command requires that the module follows the C++ module structure with a `native.cpp` file.

For pure Neutron modules (`.nt` files), simply place them in the `box/` directory and they can be imported directly.