<div align="center">

# Neutron Programming Language

### A Fast, Modern Scripting Language Built for Performance

[![CI](https://github.com/yasakei/neutron/actions/workflows/ci.yml/badge.svg)](https://github.com/yasakei/neutron/actions/workflows/ci.yml)
[![Release](https://github.com/yasakei/neutron/actions/workflows/release.yml/badge.svg)](https://github.com/yasakei/neutron/actions/workflows/release.yml)

**[Quick Start](docs/guides/quickstart.md)** • **[Documentation](docs/readme.md)** •  **[Download](https://github.com/yasakei/neutron/releases)**
</div>

> [!IMPORTANT]
> **Solo Developer Project:** Neutron is actively developed by a single developer and currently lacks many advanced features and libraries found in mature languages. The ecosystem is small with no established community yet. Contributions are highly encouraged to help grow the language and its ecosystem!

---

## Installation
 
 ### Download Pre-built Binaries
 
 **[Download Latest Release](https://github.com/yasakei/neutron/releases/latest)**
 
 #### Linux
 - **Debian/Ubuntu/Others:** Download `neutron-linux-x64.tar.gz` from Releases.
 - **Arch Linux:**
   ```bash
   yay -S neutron
   ```
 
 #### macOS
 - **Apple Silicon:** Download `neutron-macos-arm64.tar.gz` from Releases.
 - **Intel Macs:** Build from source: `python3 package.py`
 
 #### Windows
 - Download the **Installer** (`NeutronInstaller.exe`) from Releases.
 
 --- 
 ## Overview
 
 **Neutron** is a high-performance scripting language with a C++ runtime, designed for speed and simplicity.
 
 **Key Features:**
 - **Fast:** C++ bytecode VM with multi-tier tracing JIT (x86-64 & ARM64)
 - **JIT Compiled:** Tracing JIT compiles hot loops to native x86-64 and ARM64 machine code
 - **Native:** Compiles to standalone executables
 - **Battery-Included:** Standard library with HTTP, JSON, RegEx, and more
 - **Cross-Platform:** Runs on Linux, macOS (Apple Silicon & Intel), Windows
 - **Strict Typing:** Type annotations required for all variables and functions (TypeScript/Go style)
 
 > [!NOTE]
 > **New?** Check the **[Quick Start Guide](docs/guides/quickstart.md)** to get running in 5 minutes.
 
 ---

## Architecture

 Neutron uses a modern multi-stage pipeline: **Scanner/Parser** → **Bytecode Compiler** → **Stack-based VM** → **[Multi-Tier JIT](docs/implementation/jit.md)**.

 - **Zero Dependencies:** Written in C++17 with minimal external reliance.
 - **Smart Memory:** Deterministic RC/GC memory management.
 - **JIT Compilation:** Three-tier execution (interpreter → threaded code → tracing JIT) with native codegen for **x86-64** and **ARM64**. Features OSR and deoptimization support.
 - **Native Modules:** Direct C++ integration for max performance.

## Core Features

<table>
<tr>
<td width="50%">

### Language Design

**Dynamic Typing**  
Write code without type declarations - types are inferred at runtime with full type safety

**C-Style Syntax**  
Familiar `{}` blocks, `;` terminators, and control flow from C/JavaScript/Java

**Object-Oriented**  
Classes, methods, inheritance, and `this` keyword for structured programming

**First-Class Functions**  
Lambdas, closures, and higher-order functions for functional programming patterns

**Strict Type System**  
All variables and functions require type annotations. Type checking at compile-time and runtime.

</td>
<td width="50%">

### Standard Library

**System Operations** (`sys`)  
File I/O, directory manipulation, environment variables, process control

**Web & Networking** (`http`)  
HTTP client with GET/POST/PUT/DELETE support, real socket-based server

**Data Formats** (`json`)  
Fast JSON parsing, serialization, and file I/O

**Regular Expressions** (`regex`)  
Pattern matching, search, replace, split with full capture group support

**Mathematics** (`math`)  
Standard math functions and constants

**Type Utilities** (`fmt`)  
Type conversion, formatting, and inspection

**Time & Date** (`time`)  
Timestamps, formatting, and delays

**Arrays** (`arrays`)  
Advanced array operations and manipulations

**Concurrency** (`async`)  
Native multi-threading support with async/await syntax

</td>
</tr>
<tr>
<td width="50%">

### Developer Tools

**Comprehensive Errors**  
Detailed stack traces with source code context and helpful suggestions

**Module System**  
`use module` for built-ins, `using 'file.nt'` for local imports

</td>
<td width="50%">

### Modern Features

**String Interpolation**  
`\"Hello, ${name}!\"` - Embed expressions in strings

**Array Literals**  
`[1, 2, 3]` with full indexing and manipulation

**Match Statements**  
Pattern matching for cleaner conditionals

**Native Extensions**  
Write performance-critical code in C++ and call it from Neutron

</td>
</tr>
</table>

## Quick Start

### Running Neutron Code

```bash
# Run a Neutron script directly
./neutron script.nt

# Start the interactive REPL
./neutron

# Format Neutron source files
./neutron fmt file.nt
```

### Hello World

```js
// hello.nt
say("Hello, World!");
```

```bash
./neutron hello.nt  # Run it directly
```

### Type Annotations (Required)

In strict mode, ALL variables and functions MUST have type annotations:

```js
// Variables - type required
var int age = 25;
var string name = "Alice";
var float pi = 3.14159;
var bool isActive = true;
var array numbers = [1, 2, 3];
var object person = {"name": "Bob", "age": 30};

// Functions - parameter and return types required
fun add(int a, int b) -> int {
    return a + b;
}

// Classes - properties and methods require types
class Person {
    var string name;
    var int age;
    
    fun init(string n, int a) -> int {
        this.name = n;
        this.age = a;
        return a;
    }
    
    fun getName() -> string {
        return this.name;
    }
}
```

> [!TIP]
> **Common Mistake**: Use `.length` (property), not `.length()` (method). See [Common Pitfalls Guide](docs/guides/common-pitfalls.md).

> [!NOTE]
> **Looking for more examples?** Check out the `examples/real_world/` directory for practical applications like a Todo CLI, HTTP Server, and Data Processing scripts.

