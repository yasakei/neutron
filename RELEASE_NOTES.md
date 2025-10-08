# Release Build System Documentation

This document summarizes the release build system for Neutron and Box.

## Overview

Neutron now has comprehensive cross-platform release scripts that build:
- Neutron interpreter/compiler
- Box package manager
- Complete distribution packages for Linux, macOS, and Windows

## Release Scripts

### Linux/macOS: `scripts/make_release.sh`

Bash script that:
- Detects OS (Linux or macOS)
- Extracts version from CHANGELOG.md
- Builds Neutron and Box in Release mode
- Runs test suite (optional)
- Creates distribution package with:
  - Binaries (neutron, box)
  - Standard library (.nt files)
  - C API headers
  - Documentation
  - Installation script
- Generates tarball with checksums (SHA256, MD5)

**Usage:**

```bash
./scripts/make_release.sh
```

**Output:**
```
releases/neutron-v1.0.2-alpha-linux-x64.tar.gz
releases/neutron-v1.0.2-alpha-linux-x64.tar.gz.sha256
releases/neutron-v1.0.2-alpha-linux-x64.tar.gz.md5
```

### Windows: `scripts/make_release.ps1`

PowerShell script that:
- Extracts version from CHANGELOG.md
- Builds Neutron and Box with Visual Studio
- Runs test suite (optional, use `-SkipTests` to skip)
- Creates distribution package with:
  - Binaries (neutron.exe, box.exe)
  - Standard library (.nt files)
  - C API headers
  - Documentation
  - Installation script (.bat)
- Generates ZIP archive with checksums (SHA256, MD5)

**Usage:**

```powershell
.\scripts\make_release.ps1
# Or skip tests:
.\scripts\make_release.ps1 -SkipTests
```

**Output:**
```
releases\neutron-v1.0.2-alpha-windows-x64.zip
releases\neutron-v1.0.2-alpha-windows-x64.zip.sha256
releases\neutron-v1.0.2-alpha-windows-x64.zip.md5
```

## Distribution Package Contents

Each release package includes:

```
neutron-v1.0.2-alpha-{platform}-x64/
├── bin/
│   ├── neutron         # Neutron interpreter/compiler
│   └── box             # Box package manager
├── lib/
│   └── *.nt            # Standard library modules
├── include/
│   └── neutron/
│       ├── capi.h      # C API for module development
│       ├── *.h         # Other headers
│       └── */          # Module-specific headers
├── docs/
│   ├── BUILD.md
│   ├── language_reference.md
│   ├── module_system.md
│   └── ...             # All documentation
├── README.md
├── LICENSE
├── CHANGELOG.md
├── install.sh          # Linux/macOS installer
└── README.txt          # Package-specific readme
```

## Installation Scripts

### Linux/macOS: `install.sh`

Installs Neutron to `/usr/local` (or custom `$PREFIX`):

```bash
cd neutron-v1.0.2-alpha-linux-x64
./install.sh
```

### Windows: `install.bat`

Installs Neutron to `C:\Program Files\Neutron`:

```cmd
neutron-v1.0.2-alpha-windows-x64\install.bat
```

## Documentation Structure

### Main Repository (neutron/)

- `README.md` - Updated with Box package manager section
- `docs/` - Language and build documentation
- `scripts/` - Release build scripts
  - `make_release.sh` - Linux/macOS release builder
  - `make_release.ps1` - Windows release builder

### Box Package Manager (nt-box/)

- `README.md` - Quick start guide
- `docs/BOX_GUIDE.md` - Comprehensive Box guide
- `docs/COMMANDS.md` - Detailed command reference
- `docs/MODULE_DEVELOPMENT.md` - Native module development guide
- `docs/CROSS_PLATFORM.md` - Platform-specific notes

## Box Package Manager Features

Box is fully cross-platform and includes:

### Commands
- `box install <module>[@version]` - Install modules from NUR
- `box list` - List installed modules
- `box search [query]` - Search available modules
- `box remove <module>` - Remove a module
- `box build` - Build a native module
- `box info <module>` - Show module information

### Cross-Platform Build Support

**Linux:**
- Compiler: g++ or clang++
- Flags: `-std=c++17 -shared -fPIC`
- Output: `.so` shared libraries

**macOS:**
- Compiler: clang++
- Flags: `-std=c++17 -dynamiclib -fPIC`
- Output: `.dylib` dynamic libraries

**Windows:**
- Compiler: MSVC cl
- Flags: `/std:c++17 /LD /MD`
- Output: `.dll` dynamic-link libraries

### Module Installation

Modules install to `.box/modules/` in your project directory:

```
.box/
└── modules/
    └── base64/
        ├── base64.so       # Linux
        ├── base64.dll      # Windows
        ├── base64.dylib    # macOS
        └── metadata.json
```

Neutron automatically searches `.box/modules/` when loading modules with `use`.

## Release Workflow

### 1. Update Version

Edit `CHANGELOG.md` and add new version entry:

```markdown
## v1.0.3-alpha

- New feature X
- Bug fix Y
```

### 2. Build Release Packages

**On Linux:**
```bash
./scripts/make_release.sh
```

**On macOS:**
```bash
./scripts/make_release.sh
```

**On Windows:**
```powershell
.\scripts\make_release.ps1
```

### 3. Test Packages

Extract and test each package:

```bash
# Linux/macOS
tar -xzf releases/neutron-v1.0.3-alpha-linux-x64.tar.gz
cd neutron-v1.0.3-alpha-linux-x64
./install.sh
neutron --version
box --help
```

```powershell
# Windows
Expand-Archive releases\neutron-v1.0.3-alpha-windows-x64.zip
cd neutron-v1.0.3-alpha-windows-x64
.\install.bat
neutron --version
box --help
```

### 4. Create GitHub Release

Upload packages to GitHub:

```bash
gh release create v1.0.3-alpha \
  releases/neutron-v1.0.3-alpha-linux-x64.tar.gz \
  releases/neutron-v1.0.3-alpha-macos-x64.tar.gz \
  releases/neutron-v1.0.3-alpha-windows-x64.zip \
  --title "Neutron v1.0.3-alpha" \
  --notes-file CHANGELOG.md
```

## Building From Source

### Prerequisites

**All Platforms:**
- CMake 3.15+
- C++17 compiler

**Linux:**
```bash
sudo apt-get install build-essential cmake libcurl4-openssl-dev libjsoncpp-dev
```

**macOS:**
```bash
brew install cmake curl jsoncpp
```

**Windows:**
```powershell
# Install Visual Studio 2019+ with C++ tools
choco install cmake
```

### Build Steps

```bash
# Clone repository
git clone https://github.com/yasakei/neutron.git
cd neutron

# Build Neutron
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Build Box
cd nt-box
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd ..

# Test
./build/neutron tests/test_match.nt
./nt-box/build/box --help
```

## Next Steps: Separate nt-box Repository

To create a separate repository for Box:

### 1. Create GitHub Repository

```bash
gh repo create yasakei/nt-box --public \
  --description "Box Package Manager for Neutron" \
  --gitignore C++ \
  --license MIT
```

### 2. Initialize nt-box Repository

```bash
cd nt-box
git init
git add .
git commit -m "Initial commit: Box package manager"
git branch -M main
git remote add origin https://github.com/yasakei/nt-box.git
git push -u origin main
```

### 3. Remove from Neutron and Add as Submodule

```bash
cd ..
git rm -r nt-box
git commit -m "Remove nt-box directory (will be added as submodule)"
git submodule add https://github.com/yasakei/nt-box.git nt-box
git commit -m "Add nt-box as submodule"
git push
```

### 4. Update Neutron CMakeLists.txt (if needed)

Ensure the submodule path is correct:

```cmake
# Add Box subdirectory
add_subdirectory(nt-box)
```

### 5. Clone with Submodules

Users will clone with:

```bash
git clone --recurse-submodules https://github.com/yasakei/neutron.git
```

Or initialize after cloning:

```bash
git clone https://github.com/yasakei/neutron.git
cd neutron
git submodule update --init --recursive
```

## Summary

✅ **Completed:**
- Cross-platform release scripts for Linux/macOS/Windows
- Comprehensive Box documentation (BOX_GUIDE, COMMANDS, MODULE_DEVELOPMENT, CROSS_PLATFORM)
- Updated main README with Box package manager section
- Distribution package creation with installation scripts
- Checksum generation (SHA256, MD5)

📦 **Ready to Create:**
- Separate nt-box GitHub repository
- Git submodule linking

🚀 **Ready for Release:**
- Run `./scripts/make_release.sh` on Linux/macOS
- Run `.\scripts\make_release.ps1` on Windows
- Upload to GitHub Releases
