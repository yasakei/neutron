# Quick Start Guide

## Installation

### Prerequisites
Choose your platform and install dependencies:

<details>
<summary><b>Linux (Ubuntu/Debian)</b></summary>

```bash
sudo apt-get update
sudo apt-get install build-essential cmake pkg-config libcurl4-openssl-dev libjsoncpp-dev
```
</details>

<details>
<summary><b>Linux (Fedora/RHEL)</b></summary>

```bash
sudo dnf install gcc-c++ cmake pkgconfig libcurl-devel jsoncpp-devel
```
</details>

<details>
<summary><b>Linux (Arch)</b></summary>

```bash
sudo pacman -S base-devel cmake curl jsoncpp
```
</details>

<details>
<summary><b>macOS</b></summary>

```bash
brew install cmake curl jsoncpp
```
</details>

<details>
<summary><b>Windows (MSYS2)</b></summary>

```bash
# In MSYS2 MINGW64 terminal:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-curl mingw-w64-x86_64-jsoncpp make
```
</details>

## Build

### Linux / macOS
```bash
git clone https://github.com/yasakei/neutron.git
cd neutron
cmake -B build -S .
cmake --build build -j$(nproc)
./neutron --version
```

### Windows (MSYS2)
```bash
git clone https://github.com/yasakei/neutron.git
cd neutron
cmake -B build -S . -G "MSYS Makefiles"
cmake --build build -j$(nproc)
./neutron.exe --version
```

### Windows (Visual Studio)
```cmd
git clone https://github.com/yasakei/neutron.git
cd neutron
cmake -B build -S . -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
build\Release\neutron.exe --version
```

## First Program

Create `hello.nt`:
```neutron
say("Hello from Neutron!");

var name = "World";
say("Hello, ${name}!");
```

Run it:
```bash
./neutron hello.nt
```

## Run Tests

Verify your installation:

```bash
# Linux/macOS/MSYS2
./run_tests.sh

# Windows PowerShell
.\run_tests.ps1
```

Expected output:
```
================================
  Test Summary
================================
Total tests: 21
Passed: 21
Failed: 0

All tests passed!
```

## Learn More

- **[README.md](../README.md)** - Complete feature overview
- **[BUILD.md](BUILD.md)** - Comprehensive build instructions
- **[Language Reference](language_reference.md)** - Full language syntax
- **[Examples](../programs/)** - Example programs

## Quick Reference

### Variables
```neutron
var x = 42;
var name = "Alice";
var isTrue = true;
var empty = nil;

// Multiple declarations
var a = 1, b = 2;
```

### Functions
```neutron
fun greet(name) {
    return "Hello, " + name;
}

say(greet("World"));
```

### Classes
```neutron
class Person {
    var name;
    var age;
    
    init(n, a) {
        this.name = n;
        this.age = a;
    }
    
    fun greet() {
        say("Hi, I'm ${this.name}");
    }
}

var person = Person();
person.init("Alice", 30);
person.greet();
```

### Control Flow
```neutron
// If-else
if (x > 10) {
    say("Big");
} else {
    say("Small");
}

// While loop
var i = 0;
while (i < 5) {
    say(i);
    i = i + 1;
}

// For loop
for (var i = 0; i < 5; i = i + 1) {
    say(i);
}
```

### Arrays
```neutron
var numbers = [1, 2, 3, 4, 5];
say(numbers[0]);  // 1
numbers[0] = 100;
say(numbers);     // [100, 2, 3, 4, 5]
```

### Modules
```neutron
use sys;
use json;
use http;

// File I/O
sys.write("data.txt", "Hello!");
var content = sys.read("data.txt");

// JSON
var obj = {"name": "Neutron"};
var jsonStr = json.stringify(obj);

// HTTP
var response = http.get("https://api.example.com");
say(response.status);
```

## Common Mistakes

⚠️ **Important**: `.length` is a property, not a method!

```neutron
// ❌ Wrong
var arr = [1, 2, 3];
say(arr.length());  // RuntimeError!

// ✅ Correct
say(arr.length);    // 3
```

See the [Common Pitfalls Guide](common-pitfalls.md) for more tips!

## Common Issues

**"curl not found" or "jsoncpp not found"**
- Install missing dependencies (see Prerequisites above)

**"CMake version too old"**
- Install CMake 3.15+ from cmake.org or your package manager

**MSYS2: Wrong terminal**
- Use MINGW64 terminal, not MSYS terminal

**Tests fail**
- Check [Known Issues](known_issues.md)
- Ensure build completed successfully

## Getting Help

- **Documentation:** [docs/](../docs/)
- **Common Pitfalls:** [common-pitfalls.md](common-pitfalls.md)
- **Issues:** [GitHub Issues](https://github.com/yasakei/neutron/issues)
- **Examples:** [programs/](../programs/)

---

**Ready to build something awesome? Start coding! 🚀**
