# Minimal Box Module Example

This is a minimal example showing how to create a Box module for Neutron.

## Directory Structure
```
box/
└── hello/
    └── native.cpp
```

## Code (`box/hello/native.cpp`)
```cpp
#include "vm.h"
#include <vector>
#include <string>

// Simple function that returns a greeting
neutron::Value greet(std::vector<neutron::Value> args) {
    return neutron::Value(std::string("Hello from C!"));
}

// Module initialization function
extern "C" void neutron_module_init(neutron::VM* vm) {
    auto env = std::make_shared<neutron::Environment>();
    env->define("greet", neutron::Value(new neutron::NativeFn(greet, 0)));
    auto module = new neutron::Module("hello", env);
    vm->define_module("hello", module);
}
```

## Build
```bash
./neutron --build-box hello
```

## Use
```neutron
use hello;
say(hello.greet());
```