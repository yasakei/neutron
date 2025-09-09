# Neutron

Neutron is a simple, modern, and lightweight interpreted programming language written in C++. It is designed to be easy to learn and use, with a clean and expressive syntax.

## Features

- **Dynamically Typed:** No need to declare the type of a variable.
- **C-like Syntax:** Familiar syntax for developers who have used C, C++, Java, or JavaScript.
- **Rich Standard Library:** A growing standard library with modules for math and data conversion.
- **Object-Oriented:** Supports classes, methods, and the `this` keyword.
- **Built-in Functions:** A set of useful built-in functions for common tasks.
- **Modular:** Supports modules for organizing code.

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

## Documentation

For a complete reference of the Neutron language, please see the [Language Reference](docs/language_reference.md).

## Credits

This project was created and developed by **yasakei**.
