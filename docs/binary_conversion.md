# Neutron Binary Conversion

## Overview

The Neutron binary conversion feature allows you to compile your Neutron scripts (`.nt` files) into standalone executable files. This is useful for distributing your programs without requiring the end-user to have the Neutron interpreter.

## Usage

To convert a Neutron script to a standalone executable, use the `-b` flag:

```bash
./neutron -b your_script.nt [output_name]
```

-   `your_script.nt`: The Neutron script to compile.
-   `[output_name]`: (Optional) The name of the output executable. If not provided, it defaults to `your_script.nt.out`.

To run the generated executable:

```bash
./your_script.nt.out
```

## Supported Features

The binary conversion feature now supports all core Neutron language features, including:

-   **Variables and Data Types:** Numbers, strings, booleans, nil.
-   **Operators:** Arithmetic, comparison, and logical operators.
-   **Control Flow:** `if/else` statements, `while` loops, and `for` loops.
-   **Functions:** User-defined functions created with the `fun` keyword are fully supported.
-   **Built-in Functions:** All standard functions like `say()`, `int()`, `str()`, etc.
-   **Modules:** Both C++ and Neutron modules are supported (see module details below).

## Modules and Binary Conversion

### Neutron Modules (`.nt` files)

Neutron modules are fully supported in binary conversion. When you compile a script that uses a Neutron module, the source code of the module is embedded directly into the final executable. This makes for a truly standalone binary.

`my_lib.nt`:
```neutron
fun say_hello() {
    say("Hello from the module!");
}
```

`main.nt`:
```neutron
use my_lib;
my_lib.say_hello();
```

Compiling `main.nt` will produce a single executable that includes the code from `my_lib.nt`.

### C++ Modules (`.so` files)

C++ modules are **not** embedded into the binary. The compiled executable will still depend on the `.so` files being present in the `box/` directory at runtime.

If you distribute a binary that uses a C++ module, you must also distribute the corresponding `.so` file and ensure it is placed in the correct `box/` directory relative to the executable.

## Example

1.  **Create a script (`hello.nt`):**
    ```neutron
    fun greet(name) {
        return "Hello, " + name + "!";
    }

    say(greet("World"));
    ```

2.  **Convert to binary:**
    ```bash
    ./neutron -b hello.nt
    ```
    This creates `hello.nt.out`.

3.  **Run the executable:**
    ```bash
    ./hello.nt.out
    ```

    **Output:**
    ```
    Hello, World!
    ```

This demonstrates that user-defined functions are correctly compiled and executed in the standalone binary.