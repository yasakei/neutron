# Neutron Built-in Modules Documentation

This directory contains comprehensive documentation for all built-in modules in the Neutron programming language.

## Core Modules

### System & Environment
- **[Sys Module](sys_module.md)** - File operations, system utilities, environment access, and process control
  - **Functions:** File I/O (`read`, `write`, `append`), file operations (`cp`, `mv`, `rm`), directory operations (`mkdir`, `rmdir`, `exists`), system info (`cwd`, `env`, `info`), user input (`input`), process control (`exit`, `exec`)

### Data Processing & Conversion  
- **[JSON Module](json_module.md)** - JSON parsing, serialization, and object manipulation
  - **Functions:** `stringify`, `parse`, `get`
  
- **[Convert Module](convert_module.md)** - Data type conversion and string manipulation utilities
  - **Functions:** `char_to_int`, `int_to_char`, `string_length`, `string_get_char_at`, `str`, `int`, `int_to_bin`, `bin_to_int`

### Mathematical Operations
- **[Math Module](math_module.md)** - Mathematical operations and functions
  - **Functions:** `add`, `subtract`, `multiply`, `divide`, `pow`, `sqrt`, `abs`

### Network & Web
- **[HTTP Module](http_module.md)** - HTTP client functionality for web requests
  - **Functions:** `get`, `post`, `put`, `delete`, `head`, `patch`

### Time & Scheduling
- **[Time Module](time_module.md)** - Time operations, formatting, and delays
  - **Functions:** `now`, `format`, `sleep`

## Usage

All modules can be imported using the `use` statement:

```neutron
use sys;
use math;
use json;
use http;
use time;
use convert;

// Use module functions
var currentDir = sys.cwd();
var result = math.add(10, 20);
var jsonStr = json.stringify({"key": "value"});
var response = http.get("https://api.example.com");
var timestamp = time.now();
var ascii = char_to_int("A"); // Convert functions also available globally
```

## Module Categories

### **File & System Operations**
- **sys**: Complete file system access, environment variables, process control

### **Data Formats & Conversion**  
- **json**: JSON processing for APIs and configuration
- **convert**: String manipulation, character encoding, binary conversion

### **Mathematical Computing**
- **math**: Essential mathematical operations and functions

### **Network Communication**
- **http**: HTTP client for web API interaction

### **Time & Scheduling**
- **time**: Timestamps, formatting, delays, performance timing

## Documentation Features

Each module documentation includes:

- ✅ **Complete Function Reference** with parameters and return values
- ✅ **Practical Examples** showing real-world usage patterns  
- ✅ **Error Handling** guidance and best practices
- ✅ **Common Usage Patterns** and implementation templates
- ✅ **Performance Considerations** and optimization tips
- ✅ **Compatibility Information** across interpreter and compiled modes

## Quick Reference

### Most Commonly Used Functions

```neutron
// File operations
sys.read("file.txt")
sys.write("file.txt", "content")
sys.exists("file.txt")

// Math operations  
math.add(a, b)
math.pow(base, exponent)
math.sqrt(number)

// JSON processing
json.stringify(object)
json.parse(jsonString)

// HTTP requests
http.get(url)
http.post(url, data)

// Time operations
time.now()
time.format(timestamp)
time.sleep(milliseconds)

// String/conversion (available globally)
string_length(text)
char_to_int(character)
int_to_bin(number)
```

## Getting Started

1. **Choose the module** you need from the list above
2. **Click the module link** to view comprehensive documentation
3. **Import the module** in your Neutron code using `use modulename;`
4. **Follow the examples** provided in each module's documentation

## Module System Architecture

Neutron supports two types of modules:

- **Native Modules** (C++): Built-in modules compiled into the runtime (`sys`, `math`, `json`, `http`, `time`, `convert`)
- **Neutron Modules** (.nt files): User-created modules written in Neutron language

All built-in modules are automatically available and work in both interpreter and compiled binary modes.
