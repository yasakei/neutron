# Neutron Binary Conversion Feature

## Overview

The Neutron binary conversion feature allows you to convert Neutron scripts into standalone executable files that can be run directly from the command line without needing the Neutron interpreter.

## Usage

To convert a Neutron script to a standalone executable:

```bash
./neutron -b script.nt [output.out]
```

If no output file is specified, it will create `script.nt.out`.

To run the generated executable:

```bash
./script.nt.out
```

## Supported Features

The binary conversion feature supports most core Neutron language features:

1. **Variables and Data Types**
   - Numbers, strings, booleans, and nil values
   - Variable declaration and assignment

2. **Arithmetic Operations**
   - Addition (+), subtraction (-), multiplication (*), division (/)

3. **String Operations**
   - String concatenation
   - Built-in string functions (string_length, string_get_char_at)

4. **Control Flow**
   - Conditional statements (if/else)
   - While loops

5. **Built-in Functions**
   - say() - Output to console
   - int() - Convert string to integer
   - str() - Convert number to string
   - bin_to_int() - Convert binary string to integer
   - int_to_bin() - Convert integer to binary string
   - char_to_int() - Convert character to ASCII value
   - int_to_char() - Convert ASCII value to character
   - string_get_char_at() - Get character at specific index in string
   - string_length() - Get length of string

## Limitations and Known Issues

### Function Definitions
### Function Definitions
Function definitions (`fun` keyword) are currently **not supported** in the binary conversion feature. This is due to incomplete implementation in the Neutron compiler:

- Function declarations are parsed but not compiled
- Function calls will result in runtime errors
- This is a limitation of the current Neutron interpreter implementation

Example that will NOT work:
```neutron
fun greet(name) {
    return "Hello, " + name + "!";
}
say(greet("World"));  // This will cause a runtime error
```

### Classes
Object-oriented features are not fully supported:

- Class declarations (`class` keyword) are parsed but not compiled
- Creating instances of classes will result in runtime errors

### Complex Language Features
Some advanced language features may not be fully supported:
- Lists/Arrays with complex operations
- Nested functions
- Closures
- Exception handling

### Special Characters
Some special characters in the source code may cause parsing issues. If you encounter "Unexpected character" errors, try simplifying your script or checking for unusual characters.

## Best Practices

1. **Test Your Scripts**: Always test your Neutron scripts with the regular interpreter before converting to binary to ensure they work correctly.

2. **Keep It Simple**: For best results, use simple scripts that rely on core language features rather than advanced functionality.

3. **Avoid External Dependencies**: Scripts that work standalone without external modules will have the best compatibility with the binary conversion feature.

4. **Check for Errors**: If the generated executable crashes, try running a simplified version of your script to identify problematic features.

## Troubleshooting

If your generated executable is not working:

1. **Check the source script**: Ensure it runs correctly with the regular Neutron interpreter
2. **Simplify the script**: Remove complex features and test incrementally
3. **Check for special characters**: Look for unusual characters that might cause parsing issues
4. **Verify dependencies**: Ensure all required libraries are available

## Example

Create a simple script (`hello.nt`):
```neutron
say("Hello, World!");
var x = 42;
say("The answer is: " + x);

// Simple conditional
if (x > 0) {
    say("x is positive");
}

// Simple loop
var i = 0;
while (i < 3) {
    say("Iteration: " + i);
    i = i + 1;
}

// String operations
var name = "Neutron";
say("Name: " + name);
say("Length: " + string_length(name));
say("First character: " + string_get_char_at(name, 0));
```

Convert to binary:
```bash
./neutron -b hello.nt hello.out
```

Run the executable:
```bash
./hello.out
```

Output:
```
Hello, World!
The answer is: 42
x is positive
Iteration: 0
Iteration: 1
Iteration: 2
Name: Neutron
Length: 7
First character: N
```

## Future Enhancements

To fully support all Neutron language features in binary conversion, the following enhancements would be needed:

1. **Complete Function Support**:
   - Implement `visitFunctionStmt` in the compiler
   - Add bytecode instructions for function definitions and calls
   - Implement proper function call stack management

2. **Module System Integration**:
   - Embed module code directly in generated executables
   - Implement dynamic module loading in standalone binaries

3. **Class/Object Support**:
   - Implement class declaration compilation
   - Add bytecode instructions for object creation and method calls

4. **Advanced Features**:
   - Implement list/array operations
   - Add support for closures and nested functions
   - Implement exception handling

These enhancements would require significant changes to the Neutron compiler and virtual machine to properly support all language features in standalone executables.