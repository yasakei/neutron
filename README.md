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
 
 **Neutron** is a high-performance scripting language with a C++ runtime. It combines Python's simplicity with significantly better performance for computational tasks.
 
 **Key Features:**
 - **Fast:** C++ bytecode VM (2-10x faster than Python)
 - **Native:** Compiles to standalone executables
 - **Battery-Included:** Standard library with HTTP, JSON, RegEx, and more
 - **Cross-Platform:** Runs on Linux, macOS, Windows
 
 > [!NOTE]
 > **New?** Check the **[Quick Start Guide](docs/guides/quickstart.md)** to get running in 5 minutes.
 
 ---

## Architecture
 
 Neutron uses a modern three-stage pipeline: **Scanner/Parser** → **Bytecode Compiler** → **Stack-based VM**.
 
 - **Zero Dependencies:** Written in C++17 with minimal external reliance.
 - **Smart Memory:** Deterministic RC/GC memory management.
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

Neutron is distributed under the **Neutron Permissive License (NPL) 1.1**.

See [LICENSE](LICENSE) for complete terms and conditions.

---

<div align="center">

**Created and maintained by [yasakei](https://github.com/yasakei)**

Star this repo if you find Neutron useful!

</div>
