# Creating External Modules for Neutron

This guide explains how to create external, high-performance modules for the Neutron programming language using C++. These modules, called "Box Modules", are dynamically loaded at runtime, allowing you to extend the language with new functionality without modifying the core interpreter.

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

## 2. C++ Implementation

The `native.cpp` file is the heart of your module. It contains the C++ code that will be compiled into a shared library.

### 2.1. Required Headers

At a minimum, you will need to include `vm.h`, which provides access to the Neutron Virtual Machine and its core data structures.

```cpp
#include "vm.h"
#include <vector> // For std::vector
```

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

Native functions are C++ functions that can be called from Neutron code. They must have the following signature:

```cpp
neutron::Value my_native_function(std::vector<neutron::Value> args);
```

-   It takes a `std::vector<neutron::Value>` as an argument, which contains the arguments passed from the Neutron script.
-   It returns a `neutron::Value`, which is the value that will be returned to the Neutron script.

Inside your `neutron_module_init` function, you can register your native function using the `vm->define_native()` method:

```cpp
vm->define_native("my_function_name", my_native_function, arity);
```

-   `"my_function_name"`: The name of the function as it will be called from Neutron.
-   `my_native_function`: A pointer to your C++ function.
-   `arity`: The number of arguments your function expects.

### 2.4. Working with Neutron Values

The `neutron::Value` struct is the primary way to interact with data from the Neutron VM. It can hold different types of data, such as numbers, strings, booleans, etc.

Here's how to work with `Value` objects:

-   **Checking the type**:
    ```cpp
    if (args[0].type == neutron::ValueType::NUMBER) {
        // It's a number
    }
    ```
-   **Getting the value**:
    ```cpp
    double number = std::get<double>(args[0].as);
    std::string str = std::get<std::string>(args[1].as);
    ```
-   **Creating a return value**:
    ```cpp
    return neutron::Value(123.45); // Return a number
    return neutron::Value("hello"); // Return a string
    return neutron::Value(); // Return nil
    ```

## 3. Building the Module

To compile your module, you can use the `--build-box` command of the `neutron` executable:

```sh
./neutron --build-box my_module
```

This command will compile `box/my_module/native.cpp` into a shared library located at `box/my_module/my_module.so`.

## 4. Using the Module in Neutron

Once your module is built, you can use it in any Neutron script with the `use` statement:

```neutron
use my_module;

// Now you can call the native functions you defined
var result = my_function_name(arg1, arg2);
print(result);
```

## 5. Complete Example: A `greeter` Module

Let's create a simple module named `greeter` that provides a function to greet a user.

**1. Create the directory structure:**
```
box/
└── greeter/
    └── native.cpp
```

**2. Write the C++ code (`box/greeter/native.cpp`):**
```cpp
#include "vm.h"
#include <vector>
#include <string>

// The native function that will be called from Neutron
neutron::Value greet(std::vector<neutron::Value> args) {
    if (args.size() != 1 || args[0].type != neutron::ValueType::STRING) {
        // Return nil if the arguments are incorrect
        return neutron::Value();
    }

    std::string name = std::get<std::string>(args[0].as);
    std::string greeting = "Hello, " + name + "!";

    return neutron::Value(greeting);
}

// The module initialization function
extern "C" void neutron_module_init(neutron::VM* vm) {
    vm->define_native("greet", greet, 1);
}
```

**3. Build the module:**
```sh
./neutron --build-box greeter
```
This will create `box/greeter/greeter.so`.

**4. Use the module in a Neutron script (`test_greeter.nt`):**
```neutron
use greeter;

var message = greet("Neutron Developer");
print(message); // Output: Hello, Neutron Developer!
```

**5. Run the script:**
```sh
./neutron test_greeter.nt
```
