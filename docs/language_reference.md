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

## Operators

Neutron supports a variety of operators for arithmetic, comparison, logical operations, and string manipulation.

### Arithmetic Operators

- `+`: Addition (also string concatenation)
- `-`: Subtraction
- `*`: Multiplication
- `/`: Division

```neutron
var x = 10 + 5;  // 15
var y = 20 - 10; // 10
var z = 5 * 2;   // 10
var w = 10 / 2;  // 5

// String concatenation
var greeting = "Hello, " + "world!"; // "Hello, world!"
var message = "Value: " + 42;        // "Value: 42"
```

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

### Logical Operators

- `and`: Logical AND
- `or`: Logical OR
- `!`: Logical NOT

```python
say(true and false); // false
say(true or false);  // true
say(!true);          // false
```

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

- `str(number)`: Converts a number to a string.
- `int(string)`: Converts a string to an integer.

```neutron
var num = 42;
var text = str(num);           // "42"
say("Number as string: " + text);

var numberText = "123";
var number = int(numberText);  // 123
say("String as number: " + number);
```

### Binary Conversion Functions

- `int_to_bin(number)`: Converts an integer to a binary string.
- `bin_to_int(string)`: Converts a binary string to an integer.

```neutron
var decimal = 10;
var binary = int_to_bin(decimal);    // "1010"
say("10 in binary: " + binary);

var binaryStr = "1111";
var decimalVal = bin_to_int(binaryStr); // 15
say("1111 in decimal: " + decimalVal);
```

### Character/ASCII Functions

- `char_to_int(string)`: Converts a single-character string to its ASCII value.
- `int_to_char(number)`: Converts a number (ASCII value) to a single-character string.

```neutron
var character = "A";
var ascii = char_to_int(character);   // 65
say("ASCII value of A: " + ascii);

var asciiCode = 65;
var char = int_to_char(asciiCode);    // "A"
say("Character for ASCII 65: " + char);
```

### String Manipulation Functions

- `string_length(string)`: Returns the length of the string.
- `string_get_char_at(string, index)`: Returns the character at the given index.

```neutron
var text = "Hello";
var length = string_length(text);     // 5
say("Length of 'Hello': " + length);

var firstChar = string_get_char_at(text, 0);  // "H"
var lastChar = string_get_char_at(text, 4);   // "o"
say("First character: " + firstChar);
say("Last character: " + lastChar);
```

## Modules

Neutron supports two types of imports:

1. **Module imports** using `use modulename;` - for built-in and native modules
2. **File imports** using `using 'filename.nt';` - for importing other Neutron source files

### Module Imports

The `use` statement is used to import built-in or native modules. Modules must be imported before they can be used.

```neutron
use math;
use sys;
use json;
```

**Built-in Modules:**
- `json` - JSON parsing and stringification
- `convert` - Type conversion utilities
- `time` - Time and date functions
- `http` - HTTP client operations
- `math` - Mathematical operations (auto-loaded)
- `sys` - System functions (auto-loaded)

**Important:** If you try to use a module without importing it, you'll get a helpful error:
```
Runtime error: Undefined variable 'json'. Did you forget to import it? Use 'use json;' at the top of your file.
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
1. Current directory (`.`)
2. `lib/` directory
3. `box/` directory

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

### The `convert` Module

The `convert` module provides advanced string and binary conversion utilities (extends the built-in conversion functions).

```neutron
use convert;

// These functions are also available globally
var ascii = convert.char_to_int("A");      // 65
var character = convert.int_to_char(65);   // "A"
var length = convert.string_length("hello"); // 5
var char = convert.string_get_char_at("hello", 1); // "e"

// Binary conversions
var binary = convert.int_to_bin(10);       // "1010"
var decimal = convert.bin_to_int("1010");  // 10
```

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

```neutron
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

### While Loops

The `while` loop executes a block of code as long as a condition is true.

```neutron
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

```neutron
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
2. **Native Modules** (`.so` files): Written in C++ for performance or system integration

```neutron
// Import built-in modules
use sys;
use json;
use math;

// Import custom Neutron modules
use mymodule;  // Loads mymodule.nt

// Import native modules (automatically loads .so files)
use custom_extension;  // Loads custom_extension.so from box/ directory
```

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

```neutron
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

```neutron
if (nil) {
    say("This won't print");
}

if (false) {
    say("This won't print either");
}

if (0) {
    say("This WILL print - 0 is truthy");
}

if ("") {
    say("This WILL print - empty string is truthy");
}
```

## Complete Example Programs

Here are some complete example programs demonstrating various Neutron features:

### File Processing with System Operations

```neutron
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

```neutron
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

```neutron
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
