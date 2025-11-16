# Neutron Binary Conversion

## Overview

The Neutron binary conversion feature allows you to compile your Neutron scripts (`.nt` files) into standalone, optimized native executables. This powerful feature provides several advantages:

- **🚀 Cross-Platform Support**: Full support for Linux, macOS, and Windows
- **📦 Standalone Distribution**: No need for users to install the Neutron interpreter
- **⚡ Performance**: Compiled binaries with multiple optimization levels (O0, O1, O2, O3, Os)
- **🔒 Code Protection**: Source code is embedded and not easily readable
- **🎯 Smart Compiler Detection**: Automatically detects and uses the best available compiler (gcc, clang, MSVC, mingw)
- **🛠️ Developer Friendly**: Keep generated C++ files for debugging with `--keep-generated` flag

Binary conversion transforms your Neutron scripts into native executables that can run independently on the target system without any external dependencies (except standard system libraries).

## Usage

### Basic Compilation

To convert a Neutron script to a standalone executable, use the `-b` flag:

```bash
./neutron -b your_script.nt [output_name] [flags]
```

**Arguments:**
- `your_script.nt`: The Neutron script to compile
- `[output_name]`: (Optional) The name of the output executable. If not provided, defaults to `your_script.nt.out`
- `[flags]`: Optional compilation flags (see below)

**Examples:**

```bash
# Basic compilation with default output name
./neutron -b hello.nt
# Creates: hello.nt.out

# Compile with custom output name  
./neutron -b hello.nt hello_app
# Creates: hello_app (or hello_app.exe on Windows)

# Compile with optimization level O3
./neutron -b:O3 myapp.nt myapp
# Creates highly optimized binary

# Compile with verbose output
./neutron -b myapp.nt myapp -v
# Shows detailed compilation information

# Keep generated C++ file for debugging
./neutron -b myapp.nt myapp --keep-generated
# Preserves myapp_main.cpp for inspection
```

### Optimization Levels

Control compilation optimization with the `-b:ON` syntax:

```bash
# No optimization (fastest compilation, largest binary, slowest runtime)
./neutron -b:O0 script.nt output

# Basic optimization
./neutron -b:O1 script.nt output

# Moderate optimization (DEFAULT - balanced performance)
./neutron -b:O2 script.nt output
./neutron -b script.nt output  # -b alone defaults to O2

# Aggressive optimization (best performance, slower compilation)
./neutron -b:O3 script.nt output

# Size optimization (smallest binary)
./neutron -b:Os script.nt output
```

**Optimization Comparison:**

| Level | Compile Time | Binary Size | Runtime Speed | Use Case |
|-------|-------------|-------------|---------------|----------|
| O0 | Fastest | Largest | Slowest | Development/Debugging |
| O1 | Fast | Large | Moderate | Quick testing |
| O2 | Moderate | Medium | Fast | **Default - Production** |
| O3 | Slow | Medium | Fastest | Performance-critical |
| Os | Moderate | Smallest | Fast | Embedded/Space-constrained |

### Compilation Flags

Additional flags to customize compilation:

| Flag | Description | Example |
|------|-------------|---------|
| `-v`, `--verbose` | Show detailed compilation output | `./neutron -b script.nt -v` |
| `-k`, `--keep-generated` | Keep generated C++ file | `./neutron -b script.nt --keep-generated` |

**Combined Usage:**

```bash
# Compile with O3 optimization, verbose output, and keep generated file
./neutron -b:O3 myapp.nt myapp -v --keep-generated

# Size-optimized build with custom name
./neutron -b:Os script.nt tiny_app
```

### Running Compiled Binaries

The generated executables are fully standalone and run independently:

**Linux/macOS:**
```bash
./your_script.nt.out

# Or with custom name:
./hello_app
```

**Windows:**
```cmd
your_script.nt.out.exe

REM Or with custom name:
hello_app.exe
```

The executable runs independently without requiring the Neutron interpreter or any Neutron-specific dependencies.

## Cross-Platform Support

The binary compiler fully supports all major operating systems with automatic platform detection and compiler selection.

### Linux

**Supported Compilers (auto-detected in order of preference):**
1. `g++` (GNU C++ Compiler)
2. `clang++` (LLVM Clang)
3. `c++` (system default)

**Requirements:**
- C++17 compatible compiler
- libcurl development files (`libcurl4-openssl-dev` or `libcurl-devel`)
- libjsoncpp development files (`libjsoncpp-dev` or `jsoncpp-devel`)

**Installation:**
```bash
# Debian/Ubuntu
sudo apt-get install build-essential libcurl4-openssl-dev libjsoncpp-dev

# Fedora/RHEL
sudo dnf install gcc-c++ libcurl-devel jsoncpp-devel

# Arch Linux
sudo pacman -S base-devel curl jsoncpp
```

### macOS

**Supported Compilers (auto-detected in order of preference):**
1. `clang++` (Xcode Command Line Tools)
2. `g++` (if installed via Homebrew)
3. `c++` (system alias)

**Requirements:**
- Xcode Command Line Tools
- Homebrew (recommended for dependencies)
- libcurl (included in macOS)
- jsoncpp (via Homebrew)

**Installation:**
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install dependencies via Homebrew
brew install jsoncpp curl
```

### Windows

**Supported Compilers (auto-detected in order of preference):**
1. `cl` (MSVC - Microsoft Visual C++)
2. `g++` (MinGW-w64 or MSYS2)
3. `clang++` (LLVM for Windows)

**Option 1: MSYS2 (Recommended)**

MSYS2 provides a complete Unix-like environment with package management:

```bash
# Install MSYS2 from https://www.msys2.org/
# Then in MSYS2 terminal:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-curl mingw-w64-x86_64-jsoncpp

# Compile in MSYS2 terminal:
./neutron -b script.nt myapp
```

**Option 2: MinGW-w64**

```bash
# Install MinGW-w64 from https://www.mingw-w64.org/
# Add MinGW bin directory to PATH
# Install dependencies and compile
```

**Option 3: Visual Studio (MSVC)**

```cmd
REM Install Visual Studio 2019 or later
REM Install vcpkg for dependencies
vcpkg install curl jsoncpp

REM Compile (use Developer Command Prompt)
neutron.exe -b script.nt myapp
```

### Compiler Detection

The binary compiler automatically detects available compilers on your system:

```bash
# Check which compiler will be used (verbose mode)
./neutron -b script.nt output -v
# Output will show: "Using compiler: g++" or "Using compiler: clang++" etc.
```

**Manual Compiler Selection:**

If you need to use a specific compiler, set the `CXX` environment variable:

```bash
# Use clang++ instead of default
export CXX=clang++
./neutron -b script.nt output

# Use a specific version
export CXX=g++-11
./neutron -b script.nt output

# Windows (Command Prompt)
set CXX=clang++
neutron.exe -b script.nt output
```

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
- `fmt`: Type conversion and formatting utilities

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

Binary conversion is fully cross-platform and automatically adapts to your build environment:

#### Supported Platforms

| Platform | Compiler | Object Extension | Output Extension | Link Flags |
|----------|----------|------------------|------------------|------------|
| **Linux** | GCC/Clang | `.o` | (none) | `-lcurl -ljsoncpp -ldl` |
| **macOS** | Clang/GCC | `.o` | (none) | `-lcurl -ljsoncpp -framework CoreFoundation` |
| **Windows (MSVC)** | `cl.exe` | `.obj` | `.exe` | `/link CURL::libcurl.lib JsonCpp::JsonCpp.lib` |
| **Windows (MINGW64)** | `g++` | `.o` | `.exe` | `-lcurl -ljsoncpp` |

#### Platform Detection

The binary converter automatically:

1. **Detects the platform** using C++ preprocessor macros:
   - `_WIN32` for Windows (MSVC or MINGW64)
   - `__APPLE__` for macOS
   - Otherwise assumes Linux

2. **Detects MINGW64** on Windows via `MSYSTEM` environment variable

3. **Selects the appropriate compiler**:
   - Windows: MSVC (`cl`) or MINGW64 (`g++`)
   - macOS: Clang (`clang++`) or GCC (`g++`)
   - Linux: GCC (`g++`) or Clang (`clang++`)

4. **Uses platform-specific flags**:
   - MSVC: `/std:c++17 /EHsc /W4 /O2`
   - GCC/Clang: `-std=c++17 -Wall -Wextra -O2`

5. **Applies correct file extensions**:
   - Windows automatically adds `.exe` if missing
   - Unix systems don't add extensions

#### Building for Multiple Platforms

To create executables for multiple platforms, compile on each target platform:

**Linux:**
```bash
./neutron -b myapp.nt myapp
# Creates: myapp (Linux x86_64 ELF)
```

**macOS:**
```bash
./neutron -b myapp.nt myapp
# Creates: myapp (macOS Mach-O)
```

**Windows (MSVC):**
```cmd
neutron.exe -b myapp.nt myapp
REM Creates: myapp.exe (Windows PE with MSVC runtime)
```

**Windows (MINGW64):**
```bash
./neutron -b myapp.nt myapp
# Creates: myapp.exe (Windows PE with MinGW runtime)
```

#### Native Module Compatibility

- **Native modules** (`.so`/`.dll`/`.dylib`) are platform-specific
- Must compile native modules for each target platform
- Use Box Package Manager's cross-platform build support
- Ensure native modules are available in the same directory structure on target systems

#### Cross-Compilation Limitations

- Binary conversion requires compiling on the target platform or using cross-compilation toolchains
- Native modules must be available for the target platform
- The Neutron runtime library (`libneutron_runtime.a`) must exist for the target platform
- Test thoroughly on target systems before distribution

## Troubleshooting

### Common Compilation Issues

#### No Compiler Found

**Error:** `No suitable C++ compiler found on system!`

**Solution:**
```bash
# Linux (Debian/Ubuntu)
sudo apt-get install build-essential

# Linux (Fedora/RHEL)
sudo dnf install gcc-c++

# macOS
xcode-select --install

# Windows (MSYS2)
pacman -S mingw-w64-x86_64-gcc
```

#### Missing Dependencies

**Error:** `fatal error: curl/curl.h: No such file or directory`

**Solution:**
```bash
# Linux (Debian/Ubuntu)
sudo apt-get install libcurl4-openssl-dev libjsoncpp-dev

# macOS
brew install curl jsoncpp

# Windows (MSYS2)
pacman -S mingw-w64-x86_64-curl mingw-w64-x86_64-jsoncpp
```

#### Compilation Takes Too Long

**Issue:** Compilation is slow or seems to hang

**Solution:**
- This is normal for first compilation as it compiles the entire runtime
- Use optimization level O0 for faster compilation during development: `./neutron -b:O0 script.nt`
- The compilation includes all runtime sources (~20+ files)
- Subsequent compilations with the same optimization level will be cached by your compiler

#### Module Not Found

**Error:** Script uses a module that isn't embedded

**Solution:**
1. Verify the module exists: `ls lib/modulename.nt` or `ls box/modulename/`
2. Test in interpreter mode first: `./neutron your_script.nt`
3. For native modules, ensure `.so`/`.dll`/`.dylib` files are present
4. Check module path in `use` statement matches actual filename

#### Compilation Fails with Linker Errors

**Error:** `undefined reference to...` or `cannot find -lcurl`

**Solution:**
1. Install missing development libraries
2. Check that library paths are correct
3. On Linux, try: `sudo ldconfig` to update library cache
4. Use verbose mode to see full command: `./neutron -b script.nt -v`

#### Generated Binary Doesn't Run

**Error:** `error while loading shared libraries: libcurl.so.4`

**Solution:**
- Install runtime dependencies on target system
- Linux: `sudo apt-get install libcurl4 libjsoncpp1`
- macOS: Dependencies usually already installed
- Windows: Ensure DLLs are in same directory as executable or in PATH

### Debugging Compiled Binaries

#### Inspect Generated C++ Code

Use the `--keep-generated` flag to preserve the generated C++ source:

```bash
./neutron -b script.nt output --keep-generated
# This creates output_main.cpp which you can inspect
```

The generated file shows:
- How your source code is embedded
- Which modules are included
- The main() function structure
- Any compilation issues in the generated code

#### Verbose Compilation Output

Get detailed information about the compilation process:

```bash
./neutron -b script.nt output -v
```

This shows:
- Which compiler is being used
- Compiler version
- All source files being compiled
- Full compilation command
- Linking flags and libraries

#### Test Before Compiling

Always test your script in interpreter mode first:

```bash
# Test the script
./neutron script.nt

# If it works, then compile
./neutron -b script.nt output
```

#### Step-by-Step Debugging

1. **Verify syntax:** Run in interpreter mode
2. **Check modules:** Ensure all `use` statements work
3. **Test with verbose:** Compile with `-v` flag
4. **Keep generated file:** Use `--keep-generated` to inspect C++ code
5. **Try different optimization:** Use `-b:O0` for faster, easier debugging
6. **Check dependencies:** Verify all libraries are installed

### Platform-Specific Issues

#### Linux

**Issue:** Permission denied when running compiled binary

**Solution:**
```bash
chmod +x your_binary
./your_binary
```

**Issue:** Missing GLIBC version

**Solution:** Compile on a system with the same or older GLIBC version as your target

#### macOS

**Issue:** "cannot be opened because the developer cannot be verified"

**Solution:**
```bash
# Remove quarantine attribute
xattr -d com.apple.quarantine your_binary

# Or allow in System Preferences > Security & Privacy
```

**Issue:** Compilation fails on Apple Silicon

**Solution:** Ensure you have native ARM64 tools installed via Homebrew

#### Windows

**Issue:** Missing DLL errors

**Solution:**
- Use MSYS2 for self-contained environment
- Copy required DLLs to application directory
- Use static linking where possible

**Issue:** MSVC compilation fails

**Solution:** Use Developer Command Prompt for Visual Studio

### Performance Issues

#### Binary is Too Large

**Solution:**
```bash
# Use size optimization
./neutron -b:Os script.nt output

# Strip symbols (Linux/macOS)
strip output

# Windows
strip output.exe
```

#### Binary Runs Slowly

**Solution:**
```bash
# Use aggressive optimization
./neutron -b:O3 script.nt output
```

### Getting Help

If you encounter issues not covered here:

1. **Enable verbose mode:** `./neutron -b script.nt output -v`
2. **Keep generated file:** `./neutron -b script.nt output --keep-generated`
3. **Test in interpreter:** `./neutron script.nt`
4. **Check compiler:** Verify your compiler version supports C++17
5. **Verify dependencies:** Ensure curl and jsoncpp are installed
6. **Review error messages:** Read compiler output carefully
7. **Check module paths:** Verify all modules can be found

## Best Practices

### Development Workflow

1. **Develop in interpreter mode** for fast iteration
2. **Test thoroughly** before compiling
3. **Use -b:O0** during development for fast compilation
4. **Use -b:O3** or -b:O2 for production releases
5. **Keep generated files** during debugging with `--keep-generated`
6. **Test on target platforms** before distributing

### Production Deployment

```bash
# Recommended production build command
./neutron -b:O3 myapp.nt myapp -v

# For size-critical applications
./neutron -b:Os myapp.nt myapp

# Then strip symbols (optional, Linux/macOS)
strip myapp
```

### Version Control

Add to your `.gitignore`:
```
*.out
*.exe
*_main.cpp
*.o
*.obj
```

### Distribution

**Single Binary Distribution:**
```
myapp/
├── myapp (or myapp.exe on Windows)
├── README.md
└── LICENSE
```

**With Native Modules:**
```
myapp/
├── myapp (or myapp.exe)
├── box/
│   └── custom_module/
│       └── custom_module.so
├── README.md
└── LICENSE
```

## Quick Reference

### Command Syntax

```bash
./neutron -b[options] input.nt [output] [flags]
```

### Options

| Option | Description |
|--------|-------------|
| `-b` | Compile with default optimization (O2) |
| `-b:O0` | No optimization (debugging) |
| `-b:O1` | Basic optimization |
| `-b:O2` | Moderate optimization (default) |
| `-b:O3` | Aggressive optimization |
| `-b:Os` | Size optimization |

### Flags

| Flag | Description |
|------|-------------|
| `-v`, `--verbose` | Show detailed compilation output |
| `-k`, `--keep-generated` | Preserve generated C++ file |

### Examples

```bash
# Basic compile
./neutron -b script.nt

# Custom output, O3 optimization
./neutron -b:O3 script.nt myapp

# Verbose, keep generated, size optimized
./neutron -b:Os script.nt tiny_app -v --keep-generated

# Development mode (fast compilation)
./neutron -b:O0 script.nt test

# Production mode (best performance)
./neutron -b:O3 script.nt release
```

## Conclusion

Neutron's enhanced binary conversion feature provides a powerful, cross-platform solution for creating standalone applications from your Neutron scripts. With comprehensive support for:

✅ **All major platforms:** Linux, macOS, and Windows
✅ **Multiple compilers:** gcc, clang, MSVC, mingw
✅ **Optimization levels:** O0 through O3, plus size optimization
✅ **Developer tools:** Verbose mode, generated file preservation
✅ **All language features:** Full Neutron language support
✅ **Module system:** Both Neutron and native modules

You can build and distribute complete, optimized applications without requiring users to install the Neutron interpreter.

Whether you're building system utilities, web clients, data processing tools, or complex applications with object-oriented design, the binary conversion feature makes your Neutron programs production-ready and easy to distribute. The automatic compiler detection and cross-platform support ensure your builds work seamlessly across different environments.

**Happy coding and compiling! 🚀**