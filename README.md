# Neutron

Neutron is a simple, modern, and lightweight interpreted programming language written in C++. It is designed to be easy to learn and use, with a clean and expressive syntax.

## Features

- **Dynamically Typed:** No need to declare the type of a variable.
- **C-like Syntax:** Familiar syntax for developers who have used C, C++, Java, or JavaScript.
- **Rich Standard Library:** A growing standard library with modules for math, data conversion, and web development.
- **Object-Oriented:** Supports classes, methods, and the `this` keyword.
- **Built-in Functions:** A set of useful built-in functions for common tasks.
- **Modular:** Supports modules for organizing code.
- **REST API Framework:** The Nexus module provides tools for building REST APIs.

## Getting Started

### Prerequisites

To build and run Neutron, you will need a C++ compiler that supports C++17.

### Building the Project

To build the Neutron interpreter, simply run the `make` command in the root of the project directory:

```sh
make
```

This will create the `neutron` executable in the root directory.

### Running the Interpreter

You can run a Neutron script by passing the file path to the `neutron` executable:

```sh
./neutron examples/hello.nt
```

You can also start the REPL (Read-Eval-Print Loop) by running `neutron` without any arguments:

```sh
./neutron
```

### Binary Conversion

Neutron supports converting scripts to standalone executables:

```sh
./neutron -b script.nt [output.out]
```

This will compile the script and create a standalone executable that can be run directly. If no output file is specified, it will create `script.nt.out`.

To run the generated executable:

```sh
./script.nt.out
```

The generated executable contains the Neutron runtime and the embedded source code, making it a fully standalone program that can be distributed and run on any compatible system.

**Note:** See [Binary Conversion Documentation](docs/binary_conversion.md) for detailed information about supported features, limitations, and best practices.

## Examples

Here are a few examples of what you can do with Neutron:

**Hello, World!**

```neutron
print("Hello, world!");
```

**Variables and Functions**

```neutron
fun greet(name) {
    return "Hello, " + name + "!";
}

var message = greet("Neutron");
print(message);
```

**Classes**

```neutron
class Person {
    var name;

    fun setName(name) {
        this.name = name;
    }

    fun greet() {
        print("Hello, my name is " + this.name);
    }
}

var person = Person();
person.setName("Neutron");
person.greet();
```

## Benchmarks

The project includes a set of benchmark scripts in the `benchmark/` directory to test the performance of various aspects of the language:

- Arithmetic operations
- String operations
- Variable assignments
- Function calls
- Conditional statements
- Time module functions

To run all benchmarks:

```sh
./neutron benchmark/run_all.nt
```

See [Benchmark Documentation](benchmark/README.md) for more details.

## Documentation

For a complete reference of the Neutron language, please see the [Language Reference](docs/language_reference.md).

For known issues and limitations, please see the [Known Issues](docs/known_issues.md) documentation.

### Modules

Neutron includes several built-in modules:

- **[Sys Module](docs/sys_module.md)** - File operations and system utilities
- **[HTTP Module](docs/http_module.md)** - HTTP client functionality
- **[JSON Module](docs/json_module.md)** - JSON parsing and generation
- **[Time Module](docs/time_module.md)** - Time and date functions
- **[Convert Module](docs/convert_module.md)** - Data type conversion utilities
- **[Nexus Module](docs/nexus_module.md)** - REST API framework for building web services

Examples of module usage can be found in the [examples directory](examples).

### External Modules

Neutron supports external modules that can be built using the `--build-box` command:

```sh
./neutron --build-box {module_name}
```

This command will build the specified C++ module from the `box/` directory and create a shared library that can be imported in Neutron code. The module must contain a `native.cpp` file with the appropriate C++ implementation.

Note: Currently, the `--build-box` command requires that the module follows the C++ module structure and that the Makefile is properly configured to build individual modules. For pure Neutron modules (`.nt` files), simply place them in the `box/` directory and they can be imported directly.

## Credits

This project was created and developed by **yasakei**.