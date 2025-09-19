# Creating External Modules for Neutron

This guide explains how to create external, high-performance modules for the Neutron programming language using C or C++. These modules, called "Box Modules", are dynamically loaded at runtime, allowing you to extend the language with new functionality without modifying the core interpreter.

## 1. Module Structure

Each Box Module resides in its own directory inside the `box/` directory. The name of the directory becomes the name of the module.

For a module named `my_module`, the directory structure should be:

```
box/
└── my_module/
    └── native.cpp
```

-   `box/my_module/`: The root directory of your module.
-   `native.cpp`: The C++ source file containing the implementation of your module.

Alternatively, you can use C instead of C++ by naming your file `native.c`:

```
box/
└── my_module/
    └── native.c
```

-   `box/my_module/`: The root directory of your module.
-   `native.c`: The C source file containing the implementation of your module.

When using C, you'll still need to use C++ features for interacting with the Neutron VM, but your implementation file can have a `.c` extension.

## 2. Implementation

The `native.cpp` (or `native.c`) file is the heart of your module. It contains the code that will be compiled into a shared library.

### 2.1. Required Headers

At a minimum, you will need to include `vm.h`, which provides access to the Neutron Virtual Machine and its core data structures.

```cpp
#include "vm.h"
#include <vector> // For std::vector
```

Note: Even when using C for your module implementation, you'll still need to use C++ features like `std::vector` to interact with the Neutron VM.

### 2.2. The Initialization Function

Every Box Module must export a C function named `neutron_module_init`. This function is the entry point of your module and is called by the Neutron VM when the module is loaded.

The function receives a pointer to the `VM` instance, which you can use to register your module's native functions.

```cpp
extern "C" void neutron_module_init(neutron::VM* vm) {
    // Register your native functions here.
}
```
*The `extern "C"` is crucial to prevent C++ name mangling, ensuring that the Neutron VM can find the initialization function.*

### 2.3. Defining Native Functions

Native functions are functions that can be called from Neutron code. They must have the following signature:

```cpp
neutron::Value my_native_function(std::vector<neutron::Value> args);
```

-   It takes a `std::vector<neutron::Value>` as an argument, which contains the arguments passed from the Neutron script.
-   It returns a `neutron::Value`, which is the value that will be returned to the Neutron script.

Inside your `neutron_module_init` function, you can register your native function using the module environment:

```cpp
// Create a module environment
auto env = std::make_shared<neutron::Environment>();

// Register your native function
env->define("my_function_name", neutron::Value(new neutron::NativeFn(my_native_function, arity)));

// Create the module and register it in the VM's globals
auto module = new neutron::Module("my_module", env);
vm->define_module("my_module", module);
```

-   `"my_function_name"`: The name of the function as it will be called from Neutron.
-   `my_native_function`: A pointer to your function.
-   `arity`: The number of arguments your function expects (-1 for variable arguments).

### 2.4. Working with Neutron Values

The `neutron::Value` struct is the primary way to interact with data from the Neutron VM. It can hold different types of data, such as numbers, strings, booleans, etc.

Here's how to work with `Value` objects:

-   **Checking the type**:
    ```cpp
    if (args[0].type == neutron::ValueType::NUMBER) {
        // It's a number
    }
    if (args[0].type == neutron::ValueType::STRING) {
        // It's a string
    }
    ```
-   **Getting the value**:
    ```cpp
    double number = std::get<double>(args[0].as);
    std::string str = std::get<std::string>(args[0].as);
    bool boolean = std::get<bool>(args[0].as);
    ```
-   **Creating a return value**:
    ```cpp
    return neutron::Value(123.45); // Return a number
    return neutron::Value("hello"); // Return a string
    return neutron::Value(true);    // Return a boolean
    return neutron::Value();        // Return nil
    ```

## 3. Building the Module

To compile your module, you can use the `--build-box` command of the `neutron` executable:

```sh
./neutron --build-box my_module
```

This command will compile `box/my_module/native.cpp` (or `box/my_module/native.c`) into a shared library located at `box/my_module/my_module.so`.

## 4. Using the Module in Neutron

Once your module is built, you can use it in any Neutron script with the `use` statement:

```neutron
use my_module;

// Now you can call the native functions you defined
var result = my_function_name(arg1, arg2);
say(result);
```

## 5. Creating Modules with C

While Neutron's Box module system is designed to work with C++, you can also implement modules using C with some considerations:

1. Your implementation file should be named `native.c` instead of `native.cpp`
2. You'll still need to use C++ features like `std::vector` and `neutron::Value` to interact with the VM
3. The build system will compile your C file with a C++ compiler

Here's a simple example of a C module (`box/hello_c/native.c`):

```c
#include "vm.h"
#include <vector>
#include <string>

// The native function that returns a simple greeting
neutron::Value hello_world(std::vector<neutron::Value> args) {
    return neutron::Value(std::string("Hello, World from C module!"));
}

// The native function that takes a name and returns a personalized greeting
neutron::Value greet(std::vector<neutron::Value> args) {
    // Handle case where no arguments are provided
    if (args.size() == 0) {
        return neutron::Value(std::string("Hello, Anonymous!"));
    }
    
    // Handle case where argument is not a string
    if (args[0].type != neutron::ValueType::STRING) {
        return neutron::Value(std::string("Hello, Anonymous!"));
    }
    
    std::string name = std::get<std::string>(args[0].as);
    std::string greeting = "Hello, " + name + "!";
    return neutron::Value(std::string(greeting));
}

// The module initialization function
extern "C" void neutron_module_init(neutron::VM* vm) {
    // Create a module environment
    auto env = std::make_shared<neutron::Environment>();
    
    // Register our functions (-1 means variable number of arguments)
    env->define("hello_world", neutron::Value(new neutron::NativeFn(hello_world, 0)));
    env->define("greet", neutron::Value(new neutron::NativeFn(greet, -1)));
    
    // Create the module and register it in the VM's globals
    auto module = new neutron::Module("hello_c", env);
    vm->define_module("hello_c", module);
}
```

Note that while your implementation file uses a `.c` extension, you're still using C++ features like `std::vector` and `std::string` to interact with the Neutron VM.

Currently, the build system only supports C++ files by default. To build a C module, you'll need to:

1. Rename your file to `native.cpp` 
2. Or manually compile your C file with a C++ compiler:

```bash
g++ -std=c++17 -shared -fPIC -Iinclude -I. -Ilibs/json -Ilibs/http -Ilibs/time -Ilibs/sys -Ibox box/hello_c/native.c -o box/hello_c/hello_c.so
```

To use this C module in a Neutron script:

```neutron
use hello_c;

var greeting = hello_c.hello_world();
say(greeting);

var personal_greeting = hello_c.greet("C Developer");
say(personal_greeting);
```

## 7. Simple Example: A Hello World Module

Let's create a simple module named `hello` that provides basic greeting functions.

**1. Create the directory structure:**
```
box/
└── hello/
    └── native.cpp
```

**2. Write the C++ code (`box/hello/native.cpp`):**
```cpp
#include "vm.h"
#include <vector>
#include <string>

// The native function that returns a simple greeting
neutron::Value hello_world(std::vector<neutron::Value> args) {
    return neutron::Value(std::string("Hello, World from C module!"));
}

// The native function that takes a name and returns a personalized greeting
neutron::Value greet(std::vector<neutron::Value> args) {
    // Handle case where no arguments are provided
    if (args.size() == 0) {
        return neutron::Value(std::string("Hello, Anonymous!"));
    }
    
    // Handle case where argument is not a string
    if (args[0].type != neutron::ValueType::STRING) {
        return neutron::Value(std::string("Hello, Anonymous!"));
    }
    
    std::string name = std::get<std::string>(args[0].as);
    std::string greeting = "Hello, " + name + "!";
    return neutron::Value(std::string(greeting));
}

// The module initialization function
extern "C" void neutron_module_init(neutron::VM* vm) {
    // Create a module environment
    auto env = std::make_shared<neutron::Environment>();
    
    // Register our functions (-1 means variable number of arguments)
    env->define("hello_world", neutron::Value(new neutron::NativeFn(hello_world, 0)));
    env->define("greet", neutron::Value(new neutron::NativeFn(greet, -1)));
    
    // Create the module and register it in the VM's globals
    auto module = new neutron::Module("hello", env);
    vm->define_module("hello", module);
}
```

**3. Build the module:**
```sh
./neutron --build-box hello
```
This will create `box/hello/hello.so`.

**4. Use the module in a Neutron script (`test_hello.nt`):**
```neutron
use hello;

var greeting = hello.hello_world();
say(greeting);

var personal_greeting = hello.greet("Neutron Developer");
say(personal_greeting);
```

**5. Run the script:**
```sh
./neutron test_hello.nt
```

## 8. Best Practices

1. **Error Handling**: Always validate input arguments and handle errors gracefully.
2. **Memory Management**: Be careful with resource allocation and deallocation.
3. **Performance**: Use C++ for computationally intensive operations.
4. **Documentation**: Include clear comments and examples.
5. **Testing**: Create simple test scripts to verify functionality.