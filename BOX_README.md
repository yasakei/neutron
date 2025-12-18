# Box Package Manager for Neutron

The Box package manager is the official package manager for Neutron modules. It allows you to easily install, manage, and use external modules in your Neutron projects.

## Building the System

To build Neutron with the Box package manager:

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

This will build both the `neutron` executable and the `box` package manager executable.

## Using Box

### Initialize a Project
```bash
box init
```
Creates a `neutron.json` configuration file for your project.

### Install a Module
```bash
box install base64
```
This will:
1. Look for the `base64/` directory containing `native.cpp`
2. Build it as a shared library using CMake
3. Install it to `.box/modules/base64/`
4. Update your `neutron.json` to track the dependency

### List Installed Modules
```bash
box list
```

### Remove a Module
```bash
box remove base64
```

## Building Modules Directly

You can also build modules directly using the neutron executable:

```bash
neutron --build-box base64
```

This will build the `base64` module and place it in the appropriate location under `.box/modules/base64/`.

## Using Modules in Your Code

Once installed, you can use modules in your Neutron code:

```neutron
use base64;

var original = "Hello, World!";
var encoded = base64.encode(original);
var decoded = base64.decode(encoded);

say("Original: " + original);
say("Encoded: " + encoded);
say("Decoded: " + decoded);
```

## Module Directory Structure

The system creates this structure:
```
.project/
├── .box/
│   └── modules/
│       └── base64/
│           ├── base64.dll     # Windows
│           ├── libbase64.so   # Linux
│           └── libbase64.dylib # macOS
├── neutron.json
└── your_code.nt
```

## Creating Your Own Modules

To create your own module:

1. Create a directory with your module name
2. Add a `native.cpp` file with your module implementation
3. Use the Neutron C API to define native functions
4. Build and install using box

Example `mymodule/native.cpp`:
```cpp
#include <neutron.h>
#include <string>

// Native function example
NeutronValue* my_function(NeutronVM* vm, int argCount, NeutronValue** args) {
    return neutron_new_string(vm, "Hello from my module!", 21);
}

// Module initialization
extern "C" void neutron_module_init(NeutronVM* vm) {
    neutron_define_native(vm, "myFunction", my_function, 0);
}
```

Then install with: `box install mymodule`

## Cross-Platform Support

The system automatically handles platform-specific details:
- Windows: Builds `.dll` files
- Linux: Builds `.so` files  
- macOS: Builds `.dylib` files
- All use the same build and installation process
```