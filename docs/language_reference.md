# Neutron Language Reference

This document provides a reference for the Neutron programming language. It covers the syntax, data types, operators, control flow, functions, and other features of the language.

## Comments

Comments in Neutron start with `//`. Everything from `//` to the end of the line is ignored by the interpreter.

```python
// This is a comment
var x = 10; // This is also a comment
```

## Data Types

Neutron is a dynamically typed language. It has the following built-in data types:

- **Number:** Represents floating-point numbers (doubles).
- **Boolean:** Represents `true` or `false`.
- **String:** Represents a sequence of characters.
- **Nil:** Represents the absence of a value.

### Numbers

Numbers are represented as 64-bit floating-point values.

```python
var x = 10;      // An integer
var y = 3.14;    // A floating-point number
```

### Booleans

Booleans are represented by the keywords `true` and `false`.

```python
var is_active = true;
var is_admin = false;
```

### Strings

Strings are sequences of characters enclosed in double quotes (`"`) or single quotes (`'`).

```python
var name = "Neutron";
var message = 'Hello, world!';
```

### Nil

The `nil` keyword represents the absence of a value. It is similar to `null` in other languages.

```python
var x = nil;
```

## Object Literals

Object literals are collections of key-value pairs enclosed in curly braces (`{}`). Keys must be string literals, and values can be any valid expression.

```python
var person = {
  "name": "John Doe",
  "age": 30,
  "city": "New York"
};
```

Object literals can be used with the JSON module to serialize and deserialize data:

```python
use json;

var data = {
  "name": "John Doe",
  "age": 30,
  "city": "New York"
};

// Convert object to JSON string
var jsonString = json.stringify(data);

// Parse JSON string back to object
var parsed = json.parse(jsonString);
```

## Variables

Variables are declared using the `var` keyword. They can be assigned a value when they are declared, or they can be assigned a value later.

```python
var x;          // Declare a variable without a value (it will be nil)
var y = 10;     // Declare a variable with a value
x = 20;         // Assign a new value to x
```

## Operators

Neutron supports a variety of operators.

### Arithmetic Operators

- `+`: Addition
- `-`: Subtraction
- `*`: Multiplication
- `/`: Division

```python
var x = 10 + 5;  // 15
var y = 20 - 10; // 10
var z = 5 * 2;   // 10
var w = 10 / 2;  // 5
```

### Comparison Operators

- `==`: Equal
- `!=`: Not equal
- `<`: Less than
- `<=`: Less than or equal to
- `>`: Greater than
- `>=`: Greater than or equal to

```python
print(10 == 10); // true
print(10 != 5);  // true
print(5 < 10);   // true
```

### Logical Operators

- `and`: Logical AND
- `or`: Logical OR
- `!`: Logical NOT

```python
print(true and false); // false
print(true or false);  // true
print(!true);          // false
```

## Control Flow

Neutron provides several control flow statements.

### If-Else Statements

The `if` statement executes a block of code if a condition is true. The `else` statement can be used to execute a block of code if the condition is false.

```python
if (x > 10) {
    print("x is greater than 10");
} else {
    print("x is not greater than 10");
}
```

### While Loops *(Not yet implemented)*

*Note: While loops are currently under development and not yet available in Neutron.*

The `while` loop executes a block of code as long as a condition is true.

```python
var i = 0;
while (i < 5) {
    print(i);
    i = i + 1;
}
```

### For Loops *(Not yet implemented)*

*Note: For loops are currently under development and not yet available in Neutron.*

The `for` loop is a more convenient way to write loops with a clear initializer, condition, and increment.

```python
for (var i = 0; i < 5; i = i + 1) {
    print(i);
}
```

## Functions

Functions are defined using the `fun` keyword.

### Defining Functions

```python
fun greet(name) {
    print("Hello, " + name + "!");
}
```

### Calling Functions

```python
greet("Neutron"); // Prints "Hello, Neutron!"
```

### Return Statement

The `return` statement is used to return a value from a function. If no value is specified, the function returns `nil`.

```python
fun add(a, b) {
    return a + b;
}

var result = add(5, 10);
print(result); // 15
```

Functions can also return without a value:

```python
fun print_message(message) {
    if (message == "") {
        return; // Returns nil
    }
    print(message);
}
```

## Built-in Functions

Neutron provides several built-in functions that are available in the global scope.

- `say(value)`: Prints a value to the console.
- `int(string)`: Converts a string to an integer.
- `str(number)`: Converts a number to a string.
- `bin_to_int(string)`: Converts a binary string to an integer.
- `int_to_bin(number)`: Converts an integer to a binary string.
- `char_to_int(string)`: Converts a single-character string to its ASCII value.
- `int_to_char(number)`: Converts a number (ASCII value) to a single-character string.
- `string_get_char_at(string, number)`: Returns the character at the given index.
- `string_length(string)`: Returns the length of the string.

## Modules

Neutron supports modules, which allow you to organize your code into separate files.

### Using Modules

The `use` statement is used to import a module.

```python
use math;
```

### The `math` Module

The `math` module provides a set of mathematical functions.

- `math.add(a, b)`
- `math.subtract(a, b)`
- `math.multiply(a, b)`
- `math.divide(a, b)`
- `math.pow(a, b)`
- `math.sqrt(n)`
- `math.abs(n)`
- `math.sin(n)`
- `math.cos(n)`
- `math.tan(n)`
- `math.log(n)`
- `math.exp(n)`
- `math.ceil(n)`
- `math.floor(n)`
- `math.round(n)`

## The `sys` Module

The `sys` module provides system-level functions for interacting with the operating system and environment.

### Input/Output Functions

- `sys.input([prompt])`: Reads a line of input from the user. Optionally displays a prompt.

```python
use sys;

// Read input without a prompt
var name = sys.input();

// Read input with a prompt
var age = sys.input("Enter your age: ");
```

### File System Functions

- `sys.cp(source, destination)`: Copies a file from source to destination.
- `sys.mv(source, destination)`: Moves a file from source to destination.
- `sys.rm(path)`: Removes a file.
- `sys.mkdir(path)`: Creates a directory.
- `sys.rmdir(path)`: Removes a directory.
- `sys.exists(path)`: Checks if a file or directory exists.
- `sys.read(path)`: Reads the contents of a file.
- `sys.write(path, content)`: Writes content to a file.
- `sys.append(path, content)`: Appends content to a file.

### System Information Functions

- `sys.cwd()`: Returns the current working directory.
- `sys.chdir(path)`: Changes the current working directory.
- `sys.env(name)`: Returns the value of an environment variable.
- `sys.info()`: Returns system information.

### Process Control Functions

- `sys.exit([code])`: Exits the program with the given exit code (default is 0).
- `sys.exec(command)`: Executes a system command and returns the output and exit code.

## Operators

Neutron supports various operators for mathematical, comparison, and logical operations.

### Logical Operators

**Note:** There are known issues with the `||` (logical OR) and `&&` (logical AND) operators. Using these operators may cause parsing errors. As a workaround, use separate `if` statements for complex conditions. See [Known Issues](known_issues.md) for more details.

### Comparison Operators

- `==` Equal to
- `!=` Not equal to
- `<` Less than
- `>` Greater than
- `<=` Less than or equal to
- `>=` Greater than or equal to

## Classes

Neutron supports object-oriented programming with classes.

### Defining Classes

Classes are defined using the `class` keyword.

```python
class Person {
    fun greet() {
        print("Hello, I am a person.");
    }
}
```

### Creating Instances

To create an instance of a class, you call the class like a function.

```python
var person = Person();
person.greet(); // Prints "Hello, I am a person."
```

### The `this` Keyword

The `this` keyword is used to refer to the current instance of the class.

```python
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
person.greet(); // Prints "Hello, my name is Neutron"
```
