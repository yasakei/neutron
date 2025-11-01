# Neutron Language Reference

This document provides a comprehensive reference for the Neutron programming language. It covers syntax, data types, operators, control flow, functions, built-in functions, modules, classes, and all other features of the language.

## Comments

Comments in Neutron start with `//`. Everything from `//` to the end of the line is ignored by the interpreter.

```neutron
// This is a comment
var x = 10; // This is also a comment
```

## Data Types

Neutron is a dynamically typed language. It has the following built-in data types:

- **Number:** Represents floating-point numbers (doubles).
- **Boolean:** Represents `true` or `false`.
- **String:** Represents a sequence of characters.
- **Nil:** Represents the absence of a value.
- **Object:** Represents collections of key-value pairs.
- **Array:** Represents collections of values (via JSON arrays).
- **Function:** Represents callable functions.
- **Module:** Represents imported modules.

### Numbers

Numbers are represented as 64-bit floating-point values.

```neutron
var x = 10;      // An integer
var y = 3.14;    // A floating-point number
var z = -42;     // A negative number
```

### Booleans

Booleans are represented by the keywords `true` and `false`.

```neutron
var is_active = true;
var is_admin = false;
```

### Strings

Strings are sequences of characters enclosed in double quotes (`"`).

```neutron
var name = "Neutron";
var message = "Hello, world!";
var multiline = "Line 1\nLine 2\tTabbed";
```

#### String Interpolation

Neutron supports string interpolation using the `${}` syntax. You can embed expressions inside strings, and they will be automatically evaluated and converted to strings.

```neutron
var name = "Alice";
var age = 30;
say("My name is ${name} and I am ${age} years old.");
// Output: My name is Alice and I am 30 years old.

// Works with any expression
var x = 10;
var y = 20;
say("The sum of ${x} and ${y} is ${x + y}");
// Output: The sum of 10 and 20 is 30

// Works with arrays and objects
var arr = [1, 2, 3];
say("Array: ${arr}");
// Output: Array: [1, 2, 3]

var obj = {"name": "Bob", "age": 25};
say("Object: ${obj}");
// Output: Object: {name:25}
```

### Nil

The `nil` keyword represents the absence of a value. It is similar to `null` in other languages.

```neutron
var x = nil;
```

## Object Literals

Object literals are collections of key-value pairs enclosed in curly braces (`{}`). Keys must be string literals, and values can be any valid expression.

```neutron
var person = {
  "name": "John Doe",
  "age": 30,
  "city": "New York"
};

// Access object properties
say(person["name"]); // John Doe

// Objects can be nested
var config = {
  "database": {
    "host": "localhost",
    "port": 5432
  },
  "api": {
    "version": "v1",
    "timeout": 30
  }
};
```

Object literals work seamlessly with the JSON module:

```neutron
use json;

var data = {
  "name": "John Doe",
  "age": 30,
  "city": "New York"
};

// Convert object to JSON string
var jsonString = json.stringify(data);
say(jsonString); // {"name":"John Doe","age":30,"city":"New York"}

// Parse JSON string back to object
var parsed = json.parse(jsonString);
say(parsed["name"]); // John Doe
```

## Variables

Variables are declared using the `var` keyword. They can be assigned a value when they are declared, or they can be assigned a value later.

```neutron
var x;          // Declare a variable without a value (it will be nil)
var y = 10;     // Declare a variable with a value
x = 20;         // Assign a new value to x

// Variables are dynamically typed
var value = 42;    // Number
value = "hello";   // Now a string
value = true;      // Now a boolean
```

### Type Annotations (Optional)

Neutron supports optional type annotations for variables. These annotations are checked at compile-time but are not enforced at runtime in the current version.

```neutron
var int x = 42;                    // Integer type annotation
var string name = "Alice";         // String type annotation
var bool isActive = true;          // Boolean type annotation
var array numbers = [1, 2, 3];     // Array type annotation
var object person = {"name": "Bob"}; // Object type annotation
var any value = "anything";        // Any type (no restriction)
```

**Note:** Type annotations provide documentation and compile-time checking for initial assignments, but the variables can still be reassigned to values of different types at runtime since Neutron is dynamically typed.

## Operators

Neutron supports a variety of operators for arithmetic, comparison, logical operations, and string manipulation.

### Arithmetic Operators

- `+`: Addition (also string concatenation)
- `-`: Subtraction
- `*`: Multiplication
- `/`: Division
- `%`: Modulo (remainder)
- `++`: Increment (prefix or postfix)
- `--`: Decrement (prefix or postfix)

```neutron
var x = 10 + 5;  // 15
var y = 20 - 10; // 10
var z = 5 * 2;   // 10
var w = 10 / 2;  // 5
var r = 10 % 3;  // 1

// String concatenation
var greeting = "Hello, " + "world!"; // "Hello, world!"
var message = "Value: " + 42;        // "Value: 42"
```

#### Increment and Decrement Operators

Neutron supports C-style increment (`++`) and decrement (`--`) operators in both prefix and postfix forms:

```neutron
var counter = 0;

// Prefix increment: increment then use value
++counter;  // counter is now 1
say(counter);  // 1

// Postfix increment: use value then increment
counter++;  // counter is now 2
say(counter);  // 2

// Prefix decrement
--counter;  // counter is now 1

// Postfix decrement
counter--;  // counter is now 0

// Common usage in loops
var i = 0;
while (i < 5) {
    say(i);
    i++;  // Increment i
}
```

**Note:** In the current implementation, both prefix and postfix forms have the same behavior (they modify the variable and return the new value). This is a known limitation that may be addressed in future versions.

### Comparison Operators

- `==`: Equal
- `!=`: Not equal
- `<`: Less than
- `<=`: Less than or equal to
- `>`: Greater than
- `>=`: Greater than or equal to

```python
say(10 == 10); // true
say(10 != 5);  // true
say(5 < 10);   // true
```

**Note on Chained Comparisons:**
Currently, the Neutron language does not fully support chained comparison operators in the form `a < b < c`. 
While the parser may accept such expressions, they do not behave as expected (i.e., not equivalent to `(a < b) and (b < c)`).

This is a known limitation that requires more sophisticated bytecode generation to implement properly with single evaluation semantics.

As a workaround, use explicit comparisons joined with logical operators:

```neutron
// Instead of: a < b < c  (currently not working properly)
// Use:        a < b and b < c  (recommended)
```

### Logical Operators

- `and` / `&&`: Logical AND
- `or` / `||`: Logical OR
- `!`: Logical NOT

```neutron
say(true and false); // false
say(true or false);  // true
say(!true);          // false

// Symbol operators (equivalent to keyword operators)
say(true && false);  // false
say(true || false);  // true
```

Both keyword forms (`and`, `or`) and symbol forms (`&&`, `||`) are supported and behave identically.

## Control Flow

Neutron provides several control flow statements.

### If-Else Statements

The `if` statement executes a block of code if a condition is true. The `else` statement can be used to execute a block of code if the condition is false.

```python
if (x > 10) {
    say("x is greater than 10");
} else {
    say("x is not greater than 10");
}
```

### While Loops

The `while` loop executes a block of code as long as a condition is true.

```python
var i = 0;
while (i < 5) {
    say(i);
    i = i + 1;
}
```

### For Loops

The `for` loop is a more convenient way to write loops with a clear initializer, condition, and increment.

```python
for (var i = 0; i < 5; i = i + 1) {
    say(i);
}
```

### Match Statements

The `match` statement provides pattern matching for cleaner conditional logic. It evaluates an expression and executes the code block corresponding to the matching case.

```neutron
var day = 3;
match (day) {
    case 1 => say("Monday");
    case 2 => say("Tuesday");
    case 3 => say("Wednesday");
    default => say("Other day");
}
```

Match statements can also use block statements:

```neutron
var status = 2;
match (status) {
    case 1 => {
        say("Status is 1");
        say("Ready");
    }
    case 2 => {
        say("Status is 2");
        say("Running");
    }
    default => say("Unknown status");
}
```

The `default` case is optional. If no case matches and there's no default, execution continues normally.

```neutron
// Match with strings
var command = "start";
match (command) {
    case "start" => say("Starting...");
    case "stop" => say("Stopping...");
    case "restart" => say("Restarting...");
}
```

## Functions

Functions are defined using the `fun` keyword.

### Defining Functions

```python
fun greet(name) {
    say("Hello, " + name + "!");
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
say(result); // 15
```

Functions can also return without a value:

```python
fun print_message(message) {
    if (message == "") {
        return; // Returns nil
    }
    say(message);
}
```

### Lambda Functions (Anonymous Functions)

Lambda functions allow you to create anonymous functions inline. They are defined using the `fun` keyword with parameters and a body, but without a function name.

```neutron
// Lambda assigned to a variable
var add = fun(a, b) {
    return a + b;
};
say(add(5, 3));  // 8
```

Lambdas with single parameter:

```neutron
var square = fun(x) {
    return x * x;
};
say(square(4));  // 16
```

Lambdas with no parameters:

```neutron
var greet = fun() {
    return "Hello!";
};
say(greet());  // Hello!
```

Lambdas can be stored in arrays:

```neutron
var operations = [
    fun(x) { return x + 10; },
    fun(x) { return x * 2; },
    fun(x) { return x * x; }
];
say(operations[0](5));  // 15
say(operations[1](5));  // 10
say(operations[2](5));  // 25
```

Lambdas can be passed as arguments:

```neutron
fun applyTwice(f, value) {
    return f(f(value));
}
var double = fun(x) { return x * 2; };
say(applyTwice(double, 5));  // 20
```

Immediately invoked lambdas:

```neutron
var result = fun(x) { return x + 1; }(5);
say(result);  // 6
```

## Built-in Functions

Neutron provides several built-in functions that are available in the global scope without needing to import any modules.

### Output Functions

- `say(value)`: Prints a value to the console followed by a newline.

```neutron
say("Hello, world!");           // Output: Hello, world!
say(42);                       // Output: 42
say(true);                     // Output: true
say(nil);                      // Output: 
```

### Type Conversion Functions

Neutron provides type conversion capabilities through the `fmt` module. These functions perform dynamic type conversion with appropriate error handling:

**Available Functions in `fmt` Module:**
- `fmt.to_str(value)`: Converts a value to its string representation.
- `fmt.to_int(value)`: Converts a value to an integer.
- `fmt.to_float(value)`: Converts a value to a float.
- `fmt.to_bin(value)`: Converts a value to its binary string representation.
- `fmt.type(value)`: Returns the type of the value as a string.

```neutron
use fmt;

var num = 42;
var text = fmt.to_str(num);           // "42"
say("Number as string: " + text);

var numberText = "123";
var number = fmt.to_int(numberText);  // 123
say("String as number: " + number);

// Type detection
var type = fmt.type(42);              // "number"
say("Type of 42: " + type);

// Float conversion
var floatNum = fmt.to_float("3.14");  // 3.14
say("Float: " + floatNum);

// Binary conversion
var binary = fmt.to_bin(10);          // "1010"
say("10 in binary: " + binary);
```

#### String Manipulation

When working with strings, Neutron provides methods that can be accessed on string values directly or through array operations (since strings can be treated as character arrays in certain contexts):

```neutron
// String length using array method (strings support array-like operations)
var text = "Hello";
var length = text.length;             // 5
say("Length of 'Hello': " + length);

// For character access, you can use array indexing
say("First character: " + text[0]);   // "H"
say("Last character: " + text[4]);    // "o"
```

**Note:** The `char_to_int`, `int_to_char`, `string_length`, and `string_get_char_at` functions are available in the `fmt` module rather than as global functions:

```neutron
use fmt;

// These functions are provided in the fmt module
// For character/ASCII operations, you can use fmt.to_int and fmt.to_str
var type = fmt.type("A");                   // "string"
var intVal = fmt.to_int(65.0);              // 65
var strVal = fmt.to_str("Hello");           // "Hello"

say("Type detection: " + type);
say("Converted int: " + intVal);
say("Converted string: " + strVal);
```

## Modules

Neutron supports two types of imports:

1. **Module imports** using `use modulename;` - for built-in and native modules
2. **File imports** using `using 'filename.nt';` - for importing other Neutron source files

**Important:** As of the latest version, all built-in modules use lazy loading and must be explicitly imported with `use modulename;` before use.

### Module Imports

The `use` statement is used to import built-in or native modules. Modules must be imported before they can be used.

```neutron
use sys;
use json;
use math;

// Now you can use these modules
sys.write("data.txt", "Hello!");
var data = json.parse('{"name": "John"}');
var result = math.sqrt(16);
```

**Built-in Modules:**
- `sys` - File I/O, directory operations, environment access, process control (fully implemented)
- `json` - JSON parsing and stringification
- `math` - Mathematical operations
- `fmt` - Type conversion and formatting utilities
- `arrays` - Array manipulation and utility functions
- `time` - Time and date functions
- `http` - HTTP client operations

**Module Loading:**
All modules use lazy loading - they're only initialized when you explicitly use `use modulename;`. This provides:
- Faster startup time
- Explicit dependencies
- Better memory management

**Error Messages:**
If you try to use a module without importing it, you'll get a helpful error:
```
Runtime error: Undefined variable 'sys'. Did you forget to import it? Use 'use sys;' at the top of your file.
```

### File Imports

The `using` statement is used to import other Neutron source files. All functions and variables defined in the imported file become available in the current scope.

```neutron
using 'utils.nt';
using 'lib/helpers.nt';

// Use functions from imported files
say(greet("World"));
```

**File Search Paths:**
Files are searched in the following order:
1. Current directory (`.`).
2. `lib/` directory
3. `.box/modules/` directory (for installed modules)

**Example:**

**utils.nt:**
```neutron
fun greet(name) {
    return "Hello, " + name + "!";
}

var VERSION = "1.0.0";
```

**main.nt:**
```neutron
using 'utils.nt';

say(greet("Neutron"));  // Output: Hello, Neutron!
say(VERSION);           // Output: 1.0.0
```

**Best Practices:**
- Import modules at the top of your file
- Use `use` for built-in modules
- Use `using` for your own `.nt` files
- Only import what you need

## Core Modules

Neutron provides several built-in modules that extend the language's capabilities.

### The `math` Module

The `math` module provides basic mathematical functions.

```neutron
use math;

var sum = math.add(5, 3);           // 8
var diff = math.subtract(10, 4);    // 6
var product = math.multiply(6, 7);  // 42
var quotient = math.divide(15, 3);  // 5
var power = math.pow(2, 3);         // 8
var root = math.sqrt(16);           // 4
var absolute = math.abs(-5);        // 5
```

**Available Functions:**
- `math.add(a, b)`: Addition
- `math.subtract(a, b)`: Subtraction
- `math.multiply(a, b)`: Multiplication
- `math.divide(a, b)`: Division (throws error on division by zero)
- `math.pow(base, exponent)`: Power/exponentiation
- `math.sqrt(number)`: Square root
- `math.abs(number)`: Absolute value

### The `sys` Module

The `sys` module provides system-level operations including file I/O, directory operations, environment access, and process control.

```neutron
use sys;

// File operations
sys.write("hello.txt", "Hello, world!");
var content = sys.read("hello.txt");
say(content); // "Hello, world!"

// Directory operations
sys.mkdir("mydir");
var exists = sys.exists("mydir");  // true
sys.rmdir("mydir");

// Environment and system info
var currentDir = sys.cwd();
var userHome = sys.env("HOME");
var systemInfo = sys.info();
```

**File System Functions:**
- `sys.read(path)`: Read entire file contents as string
- `sys.write(path, content)`: Write content to file (overwrites)
- `sys.append(path, content)`: Append content to file
- `sys.cp(source, destination)`: Copy file
- `sys.mv(source, destination)`: Move/rename file
- `sys.rm(path)`: Remove file
- `sys.mkdir(path)`: Create directory
- `sys.rmdir(path)`: Remove directory
- `sys.exists(path)`: Check if file/directory exists
- `sys.cwd()`: Get current working directory
- `sys.chdir(path)`: Change current directory

**Environment & System Functions:**
- `sys.env(name)`: Get environment variable value
- `sys.info()`: Get system information object
- `sys.args()`: Get command line arguments (returns array)
- `sys.input([prompt])`: Read line from stdin with optional prompt

**Process Control Functions:**
- `sys.exit([code])`: Exit program with optional exit code
- `sys.exec(command)`: Execute shell command and return output

### The `json` Module

The `json` module provides JSON parsing and serialization capabilities.

```neutron
use json;

// Create an object
var data = {
  "name": "Alice",
  "age": 30,
  "active": true
};

// Convert to JSON string
var jsonStr = json.stringify(data);
say(jsonStr); // {"name":"Alice","age":30,"active":true}

// Pretty print JSON
var prettyJson = json.stringify(data, true);
say(prettyJson);
/*
{
  "name": "Alice",
  "age": 30,
  "active": true
}
*/

// Parse JSON string
var parsed = json.parse(jsonStr);
say(parsed["name"]); // "Alice"

// Get specific property
var name = json.get(parsed, "name"); // "Alice"
```

**Available Functions:**
- `json.stringify(value, [pretty])`: Convert value to JSON string. Set `pretty` to `true` for formatted output
- `json.parse(jsonString)`: Parse JSON string into object/array
- `json.get(object, key)`: Get property value from object, returns `nil` if not found

### The `fmt` Module

The `fmt` module provides advanced type conversion and formatting utilities that must be imported before use.

```neutron
use fmt;

// Type conversion functions
var numStr = fmt.to_str(42);              // "42"
var intVal = fmt.to_int("123");           // 123
var floatVal = fmt.to_float("3.14");      // 3.14
var binVal = fmt.to_bin(10);              // "1010"
var typeStr = fmt.type(42);               // "number"

say("Number as string: " + numStr);
say("String as integer: " + intVal);
say("Float value: " + floatVal);
say("Binary representation: " + binVal);
say("Type: " + typeStr);
```

**Available Functions:**
- `fmt.to_str(value)`: Converts a value to its string representation
- `fmt.to_int(value)`: Converts a value to an integer
- `fmt.to_float(value)`: Converts a value to a float
- `fmt.to_bin(value)`: Converts a value to its binary string representation  
- `fmt.type(value)`: Returns the type of the value as a string

**Type Conversion Behavior:**
- `fmt.to_int()` converts numbers to integers (truncates decimal parts), strings to integers via parsing, booleans to 0/1, and nil to 0
- `fmt.to_str()` converts values to their string representation
- `fmt.to_float()` converts values to floating-point numbers
- `fmt.to_bin()` converts values to their binary representation
- `fmt.type()` returns type names like "number", "string", "bool", "array", "object", etc.

### The `arrays` Module

The `arrays` module provides comprehensive array manipulation and utility functions that must be imported before use.

```neutron
use arrays;

// Create and manipulate arrays
var arr = arrays.new();
arrays.push(arr, 1);
arrays.push(arr, 2);
arrays.push(arr, 3);

var length = arrays.length(arr);    // 3
var element = arrays.at(arr, 0);    // 1
var last = arrays.pop(arr);         // 3
arrays.set(arr, 0, 10);             // Set first element to 10

// Array operations
arrays.reverse(arr);
arrays.sort(arr);
var index = arrays.index_of(arr, 10); // Find index of value
var hasValue = arrays.contains(arr, 10); // Check if array contains value
var joined = arrays.join(arr, ", ");    // Join elements with separator
var sliced = arrays.slice(arr, 0, 2);   // Get subarray
```

**Available Functions:**
- `arrays.new()`: Creates a new empty array
- `arrays.length(array)`: Returns the length of the array
- `arrays.push(array, value)`: Adds element to the end of the array
- `arrays.pop(array)`: Removes and returns the last element
- `arrays.at(array, index)`: Returns element at the specified index
- `arrays.set(array, index, value)`: Sets element at the specified index
- `arrays.reverse(array)`: Reverses the array in place
- `arrays.sort(array)`: Sorts the array in place
- `arrays.index_of(array, value)`: Returns index of first occurrence
- `arrays.contains(array, value)`: Checks if array contains the value
- `arrays.join(array, separator)`: Joins elements into a string with separator
- `arrays.slice(array, start, end)`: Returns a subarray
- `arrays.clear(array)`: Removes all elements from the array
- `arrays.clone(array)`: Creates a shallow copy of the array
- `arrays.remove(array, value)`: Removes first occurrence of a value
- `arrays.remove_at(array, index)`: Removes element at specified index
- `arrays.to_string(array)`: Converts array to string representation
- `arrays.flat(array)`: Flattens nested arrays
- `arrays.fill(array, value, start, end)`: Fills array with value
- `arrays.range(start, end, step)`: Creates array of numbers in range
- `arrays.shuffle(array)`: Randomly shuffles the array elements

### The `http` Module

The `http` module provides HTTP client functionality for making web requests.

```neutron
use http;

// GET request
var response = http.get("https://api.example.com/data");
say("Status: " + response["status"]);
say("Body: " + response["body"]);

// POST request with data
var postData = json.stringify({"key": "value"});
var postResponse = http.post("https://api.example.com/submit", postData);

// Other HTTP methods
var putResponse = http.put("https://api.example.com/update", data);
var deleteResponse = http.delete("https://api.example.com/remove");
var headResponse = http.head("https://api.example.com/check");
var patchResponse = http.patch("https://api.example.com/partial", patchData);
```

**Available Functions:**
- `http.get(url, [headers])`: Make HTTP GET request
- `http.post(url, [data], [headers])`: Make HTTP POST request
- `http.put(url, [data], [headers])`: Make HTTP PUT request
- `http.delete(url, [headers])`: Make HTTP DELETE request
- `http.head(url, [headers])`: Make HTTP HEAD request
- `http.patch(url, [data], [headers])`: Make HTTP PATCH request

**Response Object:**
All HTTP functions return a response object with:
- `status`: HTTP status code (number)
- `body`: Response body (string)
- `headers`: Response headers object

### The `time` Module

The `time` module provides time-related functions for timestamps, formatting, and delays.

```neutron
use time;

// Get current timestamp in milliseconds
var now = time.now();
say("Current timestamp: " + now);

// Format timestamp as string
var formatted = time.format(now);
say("Formatted time: " + formatted); // 2024-01-15 14:30:25

// Custom format
var customFormat = time.format(now, "%Y-%m-%d");
say("Date only: " + customFormat); // 2024-01-15

// Sleep for 1000 milliseconds (1 second)
time.sleep(1000);
say("Waited 1 second");
```

**Available Functions:**
- `time.now()`: Get current timestamp in milliseconds since epoch
- `time.format(timestamp, [format])`: Format timestamp as string using strftime format
- `time.sleep(milliseconds)`: Sleep/pause execution for specified milliseconds

**Default Time Format:** `%Y-%m-%d %H:%M:%S` (e.g., "2024-01-15 14:30:25")

## Classes

Neutron supports object-oriented programming with classes, methods, and the `this` keyword.

### Defining Classes

Classes are defined using the `class` keyword. Classes can contain methods and properties.

```neutron
class Person {
    var name;
    var age;
    
    fun initialize(name, age) {
        this.name = name;
        this.age = age;
    }
    
    fun greet() {
        say("Hello, my name is " + this.name + " and I am " + this.age + " years old.");
    }
    
    fun setAge(newAge) {
        this.age = newAge;
    }
    
    fun getAge() {
        return this.age;
    }
}
```

### Creating Instances

To create an instance of a class, you call the class like a function.

```neutron
var person = Person();
person.initialize("Alice", 30);
person.greet(); // "Hello, my name is Alice and I am 30 years old."

// Update properties
person.setAge(31);
say("Age: " + person.getAge()); // "Age: 31"
```

### The `this` Keyword

The `this` keyword refers to the current instance of the class and is used to access instance properties and methods.

```neutron
class Counter {
    var count;
    
    fun initialize() {
        this.count = 0;
    }
    
    fun increment() {
        this.count = this.count + 1;
        return this.count;
    }
    
    fun decrement() {
        this.count = this.count - 1;
        return this.count;
    }
    
    fun getValue() {
        return this.count;
    }
}

var counter = Counter();
counter.initialize();
say(counter.increment()); // 1
say(counter.increment()); // 2
say(counter.getValue());  // 2
```

### Class Methods and Properties

Classes can have both methods (functions) and properties (variables). Properties are accessed using dot notation.

```neutron
class Rectangle {
    var width;
    var height;
    
    fun initialize(w, h) {
        this.width = w;
        this.height = h;
    }
    
    fun area() {
        return this.width * this.height;
    }
    
    fun perimeter() {
        return 2 * (this.width + this.height);
    }
    
    fun toString() {
        return "Rectangle(" + this.width + "x" + this.height + ")";
    }
}

var rect = Rectangle();
rect.initialize(5, 3);
say("Area: " + rect.area());         // "Area: 15"
say("Perimeter: " + rect.perimeter()); // "Perimeter: 16"
say(rect.toString());                // "Rectangle(5x3)"
```

## Control Flow Structures

### Conditional Statements

Neutron supports `if` and `else` statements for conditional execution.

```python
var score = 85;

if (score >= 90) {
    say("Grade: A");
} else if (score >= 80) {
    say("Grade: B");
} else if (score >= 70) {
    say("Grade: C");
} else if (score >= 60) {
    say("Grade: D");
} else {
    say("Grade: F");
}
```

### Exception Handling

Neutron provides comprehensive exception handling with `try`, `catch`, `finally`, and `throw` statements to handle errors gracefully and prevent program crashes.

**Basic Try-Catch:**
```neutron
try {
    // Code that might throw an exception
    var result = riskyOperation();
    say("Operation completed successfully: " + result);
} catch (error) {
    // Handle the exception
    say("An error occurred: " + error);
}
```

**Try-Catch-Finally:**
```neutron
var fileHandle = openFile("data.txt");
try {
    // Risky operation with file
    var content = readFile(fileHandle);
    processContent(content);
} catch (error) {
    // Handle specific error
    say("File operation failed: " + error);
} finally {
    // Always execute cleanup code
    closeFile(fileHandle);
    say("File handle closed");
}
```

**Try-Finally (cleanup only):**
```neutron
try {
    // Main operation
    performOperation();
} finally {
    // Cleanup code that always runs
    cleanupResources();
}
```

**Throwing Exceptions:**
```neutron
fun validateAge(age) {
    if (age < 0) {
        throw "Age cannot be negative";
    }
    if (age > 150) {
        throw "Age seems unrealistic: " + age;
    }
    return true;
}

try {
    validateAge(-5);
} catch (errorMsg) {
    say("Validation error: " + errorMsg);
}
```

**Exception Types:**
You can throw any value as an exception:
```neutron
throw "Error message";           // String exception
throw 404;                      // Number exception  
throw true;                     // Boolean exception
throw ["error", "details"];     // Array exception
throw {"code": 500, "msg": "Internal error"};  // Object exception
```

**Nested Exception Handling:**
```neutron
try {
    say("Outer try");
    try {
        say("Inner try");
        throw "Inner exception";
        say("This won't execute");
    } catch (innerError) {
        say("Caught in inner: " + innerError);
    }
    say("Back to outer after inner catch");
} catch (outerError) {
    say("Caught in outer: " + outerError);
} finally {
    say("Outer finally always runs");
}
```

**Exception Flow:**
- When an exception is thrown, the runtime searches for the nearest matching `catch` block
- If found, execution jumps to the `catch` block and the exception value is assigned to the catch variable
- The `finally` block runs regardless of whether an exception occurred
- If no `catch` block handles an exception, it propagates up the call stack
- If an exception reaches the top level without being caught, the program terminates with an error

### While Loops

The `while` loop executes a block of code as long as a condition is true.

```python
var i = 0;
while (i < 5) {
    say("Count: " + i);
    i = i + 1;
}

// Infinite loop with break condition
var running = true;
var counter = 0;
while (running) {
    say("Loop iteration: " + counter);
    counter = counter + 1;
    if (counter >= 3) {
        running = false;
    }
}
```

### For Loops

The `for` loop provides a more compact way to write loops with initialization, condition, and increment.

```python
for (var i = 0; i < 10; i = i + 1) {
    say("Number: " + i);
}

// Counting down
for (var j = 10; j > 0; j = j - 1) {
    say("Countdown: " + j);
}
```

## Advanced Features

### Module System

Neutron supports two types of modules:

1. **Neutron Modules** (`.nt` files): Written in Neutron language
2. **Native Modules** (`.so`/`.dll`/`.dylib` files): Written in C++ for performance or system integration

```python
// Import built-in modules
use sys;
use json;
use math;

// Import custom Neutron modules
use mymodule;  // Loads mymodule.nt

// Import native modules from Box
use base64;     // Loads from .box/modules/base64/
use websocket;  // Loads from .box/modules/websocket/
```

**Managing Native Modules:**

Use the Box package manager to install and manage native modules:

```sh
box install base64     # Install from NUR
box list              # List installed modules
box remove base64     # Remove a module
```

**Creating Native Modules:**

See the comprehensive Box documentation:
- [Module Development Guide](../nt-box/docs/MODULE_DEVELOPMENT.md)
- [Box Commands](../nt-box/docs/COMMANDS.md)
- [Cross-Platform Guide](../nt-box/docs/CROSS_PLATFORM.md)

### Binary Compilation

Neutron scripts can be compiled to standalone executables using the `-b` flag:

```bash
# Compile script to binary
./neutron -b script.nt

# Run the compiled binary
./script.nt.out
```

This creates optimized, standalone executables that don't require the Neutron interpreter to run.

### Error Handling

Neutron provides runtime error reporting with descriptive messages:

```python
// Division by zero
var result = 10 / 0;  // Runtime error: Division by zero.

// Undefined variable
say(undefinedVar);    // Runtime error: Undefined variable 'undefinedVar'.

// Invalid function call
var num = 42;
num();                // Runtime error: Only functions and classes are callable.
```

### Truthiness

Neutron follows these truthiness rules:
- `nil` is falsy
- `false` is falsy  
- Everything else is truthy (including `0`, empty strings, etc.)

```python
if (nil) {
    say("This won't print");
}

if (false) {
    say("This won't print either");
}

if (0) {
    say("This WILL print - 0 is truthy");
}

if ("" ) {
    say("This WILL print - empty string is truthy");
}
```

## Complete Example Programs

Here are some complete example programs demonstrating various Neutron features:

### File Processing with System Operations

```python
use sys;
use json;

// Create a simple data processing program
say("=== File Processing Demo ===");

// Create sample data
var data = {
  "users": [
    {"name": "Alice", "age": 30, "active": true},
    {"name": "Bob", "age": 25, "active": false}
  ],
  "timestamp": time.now()
};

// Write data to file
var jsonString = json.stringify(data, true);
sys.write("users.json", jsonString);
say("Data written to users.json");

// Read and process the file
if (sys.exists("users.json")) {
    var content = sys.read("users.json");
    var parsed = json.parse(content);
    
    say("Found " + string_length(parsed["users"]) + " users");
    
    // Process each user (simplified example)
    var users = parsed["users"];
    say("Active users:");
    // Note: Array iteration would require additional language features
}

// Cleanup
sys.rm("users.json");
say("Cleanup completed");
```

### Object-Oriented Calculator

```python
use math;

class Calculator {
    var history;
    
    fun initialize() {
        this.history = [];
    }
    
    fun add(a, b) {
        var result = math.add(a, b);
        this.recordOperation("add", a, b, result);
        return result;
    }
    
    fun multiply(a, b) {
        var result = math.multiply(a, b);
        this.recordOperation("multiply", a, b, result);
        return result;
    }
    
    fun power(base, exp) {
        var result = math.pow(base, exp);
        this.recordOperation("power", base, exp, result);
        return result;
    }
    
    fun recordOperation(op, a, b, result) {
        var record = op + "(" + a + ", " + b + ") = " + result;
        say("Calculated: " + record);
    }
}

// Usage
var calc = Calculator();
calc.initialize();

say("=== Calculator Demo ===");
var sum = calc.add(10, 5);        // 15
var product = calc.multiply(4, 7); // 28
var power = calc.power(2, 8);      // 256

say("Final results:");
say("Sum: " + sum);
say("Product: " + product);  
say("Power: " + power);
```

### Web API Client

```python
use http;
use json;
use sys;

class ApiClient {
    var baseUrl;
    
    fun initialize(url) {
        this.baseUrl = url;
    }
    
    fun get(endpoint) {
        var fullUrl = this.baseUrl + endpoint;
        say("Fetching: " + fullUrl);
        
        var response = http.get(fullUrl);
        say("Status: " + response["status"]);
        
        if (response["status"] == 200) {
            return json.parse(response["body"]);
        } else {
            say("Error: " + response["body"]);
            return nil;
        }
    }
    
    fun post(endpoint, data) {
        var fullUrl = this.baseUrl + endpoint;
        var jsonData = json.stringify(data);
        
        say("Posting to: " + fullUrl);
        var response = http.post(fullUrl, jsonData);
        
        return response;
    }
}

// Usage example (with mock responses since http module returns mock data)
var client = ApiClient();
client.initialize("https://api.example.com");

var userData = client.get("/users/1");
if (userData != nil) {
    say("User data retrieved successfully");
}

var newUser = {"name": "Charlie", "email": "charlie@example.com"};
var createResponse = client.post("/users", newUser);
say("Create response status: " + createResponse["status"]);
```

## Performance and Optimization

### Binary Compilation

For production use or performance-critical applications, compile your Neutron scripts to native binaries:

```bash
# Compile to optimized binary
./neutron -b myapp.nt

# Run the compiled binary (faster execution)
./myapp.nt.out
```

### Best Practices

1. **Use appropriate modules**: Import only the modules you need
2. **Minimize string operations**: String concatenation can be expensive in loops
3. **Leverage native functions**: Built-in functions are optimized
4. **Use binary compilation**: For production deployments
5. **Profile your code**: Use timing functions to identify bottlenecks

## Conclusion

Neutron provides a comprehensive programming environment with:

- **Simple, familiar syntax** similar to C/JavaScript
- **Dynamic typing** with strong runtime type checking  
- **Object-oriented programming** with classes and methods
- **Rich standard library** covering math, file I/O, JSON, HTTP, and time operations
- **Module system** supporting both Neutron and native C++ extensions
- **Binary compilation** for production deployment
- **Built-in functions** for common operations

The language is designed to be approachable for beginners while providing the power and flexibility needed for real-world applications. Whether you're building system utilities, web clients, data processing tools, or learning programming concepts, Neutron provides the tools you need.

For more information, see:
- [Box Modules Documentation](box_modules.md) - Creating native extensions
- [Binary Conversion Guide](binary_conversion.md) - Compilation and deployment
- [Known Issues](known_issues.md) - Current limitations and workarounds