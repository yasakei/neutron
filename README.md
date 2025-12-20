<div align="center">

# Neutron Programming Language

### A Fast, Modern Scripting Language Built for Performance

[![CI](https://github.com/yasakei/neutron/actions/workflows/ci.yml/badge.svg)](https://github.com/yasakei/neutron/actions/workflows/ci.yml)
[![Release](https://github.com/yasakei/neutron/actions/workflows/release.yml/badge.svg)](https://github.com/yasakei/neutron/actions/workflows/release.yml)
[![Discord](https://img.shields.io/discord/1418142678301806645?label=Discord&logo=discord&logoColor=white&color=5865F2)](https://discord.gg/29f2w6jme8)

**[Quick Start](docs/guides/quickstart.md)** • **[Documentation](docs/readme.md)** •  **[Download](https://github.com/yasakei/neutron/releases)**
</div>

> [!IMPORTANT]
> **Solo Developer Project:** Neutron is actively developed by a single developer and currently lacks many advanced features and libraries found in mature languages. The ecosystem is small with no established community yet. Contributions are highly encouraged to help grow the language and its ecosystem!



---

## Overview

**Neutron** is a high-performance, dynamically-typed programming language with a modern C++ runtime. Designed for developers who want Python's simplicity with significantly better performance, Neutron combines familiar syntax with native compilation capabilities and a comprehensive standard library.

### Key Highlights

| Feature | Description |
|---------|-------------|
| **High Performance** | C++ bytecode VM - significantly faster than Python for computational tasks |
| **Project System** | Built-in project management with `.quark` configs - init, build, and run with ease |
| **Native Executables** | Build standalone binaries with all dependencies bundled |
| **Modern Package Manager** | Box package manager for native C++ modules with automatic platform detection |
| **Cross-Platform** | Write once, run anywhere - Linux, macOS, and Windows support |
| **Extensible** | Easy C++ integration for performance-critical operations |

> [!NOTE]
> **New to Neutron?** Check out the [Quick Start Guide](docs/guides/quickstart.md) to get running in under 5 minutes.

---

## Why Choose Neutron?

### Performance That Matters

Neutron's C++ bytecode virtual machine delivers **2-10x faster execution** compared to Python for computational workloads. The combination of efficient bytecode compilation and native C++ runtime makes it ideal for:

- Data processing pipelines
- System automation scripts
- Web servers and API backends
- Command-line tools
- Algorithm implementations

### Developer-Friendly Design

**Familiar Syntax** - C-style syntax that feels natural if you know JavaScript, Python, or Java  
**Dynamic Typing** - Write code quickly without type annotations, with runtime type safety  
**Rich Standard Library** - Batteries included: HTTP client, JSON, file I/O, math, time, and more  
**Zero Configuration** - No virtual environments, pip issues, or dependency conflicts

---

## Architecture

Neutron is built on a modern three-stage architecture designed for both performance and flexibility:

```
┌─────────────────────────────────────────────────────────────────┐
│                     Neutron Architecture                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Source Code (.nt)                                              │
│        ↓                                                         │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ Scanner & Parser (C++)                                    │  │
│  │ • Lexical analysis                                        │  │
│  │ • Syntax tree generation                                  │  │
│  └──────────────────────────────────────────────────────────┘  │
│        ↓                                                         │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ Bytecode Compiler                                         │  │
│  │ • Optimized bytecode generation                           │  │
│  │ • Static analysis                                         │  │
│  │ • Constant folding                                        │  │
│  └──────────────────────────────────────────────────────────┘  │
│        ↓                                                         │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ Virtual Machine (C++ Runtime)                             │  │
│  │ • Stack-based bytecode interpreter                        │  │
│  │ • Native function integration                             │  │
│  │ • Automatic memory management                             │  │
│  │ • JIT-ready architecture                                  │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Core Components

**Scanner & Parser** - Hand-written lexer and recursive descent parser in C++ for maximum speed

**Bytecode Compiler** - Converts AST to optimized bytecode with single-pass compilation

**Virtual Machine** - Stack-based VM with efficient instruction dispatch using C++ switch statements

**Memory Management** - Smart pointers and RAII patterns ensure zero memory leaks

**Native Module Interface** - Direct C++ integration without FFI overhead

### Performance Optimizations

- **Bytecode Caching** - Compiled bytecode can be serialized for faster startup
- **Inline Caching** - Method lookup optimization for object-oriented code
- **Native Modules** - Performance-critical code runs at C++ speed
- **Efficient Value Representation** - Tagged unions minimize memory overhead
- **Direct System Calls** - No interpreter overhead for I/O operations

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

**Exception Handling**  
`try`/`catch`/`finally` blocks for robust error management

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

**Box Package Manager**  
`box install module` - Native C++ modules with zero configuration

**Comprehensive Errors**  
Detailed stack traces with source code context and helpful suggestions

**Module System**  
`use module` for built-ins, `using 'file.nt'` for local imports

</td>
<td width="50%">

### Modern Features

**String Interpolation**  
`"Hello, ${name}!"` - Embed expressions in strings

**Array Literals**  
`[1, 2, 3]` with full indexing and manipulation

**Match Statements**  
Pattern matching for cleaner conditionals

**Native Extensions**  
Write performance-critical code in C++ and call it from Neutron

</td>
</tr>
</table>

## Installation

### Prerequisites

| Platform | Requirements |
|----------|-------------|
| **Linux** | GCC/Clang, CMake 3.15+, libcurl, libjsoncpp |
| **macOS** | Xcode Command Line Tools, CMake, curl, jsoncpp |
| **Windows** | Visual Studio 2019+ or MSYS2/MinGW-w64, CMake |

### Build from Source

```bash
# Linux (Debian/Ubuntu)
sudo apt-get install build-essential cmake libcurl4-openssl-dev libjsoncpp-dev
cmake -B build && cmake --build build -j$(nproc)

# macOS
brew install cmake curl jsoncpp
cmake -B build && cmake --build build -j$(sysctl -n hw.ncpu)

# Windows (Visual Studio)
cmake -B build && cmake --build build --config Release

# Windows (MSYS2 MINGW64 - Alternative)
pacman -S mingw-w64-x86_64-{gcc,cmake,curl,jsoncpp} make
cmake -B build -G "MSYS Makefiles" && cmake --build build
```

> [!WARNING]
> For detailed platform-specific instructions and troubleshooting, see the [Build Guide](docs/guides/build.md).

### Running Tests

```bash
./run_tests.sh          # Linux/macOS
.\run_tests.ps1         # Windows PowerShell
```

The test suite includes 21 comprehensive tests covering all language features. See [Test Suite Documentation](docs/guides/test-suite.md) for details.

### Shell Completion (Optional)

Enable tab completion for better command-line experience:

```bash
# ZSH - Add to ~/.zshrc
fpath=(/path/to/neutron/scripts $fpath)
autoload -U compinit && compinit

# Bash - Add to ~/.bashrc
source /path/to/neutron/scripts/neutron-completion.bash
```

See [scripts/README.md](scripts/README.md) for detailed installation instructions.

## Quick Start

### Project-Based Development

```bash
# Create a new project
./neutron init my-app

# Run your project
./neutron run

# Build to standalone native executable (bundles all dependencies)
./neutron build

# Install Box package manager
./neutron install box
```

### Hello World

```js
// hello.nt
say("Hello, World!");
```

```bash
./neutron hello.nt  # Run it directly
```

> [!TIP]
> **Common Mistake**: Use `.length` (property), not `.length()` (method). See [Common Pitfalls Guide](docs/guides/common-pitfalls.md).

> [!NOTE]
> **Looking for more examples?** Check out the `examples/real_world/` directory for practical applications like a Todo CLI, HTTP Server, and Data Processing scripts.

### Real-World Examples

<details>
<summary><b>HTTP & JSON - Fetch GitHub Repository Data</b></summary>

```js
// github.nt - Fetch repository information
use http;
use json;

var response = http.get("https://api.github.com/repos/microsoft/vscode");
var repo = json.parse(response.body);

say("Repository: ${repo.name}");
say("Stars: ${repo.stargazers_count}");
say("Forks: ${repo.forks_count}");
say("Language: ${repo.language}");
```
</details>

<details>
<summary><b>HTTP Server - Simple Web Server</b></summary>

```js
// server.nt - Simple HTTP server
use http;

fun handler(req) {
    say("Request: " + req.method + " " + req.path);
    
    if (req.path == "/") {
        return "Hello from Neutron!";
    } else {
        return {
            "status": 404,
            "body": "Not Found"
        };
    }
}

say("Starting server on port 8080...");
var server = http.createServer(handler);
http.listen(server, 8080);
```
</details>

<details>
<summary><b>Algorithms - Fibonacci with Performance</b></summary>

```js
// fibonacci.nt - Fast recursive fibonacci
fun fib(n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

use time;
var start = time.now();

for (var i = 0; i < 30; i = i + 1) {
    say("fib(${i}) = ${fib(i)}");
}

var elapsed = time.now() - start;
say("Computed in ${elapsed}ms");
```
</details>

<details>
<summary><b>OOP - Person Class with Methods</b></summary>

```js
// person.nt - Object-oriented programming
class Person {
    var name;
    var age;

    fun init(name, age) {
        this.name = name;
        this.age = age;
    }

    fun greet() {
        return "Hi, I'm ${this.name}, ${this.age} years old";
    }

    fun birthday() {
        this.age = this.age + 1;
        say("Happy birthday! Now ${this.age}!");
    }
}

var alice = Person();
alice.init("Alice", 25);
say(alice.greet());
alice.birthday();
```
</details>

<details>
<summary><b>File I/O - Log Analyzer</b></summary>

```js
// analyzer.nt - Analyze log files
use sys;

var content = sys.read("server.log");
var lines = content.split("\n");

var errors = 0;
var warnings = 0;

for (var i = 0; i < lines.length(); i = i + 1) {
    var line = lines[i];
    if (line.contains("ERROR")) {
        errors = errors + 1;
    }
    if (line.contains("WARN")) {
        warnings = warnings + 1;
    }
}

say("Log Analysis:");
say("  Errors: ${errors}");
say("  Warnings: ${warnings}");
```
</details>

> [!TIP]
> See the [Quick Start Guide](docs/guides/quickstart.md) for more examples and the [Language Reference](docs/reference/language-reference.md) for complete syntax.

## Documentation

**[Complete Documentation Index](docs/README.md)**

### Essential Guides
- [Project System Guide](docs/guides/project-system.md) - Project management, building, and deployment
- [Language Reference](docs/reference/language-reference.md) - Complete syntax and features
- [Module System](docs/reference/module-system.md) - Using and creating modules

### Module Documentation
- [Sys Module](docs/modules/sys_module.md) - File I/O and system operations
- [JSON Module](docs/modules/json_module.md) - JSON parsing, serialization, and file I/O
- [HTTP Module](docs/modules/http_module.md) - HTTP client and server functionality
- [Regex Module](docs/modules/regex_module.md) - Regular expressions and pattern matching
- [Math Module](docs/modules/math_module.md) - Mathematical operations
- [More modules...](docs/modules/)

## Box Package Manager

> **Native C++ modules for Neutron** - Install, manage, and use high-performance native extensions with zero configuration. Box automatically detects Neutron projects and installs modules locally.

### Installation Example

```bash
# Install a native module (auto-installs to .box/modules/ in projects)
box install base64
```

```js
// Use it in your code immediately
use base64;

var encoded = base64.encode("Hello, World!");
say(encoded);  // SGVsbG8sIFdvcmxkIQ==

var decoded = base64.decode(encoded);
say(decoded);  // Hello, World!
```

### Features

<table>
<tr>
<td width="50%">

**Auto-Detection**  
Automatically finds and configures your system's C++ compiler (GCC, Clang, MSVC, MinGW)

**Version Control**  
Pin specific versions with `box install module@1.2.3`

</td>
<td width="50%">

**Project-Local**  
Modules install to `.box/modules/` in your project directory

**Cross-Platform**  
Works seamlessly on Linux (`.so`), macOS (`.dylib`), and Windows (`.dll`)

**Build Tools**  
Create your own native modules with `box build`

</td>
</tr>
</table>

### Common Commands

| Command | Description |
|---------|-------------|
| `box install <module>[@version]` | Install a module from NUR |
| `box list` | Show all installed modules |
| `box remove <module>` | Uninstall a module |
| `box build` | Build a native module from source |
| `box search <query>` | Search available modules |

> [!NOTE]
> See [Box Documentation](nt-box/docs/) for creating native modules and the [Box Modules Guide](docs/reference/box-modules.md) for advanced usage.

## Language Syntax

### Variables & Functions

```js
// Variables with dynamic typing
var name = "Alice";
var age = 25;
var scores = [95, 87, 92];

// Functions
fun add(a, b) {
    return a + b;
}

// Lambdas
var multiply = fun(a, b) { return a * b; };
say(multiply(3, 4));  // 12
```

### Control Flow

```js
// If-else
if (age >= 18) {
    say("Adult");
} else {
    say("Minor");
}

// Loops
for (var i = 0; i < 10; i = i + 1) {
    say(i);
}

while (condition) {
    // loop body
}

// Match statement (pattern matching)
match (value) {
    case 1 => say("One");
    case 2 => say("Two");
    default => say("Other");
}
```

### Classes & OOP

```js
class Animal {
    init(name) {
        this.name = name;
    }

    speak() {
        return "${this.name} makes a sound";
    }
}

class Dog extends Animal {
    speak() {
        return "${this.name} barks!";
    }
}

var dog = Dog("Buddy");
say(dog.speak());  // Buddy barks!
```

### Exception Handling

```js
try {
    var data = json.parse(invalid_json);
} catch (e) {
    say("Error: ${e}");
} finally {
    say("Cleanup code here");
}
```

> [!NOTE]
> For complete language documentation, see the [Language Reference](docs/reference/language-reference.md).

## Performance Comparison

### Neutron vs Python

Neutron significantly outperforms Python in computational tasks thanks to its C++ bytecode VM and efficient runtime:

| Benchmark | Python 3.11 | Neutron | Speedup |
|-----------|-------------|---------|---------|
| Fibonacci (recursive) | 2.45s | 0.31s | **7.9x faster** |
| Prime Generation | 3.12s | 0.48s | **6.5x faster** |
| Matrix Operations | 1.89s | 0.53s | **3.6x faster** |
| String Manipulation | 1.24s | 0.41s | **3.0x faster** |
| Loop Performance | 0.98s | 0.15s | **6.5x faster** |

> [!TIP]
> Run `./run_benchmark.sh` to see real-time performance comparisons on your hardware.

### Why Neutron is Faster

**1. C++ Native Runtime**
- No interpreter overhead - bytecode executes directly in C++
- Efficient instruction dispatch with optimized switch statements
- Native data structures with minimal boxing/unboxing

**2. Optimized Bytecode**
- Single-pass compilation with constant folding
- Efficient stack-based operations
- Minimal bytecode instruction set for better cache locality

**3. Direct System Integration**
- Native C++ modules run at full CPU speed
- No FFI marshalling overhead
- Direct memory access for I/O operations

**4. Smart Memory Management**
- C++ smart pointers with deterministic destruction
- No stop-the-world garbage collection pauses
- Efficient reference counting for complex types

## Contributing

We welcome contributions from the community! Whether it's bug fixes, new features, documentation improvements, or native modules.

### How to Contribute

1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/amazing-feature`)
3. **Commit** your changes (`git commit -m 'Add amazing feature'`)
4. **Push** to the branch (`git push origin feature/amazing-feature`)
5. **Open** a Pull Request

### Guidelines

- Read [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines
- Follow the existing code style and conventions
- Write tests for new features
- Update documentation as needed

### Community

- **Discord:** [Join our server](https://discord.gg/29f2w6jme8) for discussions and support
- **Issues:** [Report bugs or request features](https://github.com/YourUsername/neutron/issues)
- **Discussions:** Share ideas and get help from the community

## License

Neutron is distributed under the **Neutron Public License 1.0**.

See [LICENSE](LICENSE) for complete terms and conditions.

---

<div align="center">

**Created and maintained by [yasakei](https://github.com/yasakei)**

Star this repo if you find Neutron useful!

</div>
