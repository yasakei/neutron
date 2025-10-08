# Cross-Platform Development

Platform-specific notes for building and using Box and Neutron modules.

## Supported Platforms

Box and Neutron fully support:
- **Linux** (x86_64, ARM64)
- **macOS** (x86_64, Apple Silicon)
- **Windows** (x86_64)

## Platform Detection

Box automatically detects your platform and handles:
- Compiler selection (g++/clang++/MSVC)
- Linker flags (-shared/-dynamiclib//LD)
- Library extensions (.so/.dylib/.dll)
- Include paths
- System dependencies

## Linux

### Compilers

Box supports both GCC and Clang on Linux:

```sh
# GCC (default if both available)
g++ --version

# Clang
clang++ --version
```

### Build Flags

When building modules on Linux, Box uses:

```sh
-std=c++17           # C++17 standard
-shared              # Create shared library
-fPIC                # Position-independent code
-I/usr/local/include # Include Neutron headers
```

### Library Extension

Linux modules use `.so` (shared object):

```
.box/modules/base64/base64.so
```

### Dependencies

Install required development tools:

```sh
# Ubuntu/Debian
sudo apt-get install build-essential cmake libcurl4-openssl-dev

# Fedora/RHEL
sudo dnf install gcc-c++ cmake libcurl-devel

# Arch Linux
sudo pacman -S base-devel cmake curl
```

### Installation

Install Neutron and Box:

```sh
# Build Neutron
cmake -B build && cmake --build build
sudo cmake --install build

# Build Box
cd nt-box
cmake -B build && cmake --build build
sudo cmake --install build
```

Headers are installed to `/usr/local/include/neutron/`.

### Dynamic Loading

Neutron uses `dlopen()` to load modules at runtime. The Neutron executable must export symbols for native modules to access the C API:

```cmake
target_link_options(neutron PRIVATE "-Wl,--export-dynamic")
```

This is already configured in Neutron's CMakeLists.txt.

---

## macOS

### Compilers

macOS uses Clang (via Xcode Command Line Tools):

```sh
xcode-select --install
clang++ --version
```

### Build Flags

When building modules on macOS, Box uses:

```sh
-std=c++17                # C++17 standard
-dynamiclib               # Create dynamic library
-fPIC                     # Position-independent code
-I/usr/local/include      # Intel Macs
-I/opt/homebrew/include   # Apple Silicon Macs
```

### Library Extension

macOS modules use `.dylib` (dynamic library):

```
.box/modules/base64/base64.dylib
```

### Dependencies

Install dependencies via Homebrew:

```sh
# Install Homebrew (if needed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake curl jsoncpp
```

### Installation

Install Neutron and Box:

```sh
# Build Neutron
cmake -B build && cmake --build build
sudo cmake --install build

# Build Box
cd nt-box
cmake -B build && cmake --build build
sudo cmake --install build
```

**Intel Macs:** Headers install to `/usr/local/include/neutron/`  
**Apple Silicon:** Headers install to `/opt/homebrew/include/neutron/`

### Dynamic Loading

Neutron uses `dlopen()` on macOS. The executable exports symbols via the `-Wl,-export_dynamic` linker flag (automatically configured).

### Code Signing

On macOS Catalina+ with Gatekeeper, you may need to sign modules:

```sh
codesign -s - bin/v1.0.0/mymodule.dylib
```

Or disable Gatekeeper verification (not recommended for production):

```sh
sudo spctl --master-disable
```

---

## Windows

### Compilers

Box supports two compiler toolchains on Windows:

**1. MSVC (Microsoft Visual C++):**

```powershell
# Install Visual Studio Build Tools or Visual Studio
# https://visualstudio.microsoft.com/downloads/

# Verify installation
cl.exe
```

**2. MSYS2/MINGW64 (GCC on Windows):**

```bash
# Install MSYS2 from https://www.msys2.org/
# Then in MINGW64 terminal:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-curl

# Verify installation
g++ --version
```

Box automatically detects which compiler to use based on the environment.

### Build Flags

**MSVC:**

```powershell
/std:c++17                      # C++17 standard
/LD                             # Create DLL
/MD                             # Multi-threaded DLL runtime
/I"C:\Program Files\Neutron\include"
/Fe:output.dll                  # Specify output name
```

**MINGW64:**

```bash
-std=c++17           # C++17 standard
-shared              # Create shared library
-fPIC                # Position-independent code
-I/mingw64/include   # Include paths
```

### Library Extension

Windows modules use `.dll` (dynamic-link library):

```
.box\modules\base64\base64.dll
```

### Dependencies

**Using MSVC with vcpkg:**

```powershell
# Install vcpkg
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Install dependencies
.\vcpkg install curl jsoncpp
```

**Using MSYS2/MINGW64:**

```bash
# In MINGW64 terminal
pacman -S mingw-w64-x86_64-curl mingw-w64-x86_64-jsoncpp
```

### Installation

**Using MSVC:**

```powershell
# Build Neutron
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
cmake --install build

# Build Box
cd nt-box
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
cmake --install build
```

**Using MSYS2/MINGW64:**

```bash
# In MINGW64 terminal
# Build Neutron
cmake -B build -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build

# Build Box
cd nt-box
cmake -B build -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

Headers are installed to:
- MSVC: `C:\Program Files\Neutron\include\neutron\`
- MINGW64: `/mingw64/include/neutron/`

### Dynamic Loading

**MSVC:**
Neutron uses `LoadLibrary()` on Windows. The executable exports C API symbols via:

```cmake
set_target_properties(neutron PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
```

**MINGW64:**
Uses `dlopen()` like Unix systems. Symbols are exported via:

```cmake
target_link_options(neutron PRIVATE "-Wl,--export-dynamic")
```

Both are already configured in Neutron's CMakeLists.txt.

### Environment Detection

Box automatically detects MSYS2/MINGW64 by checking the `MSYSTEM` environment variable:
- `MINGW64`, `MINGW32`, `MSYS` → Use g++ with GCC flags
- Not set → Use MSVC cl with MSVC flags

You can override by setting `NEUTRON_COMPILER`:

```bash
export NEUTRON_COMPILER=g++  # Force GCC
export NEUTRON_COMPILER=cl   # Force MSVC
```

### PATH Configuration

Add Neutron and Box to your PATH:

```powershell
$env:Path += ";C:\Program Files\Neutron\bin;C:\Program Files\Neutron\box"
```

Or permanently via System Properties → Environment Variables.

### DLL Dependencies

If modules link against external DLLs (e.g., `libcurl.dll`), ensure they're accessible:

1. Place DLLs in the same directory as the module
2. Add DLL directory to PATH
3. Use vcpkg's automatic copying:

```cmake
set(CMAKE_TOOLCHAIN_FILE "C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake")
```

---

## Platform-Specific Code

### Conditional Compilation

Use preprocessor macros to handle platform differences:

```cpp
#ifdef _WIN32
    // Windows-specific code
    #include <windows.h>
#elif __APPLE__
    // macOS-specific code
    #include <mach-o/dyld.h>
#elif __linux__
    // Linux-specific code
    #include <unistd.h>
#endif
```

### File Paths

Use forward slashes (`/`) even on Windows (C++ standard library handles conversion):

```cpp
std::ifstream file("data/config.json");  // Works on all platforms
```

Or use `std::filesystem`:

```cpp
#include <filesystem>
namespace fs = std::filesystem;

fs::path configPath = fs::path("data") / "config.json";
```

### Line Endings

Git automatically handles line ending conversion. Configure in `.gitattributes`:

```
* text=auto
*.cpp text eol=lf
*.h text eol=lf
*.sh text eol=lf
*.ps1 text eol=crlf
```

---

## Testing Across Platforms

### GitHub Actions

Use GitHub Actions for CI/CD across all platforms:

```yaml
name: Build and Test

on: [push, pull_request]

jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
    runs-on: ${{ matrix.os }}
    
    steps:
      - uses: actions/checkout@v3
      
      - name: Install Dependencies (Ubuntu)
        if: matrix.os == 'ubuntu-latest'
        run: sudo apt-get install -y build-essential cmake libcurl4-openssl-dev
      
      - name: Install Dependencies (macOS)
        if: matrix.os == 'macos-latest'
        run: brew install cmake curl jsoncpp
      
      - name: Install Dependencies (Windows)
        if: matrix.os == 'windows-latest'
        run: |
          choco install cmake
          vcpkg install curl jsoncpp
      
      - name: Build Neutron
        run: |
          cmake -B build
          cmake --build build
      
      - name: Run Tests
        run: ./build/neutron tests/test_all.nt
```

### Local Testing

Test on virtual machines:

- **VirtualBox:** Create VMs for Linux/Windows
- **UTM (macOS):** ARM/x86 virtualization
- **WSL2 (Windows):** Test Linux builds on Windows

---

## Common Issues

### Issue: "neutron: command not found" (Linux/macOS)

**Solution:** Add Neutron to PATH:

```sh
# Add to ~/.bashrc or ~/.zshrc
export PATH="/usr/local/bin:$PATH"
```

### Issue: "Module not found" after installation

**Solution:** Ensure module has correct extension for your platform:
- Linux: `.so`
- macOS: `.dylib`
- Windows: `.dll`

### Issue: Symbol resolution errors (Linux)

**Problem:** `undefined symbol: neutron_define_native`

**Solution:** Rebuild Neutron with symbol export:

```sh
cmake -B build -DCMAKE_EXE_LINKER_FLAGS="-Wl,--export-dynamic"
cmake --build build
```

### Issue: DLL not found (Windows)

**Problem:** `The code execution cannot proceed because libcurl.dll was not found`

**Solution:** Copy dependencies to executable directory or add to PATH.

### Issue: Permission denied (macOS)

**Problem:** macOS blocks unsigned binaries

**Solution:** Allow in System Preferences → Security & Privacy, or sign the binary:

```sh
codesign -s - neutron
```

### Issue: Compiler not found (Windows)

**Problem (MSVC):** `'cl' is not recognized as an internal or external command`

**Solution:** Run from "Developer Command Prompt for VS":
- Start Menu → Visual Studio → Developer Command Prompt
- Or use `vcvars64.bat`:

```powershell
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
```

**Problem (MINGW64):** `'g++' is not recognized as an internal or external command`

**Solution:** Launch MINGW64 terminal from MSYS2:
- Start Menu → MSYS2 → MINGW64
- Or add MINGW64 to PATH:

```powershell
$env:Path += ";C:\msys64\mingw64\bin"
```

### Issue: Wrong compiler detected (Windows)

**Problem:** Box is using MSVC but you want MINGW64 (or vice versa)

**Solution:** Set the `MSYSTEM` environment variable:

```bash
# Force MINGW64 mode
export MSYSTEM=MINGW64
box build

# Or unset to use MSVC
unset MSYSTEM
box build
```

### Issue: Module builds but fails to load (MINGW64)

**Problem:** DLL dependency issues with MINGW64 modules

**Solution:** Ensure MINGW64 runtime DLLs are accessible:

```bash
# Copy required DLLs to module directory
cp /mingw64/bin/libgcc_s_seh-1.dll bin/v1.0.0/
cp /mingw64/bin/libstdc++-6.dll bin/v1.0.0/
cp /mingw64/bin/libwinpthread-1.dll bin/v1.0.0/

# Or add MINGW64 bin to PATH
export PATH="/mingw64/bin:$PATH"
```

---

## Build Matrix

Quick reference for platform-specific build commands:

| Platform       | Compiler | Command | Output |
|----------------|----------|---------|--------|
| Linux          | g++      | `g++ -std=c++17 -shared -fPIC -o mod.so mod.cpp` | `mod.so` |
| Linux          | clang++  | `clang++ -std=c++17 -shared -fPIC -o mod.so mod.cpp` | `mod.so` |
| macOS          | clang++  | `clang++ -std=c++17 -dynamiclib -o mod.dylib mod.cpp` | `mod.dylib` |
| Windows (MSVC) | cl       | `cl /std:c++17 /LD /MD /Fe:mod.dll mod.cpp` | `mod.dll` |
| Windows (MINGW64) | g++ | `g++ -std=c++17 -shared -fPIC -o mod.dll mod.cpp` | `mod.dll` |

**Note:** Box automatically detects and uses the appropriate compiler for your environment.

---

## See Also

- [Module Development Guide](MODULE_DEVELOPMENT.md) - Creating native modules
- [Box Commands](COMMANDS.md) - Command reference
- [Neutron Build Instructions](../../docs/BUILD.md) - Building Neutron from source
