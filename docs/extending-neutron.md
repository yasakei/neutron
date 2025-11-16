# Adding New Features and Modules to Neutron

This document explains how to easily extend the Neutron programming language with new features and modules.

## Adding New Features

To add a new feature to Neutron, place your source files in the `src/features/` directory. Then use the CMake function to add it:

```cmake
add_neutron_feature("MyFeature" "src/features/my_feature.cpp")
```

## Adding New Extensions

For more complex extensions, place your source files in the `src/extensions/` directory:

```cmake
add_neutron_extension("MyExtension" "src/extensions/my_extension.cpp")
```

## Adding New Utilities

For utility functions that can be shared across the codebase:

```cmake
add_neutron_utility("MyUtility" "src/utils/my_utility.cpp")
```

## Adding New Modules

To add a new built-in module to Neutron:

1. Create a new directory in `libs/` for your module:
   ```bash
   mkdir libs/my_module
   ```

2. Create the module implementation files:
   ```bash
   touch libs/my_module/native.cpp
   touch libs/my_module/native.h
   ```

3. Use the existing modules (like `libs/sys/`) as examples for the required structure.

4. The module will be automatically included in the build system.

## Example Module Structure

A typical module in the `libs/` directory should have:

- `native.h`: Contains function declarations and initialization function
- `native.cpp`: Contains implementation and proper module registration

For example, the initialization function should follow this pattern:

```cpp
extern "C" void neutron_init_MODULENAME_module(VM* vm) {
    auto module_env = std::make_shared<Environment>();
    register_MODULENAME_functions(module_env);
    auto module = new Module("MODULENAME", module_env);
    vm->define_module("MODULENAME", module);
}
```

## Build System Functions

The CMake build system provides helper functions to make adding new components easier:

- `add_neutron_module(name)` - For built-in modules in libs/
- `add_neutron_feature(name, path)` - For core language features
- `add_neutron_extension(name, path)` - For extensions
- `add_neutron_utility(name, path)` - For utility components

These functions will automatically add the files to the build, perform error checking, and provide feedback during the build process.