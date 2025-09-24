# Neutron Binary Conversion

## Overview

The Neutron binary conversion feature allows you to compile your Neutron scripts (`.nt` files) into standalone, optimized executable files. This provides several advantages:

- **Standalone Distribution**: No need for users to install the Neutron interpreter
- **Performance**: Compiled binaries may offer improved performance
- **Code Protection**: Source code is embedded and not easily readable
- **Simplified Deployment**: Single executable file for easy distribution

Binary conversion transforms your Neutron scripts into native executables that can run independently on the target system.

## Usage

### Basic Compilation

To convert a Neutron script to a standalone executable, use the `-b` flag:

```bash
./neutron -b your_script.nt [output_name]
```

-   `your_script.nt`: The Neutron script to compile.
-   `[output_name]`: (Optional) The name of the output executable. If not provided, it defaults to `your_script.nt.out`.

**Examples:**

```bash
# Compile with default output name
./neutron -b hello.nt
# Creates: hello.nt.out

# Compile with custom output name  
./neutron -b hello.nt hello_app
# Creates: hello_app

# Compile a complex application
./neutron -b myapp.nt
# Creates: myapp.nt.out
```

### Running Compiled Binaries

To run the generated executable:

```bash
./your_script.nt.out

# Or with custom name:
./hello_app
```

The executable runs independently without requiring the Neutron interpreter.

## Supported Features

The binary conversion feature supports all core Neutron language features:

### Core Language Features
-   **Variables and Data Types:** Numbers, strings, booleans, nil, objects, arrays
-   **Operators:** Arithmetic (`+`, `-`, `*`, `/`), comparison (`==`, `!=`, `<`, `>`, `<=`, `>=`), and logical (`and`, `or`, `!`) operators
-   **Control Flow:** `if/else` statements, `while` loops, and `for` loops with full conditional logic
-   **Functions:** User-defined functions with parameters, return values, and local scope
-   **Classes:** Object-oriented programming with classes, methods, properties, and the `this` keyword
-   **Object Literals:** JSON-style object creation and manipulation

### Built-in Functions
All built-in functions are fully supported in compiled binaries:

-   **Output:** `say(value)` for console output
-   **Type Conversion:** `str()`, `int()` for type conversion
-   **Binary Operations:** `int_to_bin()`, `bin_to_int()` for binary conversion
-   **String Operations:** `string_length()`, `string_get_char_at()`, `char_to_int()`, `int_to_char()`

### Module System
Both types of modules are supported with different behaviors:

-   **Neutron Modules (`.nt`):** Fully embedded in binary (see details below)
-   **Native Box Modules (`.so`):** External dependencies (see details below)

### Advanced Features
-   **Error Handling:** Runtime error reporting and stack traces
-   **Memory Management:** Automatic garbage collection
-   **Dynamic Typing:** Full type checking and conversion at runtime

## Modules and Binary Conversion

The Neutron module system has different behaviors depending on module type when compiling to binary.

### Built-in Modules

All built-in modules are automatically available in compiled binaries:

- `sys`: File I/O, system operations, environment access
- `json`: JSON parsing and serialization  
- `math`: Mathematical operations
- `http`: HTTP client functionality
- `time`: Time and date operations
- `convert`: String and binary conversion utilities

**Example:**
```neutron
use sys;
use json;
use math;

var data = {"result": math.pow(2, 8)};
var jsonStr = json.stringify(data);
sys.write("result.txt", jsonStr);
say("Calculation complete!");
```

This compiles to a fully standalone binary with all module functionality included.

### Neutron Modules (`.nt` files)

Neutron modules are fully embedded in the compiled binary. The module source code is included directly in the executable, making it truly standalone.

**my_utils.nt:**
```neutron
use math;
use sys;

fun calculate_fibonacci(n) {
    if (n <= 1) {
        return n;
    }
    return calculate_fibonacci(n - 1) + calculate_fibonacci(n - 2);
}

fun save_result(filename, value) {
    sys.write(filename, "Result: " + str(value));
    return true;
}

fun load_config(filename) {
    if (sys.exists(filename)) {
        return sys.read(filename);
    }
    return "default_config";
}
```

**main.nt:**
```neutron
use my_utils;

var fib_result = my_utils.calculate_fibonacci(10);
my_utils.save_result("output.txt", fib_result);
say("Fibonacci(10) = " + fib_result);
```

Compiling `main.nt` produces a single executable that includes all code from `my_utils.nt`.

### Native Box Modules (`.so` files)

Native C++ modules are **not** embedded into the binary. The compiled executable maintains external dependencies on `.so` files.

**Important Considerations:**
- The binary will look for `.so` files in the `box/` directory at runtime
- You must distribute the `.so` files alongside your binary
- The directory structure must be maintained: `box/module_name/module_name.so`

**Example with custom native module:**

If your script uses:
```neutron
use my_native_module;
var result = my_native_module.fast_calculation(1000);
```

You must distribute:
- Your compiled binary: `myapp.nt.out`
- The native module: `box/my_native_module/my_native_module.so`

**Distribution structure:**
```
myapp.nt.out
box/
└── my_native_module/
    └── my_native_module.so
```

## Compilation Process

### How Binary Conversion Works

The Neutron binary conversion process involves several steps:

1. **Source Code Analysis**: The Neutron compiler parses your script and identifies all dependencies
2. **Module Resolution**: All `use` statements are resolved and Neutron modules are loaded
3. **Code Generation**: The compiler generates C++ code that embeds your Neutron script
4. **Native Compilation**: The generated C++ code is compiled using `g++` with optimization flags
5. **Linking**: The final executable is linked with the Neutron runtime library

### Generated Files

During compilation, several temporary files are created:

- `your_script.nt.out_main.cpp`: Generated C++ wrapper code
- `your_script.nt.out`: Final executable binary

The temporary C++ file is usually cleaned up after successful compilation.

### Compilation Flags

The binary conversion uses these `g++` compilation flags:

```bash
g++ -std=c++17 -Wall -Wextra -O2 -Iinclude -I. -Ilibs/json -Ilibs/http -Ilibs/time -Ilibs/sys -Ibox -Ilibs/websocket [source files] -o [output] -ldl -pthread
```

- **`-std=c++17`**: Uses C++17 standard
- **`-O2`**: Enables optimization for performance
- **`-Wall -Wextra`**: Enables comprehensive warnings
- **`-ldl -pthread`**: Links dynamic loading and threading libraries

## Comprehensive Examples

### Example 1: Simple Function Script

**hello.nt:**
```neutron
fun greet(name) {
    return "Hello, " + name + "!";
}

fun main() {
    var user = "World";
    var message = greet(user);
    say(message);
    say("Compilation successful!");
}

main();
```

**Compilation and execution:**
```bash
./neutron -b hello.nt
./hello.nt.out
```

**Output:**
```
Hello, World!
Compilation successful!
```

### Example 2: Object-Oriented Application

**calculator.nt:**
```neutron
use math;

class Calculator {
    var history;
    
    fun initialize() {
        this.history = "Calculator initialized\n";
    }
    
    fun add(a, b) {
        var result = math.add(a, b);
        this.history = this.history + "Added " + a + " + " + b + " = " + result + "\n";
        return result;
    }
    
    fun power(base, exp) {
        var result = math.pow(base, exp);
        this.history = this.history + "Power " + base + "^" + exp + " = " + result + "\n";
        return result;
    }
    
    fun show_history() {
        say("=== Calculator History ===");
        say(this.history);
    }
}

var calc = Calculator();
calc.initialize();

var sum = calc.add(15, 25);
var power_result = calc.power(2, 10);

say("Sum: " + sum);
say("Power: " + power_result);
calc.show_history();
```

**Compilation:**
```bash
./neutron -b calculator.nt calculator_app
./calculator_app
```

### Example 3: File Processing Application

**file_processor.nt:**
```neutron
use sys;
use json;
use time;

class FileProcessor {
    fun process_data(input_file, output_file) {
        if (!sys.exists(input_file)) {
            say("Error: Input file '" + input_file + "' not found");
            return false;
        }
        
        say("Processing file: " + input_file);
        
        // Read input
        var content = sys.read(input_file);
        say("Read " + string_length(content) + " characters");
        
        // Create processing report
        var report = {
            "input_file": input_file,
            "output_file": output_file,
            "processed_at": time.now(),
            "content_length": string_length(content),
            "status": "completed"
        };
        
        // Write report
        var report_json = json.stringify(report, true);
        sys.write(output_file, report_json);
        
        say("Processing complete. Report saved to: " + output_file);
        return true;
    }
}

// Create test input file
sys.write("input.txt", "This is sample data for processing.\nLine 2 of data.\nEnd of file.");

// Process the file
var processor = FileProcessor();
var success = processor.process_data("input.txt", "report.json");

if (success) {
    say("File processing successful!");
    
    // Show the report
    var report_content = sys.read("report.json");
    say("Generated report:");
    say(report_content);
} else {
    say("File processing failed!");
}

// Cleanup
sys.rm("input.txt");
sys.rm("report.json");
```

**Compile and run:**
```bash
./neutron -b file_processor.nt
./file_processor.nt.out
```

### Example 4: HTTP Client Application

**api_client.nt:**
```neutron
use http;
use json;
use sys;

class ApiClient {
    var base_url;
    
    fun initialize(url) {
        this.base_url = url;
        say("API Client initialized for: " + url);
    }
    
    fun fetch_data(endpoint) {
        var full_url = this.base_url + endpoint;
        say("Fetching: " + full_url);
        
        var response = http.get(full_url);
        
        say("Response status: " + response["status"]);
        say("Response body: " + response["body"]);
        
        return response;
    }
    
    fun post_data(endpoint, data) {
        var full_url = this.base_url + endpoint;
        var json_data = json.stringify(data);
        
        say("Posting to: " + full_url);
        say("Data: " + json_data);
        
        var response = http.post(full_url, json_data);
        
        say("Post response status: " + response["status"]);
        return response;
    }
}

// Example usage (uses mock HTTP responses)
var client = ApiClient();
client.initialize("https://api.example.com");

// GET request
var get_response = client.fetch_data("/users/1");

// POST request
var user_data = {
    "name": "New User",
    "email": "user@example.com",
    "active": true
};

var post_response = client.post_data("/users", user_data);

say("API client demo completed!");
```

**Compile and run:**
```bash
./neutron -b api_client.nt web_client
./web_client
```

## Performance Considerations

### Optimization Benefits

Compiled binaries provide several performance advantages:

1. **Startup Time**: Faster startup compared to interpreted execution
2. **Memory Usage**: More efficient memory allocation and management
3. **Code Optimization**: G++ compiler optimizations are applied
4. **Reduced I/O**: No need to read and parse source files at runtime

### Performance Tips

1. **Use Built-in Modules**: Built-in modules are optimized and efficient
2. **Minimize String Operations**: String concatenation in loops can be expensive
3. **Leverage Native Functionality**: Use math operations instead of manual calculations
4. **Profile Before Optimizing**: Measure performance to identify bottlenecks

### Binary Size

Compiled binaries include:
- The Neutron runtime
- Your application code
- All embedded Neutron modules
- Standard library functions

Typical binary sizes range from 100KB to several MB depending on:
- Application complexity
- Number of modules used
- Amount of embedded module code

## Deployment and Distribution

### Single Binary Distribution

For applications using only built-in modules and Neutron modules:

```bash
# Just distribute the binary
./myapp.nt.out
```

### Complex Application Distribution

For applications using native box modules:

```
myapp/
├── myapp.nt.out          # Main executable
├── README.md             # Usage instructions
└── box/                  # Native modules
    ├── custom_module/
    │   └── custom_module.so
    └── another_module/
        └── another_module.so
```

### Cross-Platform Considerations

- Binaries are platform-specific (Linux, macOS, Windows)
- Compile on target platform or use cross-compilation
- Native modules (`.so`) are also platform-specific
- Test on target systems before distribution

## Troubleshooting

### Common Compilation Issues

1. **Missing Dependencies**: Ensure all required libraries are installed
2. **Module Not Found**: Check that all `use` statements reference valid modules
3. **Compilation Errors**: Review C++ compiler output for syntax issues
4. **Runtime Failures**: Verify native module dependencies are present

### Debugging Compiled Binaries

1. **Add Debug Output**: Use `say()` statements for debugging
2. **Test Modules Separately**: Test individual modules before compilation
3. **Check File Paths**: Ensure relative paths work in deployment environment
4. **Validate Permissions**: Check file system permissions for I/O operations

### Getting Help

If you encounter issues:

1. Check the compilation output for error messages
2. Test your script in interpreter mode first: `./neutron your_script.nt`
3. Verify all modules load correctly before compilation
4. Review the generated C++ code (if available) for insights

## Conclusion

Neutron's binary conversion feature provides a powerful way to create standalone applications from your Neutron scripts. With support for all language features, built-in modules, and both Neutron and native extension modules, you can build and distribute complete applications without requiring users to install the Neutron interpreter.

The compilation process is optimized for performance and creates efficient binaries suitable for production deployment. Whether you're building system utilities, web clients, data processing tools, or complex applications with object-oriented design, binary conversion makes your Neutron programs ready for distribution.