# Cross-Platform Implementation Guide

## Overview

Neutron now has a lightweight cross-platform abstraction layer that supports **Windows**, **macOS**, and **Linux** without any external dependencies beyond the C++ standard library.

## Architecture

### Platform Abstraction Layer

Located in `src/platform/platform.cpp` and `include/platform/platform.h`, this layer provides a unified API for platform-specific operations.

**Key Features:**
- **Zero Dependencies** - Uses only standard C++ and platform APIs
- **Header-Only Interface** - Clean separation between interface and implementation
- **Minimal Overhead** - Thin wrappers around native APIs
- **Compile-Time Detection** - Uses preprocessor directives for platform selection

## Supported Platforms

| Platform | Macro | Status |
|----------|-------|--------|
| Windows  | `_WIN32` | ✅ Fully Supported |
| macOS    | `__APPLE__` | ✅ Fully Supported |
| Linux    | `__linux__` | ✅ Fully Supported |
| FreeBSD  | `__FreeBSD__` | ✅ Supported |
| OpenBSD  | `__OpenBSD__` | ✅ Supported |
| NetBSD   | `__NetBSD__` | ✅ Supported |

## API Reference

### File Operations

```cpp
// Check if file/directory exists
bool fileExists(const std::string& path);

// Check if path is a directory
bool isDirectory(const std::string& path);

// List directory contents
std::vector<std::string> listDirectory(const std::string& path);

// File manipulation
bool copyFile(const std::string& from, const std::string& to);
bool moveFile(const std::string& from, const std::string& to);
bool removeFile(const std::string& path);
```

### Directory Operations

```cpp
// Create directory (0755 permissions on Unix)
bool createDirectory(const std::string& path);

// Remove empty directory
bool removeDirectory(const std::string& path);

// Get/set current working directory
std::string getCwd();
bool setCwd(const std::string& path);
```

### Process Operations

```cpp
// Environment variables
std::string getEnv(const std::string& name);
void setEnv(const std::string& name, const std::string& value);

// Execute system command
int execute(const std::string& command);

// Exit process
void exitProcess(int code);
```

### System Information

```cpp
// Platform name: "Windows", "macOS", "Linux", etc.
std::string getPlatform();

// Architecture: "x86_64", "arm64", "x86", etc.
std::string getArch();

// System user information
std::string getUsername();
std::string getHostname();
```

### Path Operations

```cpp
// Join path components with correct separator
std::string joinPath(const std::string& a, const std::string& b);

// Get platform-specific path separator ("/" or "\\")
std::string getPathSeparator();
```

## Platform-Specific Details

### Windows Implementation

- Uses Win32 API (`windows.h`)
- Functions: `GetFileAttributesA`, `CopyFileA`, `MoveFileA`, etc.
- Path separator: `\\`
- UTF-8 support enabled via `/utf-8` compiler flag

### macOS/Linux Implementation

- Uses POSIX APIs (`unistd.h`, `sys/stat.h`, `dirent.h`)
- Functions: `stat`, `opendir`, `rename`, etc.
- Path separator: `/`
- Standard permissions: `0755` for directories

## Usage in Neutron Scripts

All platform functions are accessible through the `sys` module:

```neutron
use sys;

// Get system information
var info = sys.info();
say("Platform: " + info.platform);    // "Linux", "Windows", "macOS"
say("Architecture: " + info.arch);     // "x86_64", "arm64", etc.
say("User: " + info.user);
say("Hostname: " + info.hostname);

// File operations
sys.write("test.txt", "Hello!");
var content = sys.read("test.txt");
say("Exists: " + sys.exists("test.txt"));

// Directory operations
sys.mkdir("my_dir");
sys.chdir("my_dir");
say("Current dir: " + sys.cwd());

// File manipulation
sys.cp("file1.txt", "file2.txt");
sys.mv("file2.txt", "file3.txt");
sys.rm("file3.txt");

// Environment
var path = sys.env("PATH");
say("PATH: " + path);

// Execute commands (returns exit code)
var code = sys.exec("echo Hello");
say("Exit code: " + code);
```

## Building for Different Platforms

### Linux

**Ubuntu/Debian:**
```bash
# Install dependencies
sudo apt-get update
sudo apt-get install build-essential cmake pkg-config libcurl4-openssl-dev libjsoncpp-dev

# Build
cmake -B build -S .
cmake --build build -j$(nproc)

# Test
./neutron tests/test_cross_platform.nt
```

**Fedora/RHEL:**
```bash
# Install dependencies
sudo dnf install gcc-c++ cmake pkgconfig libcurl-devel jsoncpp-devel

# Build
cmake -B build -S .
cmake --build build -j$(nproc)
```

**Arch Linux:**
```bash
# Install dependencies
sudo pacman -S base-devel cmake curl jsoncpp

# Build
cmake -B build -S .
cmake --build build -j$(nproc)
```

### Windows

**MSYS2 (Recommended):**
```bash
# Install MSYS2 from https://www.msys2.org/
# Open MSYS2 MINGW64 terminal

# Install dependencies
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake \\
          mingw-w64-x86_64-curl mingw-w64-x86_64-jsoncpp make

# Build
cmake -B build -S . -G "MSYS Makefiles"
cmake --build build -j$(nproc)

# Test
./neutron.exe tests/test_cross_platform.nt
```

**Visual Studio:**
```cmd
REM Install Visual Studio 2017 or later with C++ tools
REM Install vcpkg: https://github.com/microsoft/vcpkg

REM Install dependencies with vcpkg
vcpkg install curl jsoncpp --triplet x64-windows

REM Generate Visual Studio solution
cmake -B build -S . ^
  -G "Visual Studio 17 2022" ^
  -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake

REM Build
cmake --build build --config Release

REM Test
build\\Release\\neutron.exe tests\\test_cross_platform.nt
```

**MinGW (Alternative):**
```cmd
REM Install MinGW-w64 from https://www.mingw-w64.org/

cmake -B build -S . -G "MinGW Makefiles"
cmake --build build
```

### macOS

**Using Homebrew:**
```bash
# Install Homebrew if not installed: https://brew.sh

# Install dependencies
brew install cmake curl jsoncpp

# Build
cmake -B build -S .
cmake --build build -j$(sysctl -n hw.ncpu)

# Test
./neutron tests/test_cross_platform.nt
```

**Using Xcode:**
```bash
# Ensure Xcode Command Line Tools are installed
xcode-select --install

# Generate Xcode project
cmake -B build -S . -G Xcode

# Build with Xcode
cmake --build build --config Release

# Or open in Xcode
open build/neutron.xcodeproj
```

**For Apple Silicon (M1/M2/M3):**
```bash
# Dependencies
brew install cmake curl jsoncpp

# Build with architecture specification
cmake -B build -S . -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build -j$(sysctl -n hw.ncpu)
```

## CMake Configuration

The platform is automatically detected in `CMakeLists.txt`:

```cmake
# Platform detection
if(WIN32)
    add_definitions(-DNEUTRON_WINDOWS)
elseif(APPLE)
    add_definitions(-DNEUTRON_MACOS)
elseif(UNIX)
    add_definitions(-DNEUTRON_LINUX)
endif()

# Windows-specific flags
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -O2)
endif()
```

## Testing

Run the platform test suite:

```bash
./neutron test_platform.nt
```

Expected output:
```
=== Cross-Platform Test ===

Platform: [Your Platform]
Architecture: [Your Arch]
User: [Your Username]
Hostname: [Your Hostname]
Current directory: [Your Path]

Testing file operations...
File content: Hello from [Platform]!
File exists: true

Testing directory operations...
Directory created: test_dir
Directory exists: true
File copied to test_dir/copy.txt
Cleanup complete

=== All platform tests passed! ===
```

## Benefits

1. **No External Dependencies** - No vcpkg, conan, or other package managers needed
3. **Fast Compilation** - No heavy external libraries to link
4. **Easy Distribution** - All code is in the repository
5. **Simple Maintenance** - Clear, straightforward implementation
6. **Future-Proof** - Easy to extend for new platforms

## Future Enhancements

Potential additions (if needed):

- File watching / monitoring
- Process spawning with pipes
- Network interfaces enumeration
- Dynamic library loading
- Thread-safe file locking
- Memory-mapped files
- Symbolic link operations

## Troubleshooting

### Windows Build Issues
- Ensure Visual Studio or Build Tools are installed
- Use `/utf-8` flag for proper Unicode support
- May need to link against `user32.lib` and `advapi32.lib`

### macOS Build Issues
- Ensure Xcode Command Line Tools are installed: `xcode-select --install`
- May need to specify SDK: `cmake -DCMAKE_OSX_SYSROOT=/path/to/sdk`

### Linux Build Issues
- Ensure build essentials are installed: `sudo apt install build-essential cmake`
- GCC 7+ or Clang 5+ required for C++17 support

---

**Implementation Date:** October 5, 2025  
**Status:** Production Ready ✅
