# Building Neutron from Source

This guide provides comprehensive instructions for building Neutron on all supported platforms.

## Table of Contents
- [Prerequisites](#prerequisites)
- [Linux](#linux)
- [macOS](#macos)
- [Windows](#windows)
- [Build Options](#build-options)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### Required Software

- **CMake** 3.15 or higher
- **C++17 compiler**:
  - GCC 7.0+
  - Clang 5.0+
  - Visual Studio 2017+
  - MSVC 19.14+

### Required Libraries

- **libcurl** - HTTP client functionality
- **jsoncpp** - JSON parsing and generation

---

## Linux

### Ubuntu / Debian

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libcurl4-openssl-dev \
    libjsoncpp-dev

# Clone repository (if not already done)
git clone https://github.com/yasakei/neutron.git
cd neutron

# Build
cmake -B build -S .
cmake --build build -j$(nproc)

# Verify build
./neutron --version

# Run tests
./run_tests.sh
```

### Fedora / RHEL / CentOS

```bash
# Fedora
sudo dnf install -y \
    gcc-c++ \
    cmake \
    pkgconfig \
    libcurl-devel \
    jsoncpp-devel

# RHEL/CentOS (enable EPEL first)
sudo yum install -y epel-release
sudo yum install -y \
    gcc-c++ \
    cmake3 \
    pkgconfig \
    libcurl-devel \
    jsoncpp-devel

# Build
cmake -B build -S .
cmake --build build -j$(nproc)

# Verify
./neutron --version
```

### Arch Linux

```bash
# Install dependencies
sudo pacman -S --needed \
    base-devel \
    cmake \
    curl \
    jsoncpp

# Build
cmake -B build -S .
cmake --build build -j$(nproc)

# Verify
./neutron --version
```

### Alpine Linux

```bash
# Install dependencies
apk add --no-cache \
    build-base \
    cmake \
    curl-dev \
    jsoncpp-dev

# Build
cmake -B build -S .
cmake --build build -j$(nproc)

# Verify
./neutron --version
```

---

## macOS

### Using Homebrew (Recommended)

```bash
# Install Homebrew if not installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake curl jsoncpp

# Clone repository
git clone https://github.com/yasakei/neutron.git
cd neutron

# Build
cmake -B build -S .
cmake --build build -j$(sysctl -n hw.ncpu)

# Verify
./neutron --version

# Run tests
./run_tests.sh
```

### Apple Silicon (M1/M2/M3)

```bash
# Install dependencies
brew install cmake curl jsoncpp

# Build for ARM64
cmake -B build -S . -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build -j$(sysctl -n hw.ncpu)

# Or build Universal Binary (x86_64 + arm64)
cmake -B build -S . -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake --build build -j$(sysctl -n hw.ncpu)
```

### Using Xcode

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install dependencies
brew install cmake curl jsoncpp

# Generate Xcode project
cmake -B build -S . -G Xcode

# Build from command line
cmake --build build --config Release

# Or open in Xcode IDE
open build/neutron.xcodeproj
```

---

## Windows

### MSYS2 (Recommended)

MSYS2 provides a Unix-like environment on Windows with easy package management.

```bash
# 1. Download and install MSYS2 from https://www.msys2.org/

# 2. Open MSYS2 MINGW64 terminal (not MSYS2 MSYS!)
#    The prompt should show: MINGW64

# 3. Update package database
pacman -Syu
# Close terminal and reopen MSYS2 MINGW64

# 4. Install build tools and dependencies
pacman -S --needed \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-curl \
    mingw-w64-x86_64-jsoncpp \
    make

# Note: dlfcn-win32 is NOT required - Neutron includes a built-in Windows shim
# for dlopen/dlsym/dlclose that will be used automatically if dlfcn-win32 is not found

# 5. Clone repository
git clone https://github.com/yasakei/neutron.git
cd neutron

# 6. Build
cmake -B build -S . -G "MSYS Makefiles"
cmake --build build -j$(nproc)

# MinGW runtime DLLs are automatically copied to the build directory
# so the executable works from PowerShell or outside the MSYS environment

# 7. Verify
./build/neutron.exe --version

# 8. Run tests
bash run_tests.sh
# Or from PowerShell: ./run_tests.ps1
```

### Visual Studio 2019/2022 with vcpkg [powershell]

```pwsh
REM 1. Install Visual Studio with C++ Desktop Development workload
REM    Download from: https://visualstudio.microsoft.com/

REM 2. Clone Neutron repository
git clone https://github.com/yasakei/neutron.git
cd neutron

REM 3. Install vcpkg
git submodule add https://github.com/microsoft/vcpkg.git vcpkg
./vcpkg/bootstrap-vcpkg.bat

REM 4. Install dependencies
./vcpkg/vcpkg install dlfcn-win32 curl:x64-windows jsoncpp:x64-windows                                


REM 5. Generate Visual Studio solution
cmake --preset winmsvc

REM 6. Build
cmake --build build --config Release

REM 7. Verify
.\build\Release\neutron.exe --version

REM 8. Run tests (PowerShell)
.\run_tests.ps1
```

### MinGW-w64 (Alternative)

```cmd
REM 1. Install MinGW-w64 from https://www.mingw-w64.org/

REM 2. Add MinGW bin directory to PATH
REM    Example: C:\mingw64\bin

REM 3. Install dependencies manually or use vcpkg

REM 4. Generate Makefiles
cmake -B build -S . -G "MinGW Makefiles"

REM 5. Build
cmake --build build -j

REM 6. Verify
neutron.exe --version
```

---

## Build Options

### CMake Configuration Options

```bash
# Debug build (with symbols, no optimization)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug

# Release build (optimized, no debug symbols)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Release with debug info
cmake -B build -S . -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Specify compiler
cmake -B build -S . -DCMAKE_CXX_COMPILER=clang++

# Custom install prefix
cmake -B build -S . -DCMAKE_INSTALL_PREFIX=/usr/local

# Verbose build output
cmake --build build --verbose
```

### Parallel Build

```bash
# Linux/macOS - use all CPU cores
cmake --build build -j$(nproc)         # Linux
cmake --build build -j$(sysctl -n hw.ncpu)  # macOS

# Windows - use all cores
cmake --build build -j %NUMBER_OF_PROCESSORS%

# Or specify number of jobs
cmake --build build -j 4
```

### Clean Build

```bash
# Remove build directory
rm -rf build

# Or clean without removing
cmake --build build --target clean

# Then rebuild
cmake -B build -S .
cmake --build build
```

---

## Troubleshooting

### Linux Issues

**"curl not found" or "jsoncpp not found":**
```bash
# Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev libjsoncpp-dev

# Fedora
sudo dnf install libcurl-devel jsoncpp-devel

# Arch
sudo pacman -S curl jsoncpp
```

**CMake version too old:**
```bash
# Install newer CMake from snap
sudo snap install cmake --classic

# Or download from cmake.org
wget https://github.com/Kitware/CMake/releases/download/v3.25.0/cmake-3.25.0-linux-x86_64.sh
sudo sh cmake-3.25.0-linux-x86_64.sh --prefix=/usr/local --skip-license
```

**Compiler too old (need C++17):**
```bash
# Ubuntu - install newer GCC
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install g++-9

# Use newer compiler
cmake -B build -S . -DCMAKE_CXX_COMPILER=g++-9
```

### macOS Issues

**CMake can't find libraries:**
```bash
# Set PKG_CONFIG_PATH
export PKG_CONFIG_PATH="$(brew --prefix)/lib/pkgconfig"
cmake -B build -S .

# Or specify paths manually
cmake -B build -S . \
    -DCURL_LIBRARY=$(brew --prefix)/lib/libcurl.dylib \
    -DJSONCPP_LIBRARY=$(brew --prefix)/lib/libjsoncpp.dylib
```

**Xcode Command Line Tools not installed:**
```bash
xcode-select --install
```

**Library linking errors:**
```bash
# Update Homebrew
brew update
brew upgrade

# Reinstall dependencies
brew reinstall curl jsoncpp
```

### Windows Issues

**MSYS2: Wrong terminal:**
```
ERROR: Make sure you're in MINGW64 terminal, not MSYS!
The prompt should show: MINGW64

Solution: Open "MSYS2 MINGW64" from Start Menu
```

**MSYS2: pacman fails to install:**
```bash
# Update package database
pacman -Syu

# If still fails, update mirrors
pacman -S pacman-mirrors
```

**Visual Studio: Can't find vcpkg:**
```cmd
REM Make sure path is correct in CMAKE_TOOLCHAIN_FILE
REM Example:
cmake -B build -S . ^
    -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

**Visual Studio: Missing dependencies:**
```cmd
REM Install dependencies for your architecture
vcpkg install curl:x64-windows jsoncpp:x64-windows

REM Or for x86
vcpkg install curl:x86-windows jsoncpp:x86-windows

REM Verify installation
vcpkg list
```

**Link errors (LNK1104, LNK2019):**
- Ensure all dependencies are installed
- Check architecture matches (x64 vs x86)
- Try clean rebuild: `cmake --build build --clean-first`

---

## Verification

After successful build, verify your installation:

```bash
# Check version
./neutron --version

# Run a simple script
echo 'say("Hello from Neutron!");' > test.nt
./neutron test.nt

# Run test suite (all 21 tests should pass)
./run_tests.sh           # Linux/macOS/MSYS2
./run_tests.ps1          # Windows PowerShell

# Expected output:
# ================================
#   Test Summary
# ================================
# Total tests: 21
# Passed: 21
# Failed: 0
# 
# All tests passed!
```

---

## Advanced Topics

### Static Linking

```bash
# Linux - static libstdc++
cmake -B build -S . -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++"

# Windows - static runtime
cmake -B build -S . -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded"
```

### Cross-Compilation

```bash
# Example: Compile for ARM on x86_64
cmake -B build -S . \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=arm \
    -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc \
    -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++
```

### Custom Installation

```bash
# Install to system
sudo cmake --install build

# Or specify prefix
cmake -B build -S . -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --install build
```

---

## Getting Help

If you encounter issues not covered here:

1. Check [Known Issues](known_issues.md)
2. Search [GitHub Issues](https://github.com/yasakei/neutron/issues)
3. Create a new issue with:
   - Your OS and version
   - CMake version (`cmake --version`)
   - Compiler version (`g++ --version` or `clang++ --version`)
   - Full error output
   - Steps to reproduce

---

**Last Updated:** October 5, 2025  
**Neutron Version:** 1.0.0+
