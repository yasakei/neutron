# Scripts Documentation

This directory contains build automation and release management scripts for the Neutron project.

## Overview

| Script | Platform | Purpose |
|--------|----------|---------|
| `build_all.sh` | Linux/macOS | Build Neutron + Box |
| `build_all.ps1` | Windows | Build Neutron + Box |
| `make_release.sh` | Linux/macOS | Create release packages |
| `make_release.ps1` | Windows | Create release packages |

---

## Build Scripts

### build_all.sh (Linux/macOS)

Builds both Neutron interpreter and Box package manager in one command.

**Usage:**
```bash
./scripts/build_all.sh [OPTIONS]
```

**Options:**
- `-h, --help` - Show help message
- `-d, --debug` - Build in Debug mode (default: Release)
- `-c, --clean` - Clean build directories before building
- `-t, --test` - Run test suite after building
- `-v, --verbose` - Verbose build output
- `-j, --jobs N` - Number of parallel jobs (default: auto-detected)

**Examples:**
```bash
# Standard release build
./scripts/build_all.sh

# Debug build with tests
./scripts/build_all.sh -d -t

# Clean build with 8 parallel jobs
./scripts/build_all.sh -c -j 8

# Verbose debug build
./scripts/build_all.sh -d -v
```

**Requirements:**
- CMake 3.15+
- GCC 9+ or Clang 10+
- Make or Ninja
- Git (for version detection)

**Output:**
```
build/neutron              # Neutron interpreter
nt-box/build/box          # Box package manager
```

---

### build_all.ps1 (Windows)

Windows equivalent of build_all.sh with support for both MSVC and MINGW64.

**Usage:**
```powershell
.\scripts\build_all.ps1 [OPTIONS]
```

**Options:**
- `-Help` - Show help message
- `-Debug` - Build in Debug mode (default: Release)
- `-Clean` - Clean build directories before building
- `-Test` - Run test suite after building
- `-Verbose` - Verbose build output
- `-Jobs N` - Number of parallel jobs (default: auto-detected)
- `-MINGW` - Force MINGW64/GCC build
- `-MSVC` - Force MSVC build (default if available)

**Examples:**
```powershell
# Standard release build (auto-detects compiler)
.\scripts\build_all.ps1

# Debug build with tests
.\scripts\build_all.ps1 -Debug -Test

# Force MINGW64 build
.\scripts\build_all.ps1 -MINGW

# Clean MSVC build with 8 jobs
.\scripts\build_all.ps1 -Clean -MSVC -Jobs 8
```

**Requirements:**

**For MSVC:**
- Visual Studio 2019+ or Build Tools
- CMake 3.15+
- Run from Developer Command Prompt

**For MINGW64:**
- MSYS2 with MINGW64 toolchain
- CMake 3.15+
- Run from MINGW64 terminal

**Output:**
```
# MSVC build
build\Release\neutron.exe
nt-box\build\Release\box.exe

# MINGW64 build
build\neutron.exe
nt-box\build\box.exe
```

---

## Release Scripts

### make_release.sh (Linux/macOS)

Creates distribution packages with binaries, libraries, headers, and documentation.

**Usage:**
```bash
./scripts/make_release.sh [OPTIONS]
```

**Options:**
- `-h, --help` - Show help message
- `-s, --skip-tests` - Skip running tests before packaging
- `-k, --keep-build` - Keep build artifacts after packaging

**Examples:**
```bash
# Create release package with tests
./scripts/make_release.sh

# Create package without tests
./scripts/make_release.sh -s

# Keep build directory
./scripts/make_release.sh -k
```

**What it does:**
1. Extracts version from CHANGELOG.md
2. Runs test suite (unless `-s` specified)
3. Builds Neutron and Box in Release mode
4. Strips debug symbols
5. Creates distribution directory structure
6. Copies binaries, libraries, headers, docs
7. Creates tarball (.tar.gz)
8. Generates SHA256 and MD5 checksums

**Output:**
```
releases/
├── neutron-v1.0.4-alpha-linux-x64.tar.gz
├── neutron-v1.0.4-alpha-linux-x64.tar.gz.sha256
└── neutron-v1.0.4-alpha-linux-x64.tar.gz.md5
```

**Package contents:**
```
neutron-v1.0.4-alpha-linux-x64/
├── bin/
│   ├── neutron         # Interpreter
│   └── box             # Package manager
├── lib/
│   └── *.nt            # Standard library
├── include/
│   └── neutron/        # C API headers
├── docs/               # Documentation
├── README.md
├── LICENSE
├── CHANGELOG.md
└── install.sh          # Installation script
```

---

### make_release.ps1 (Windows)

Windows release packaging script.

**Usage:**
```powershell
.\scripts\make_release.ps1 [OPTIONS]
```

**Options:**
- `-Help` - Show help message
- `-SkipTests` - Skip running tests before packaging
- `-KeepBuild` - Keep build artifacts after packaging

**Examples:**
```powershell
# Create release package with tests
.\scripts\make_release.ps1

# Create package without tests
.\scripts\make_release.ps1 -SkipTests
```

**What it does:**
1. Extracts version from CHANGELOG.md
2. Runs test suite (unless `-SkipTests` specified)
3. Builds with MSVC in Release mode
4. Creates distribution directory structure
5. Copies binaries, DLLs, libraries, headers, docs
6. Creates ZIP archive
7. Generates SHA256 and MD5 checksums

**Output:**
```
releases\
├── neutron-v1.0.4-alpha-windows-x64.zip
├── neutron-v1.0.4-alpha-windows-x64.zip.sha256
└── neutron-v1.0.4-alpha-windows-x64.zip.md5
```

**Package contents:**
```
neutron-v1.0.4-alpha-windows-x64\
├── bin\
│   ├── neutron.exe     # Interpreter
│   ├── box.exe         # Package manager
│   └── *.dll           # Runtime libraries
├── lib\
│   └── *.nt            # Standard library
├── include\
│   └── neutron\        # C API headers
├── docs\               # Documentation
├── README.md
├── LICENSE
├── CHANGELOG.md
└── install.bat         # Installation script
```

---

## Environment Setup

### Linux

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install build-essential cmake git
```

**Fedora:**
```bash
sudo dnf install gcc-c++ cmake make git
```

**Arch:**
```bash
sudo pacman -S base-devel cmake git
```

### macOS

**Using Homebrew:**
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install CMake
brew install cmake
```

### Windows (MSVC)

1. Install Visual Studio 2019+ or Build Tools
2. Select "Desktop development with C++"
3. Install CMake: https://cmake.org/download/
4. Run scripts from "Developer Command Prompt for VS"

### Windows (MINGW64)

1. Install MSYS2: https://www.msys2.org/
2. Open MINGW64 terminal
3. Install packages:
```bash
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-cmake
pacman -S make
```

---

## CI/CD Integration

### GitHub Actions

See `.github/workflows/` for CI/CD workflows:
- `build.yml` - Build and test on all platforms
- `release.yml` - Automated release packaging

### Local Release Workflow

**Step 1: Update version in CHANGELOG.md**
```markdown
## [1.0.4-alpha] - 2025-10-08
```

**Step 2: Run release script**
```bash
# Linux/macOS
./scripts/make_release.sh

# Windows
.\scripts\make_release.ps1
```

**Step 3: Verify checksums**
```bash
# Linux/macOS
sha256sum -c releases/*.sha256

# Windows
certutil -hashfile releases\*.zip SHA256
```

**Step 4: Test installation**
```bash
# Extract package
tar -xzf releases/neutron-*.tar.gz
cd neutron-*/

# Run installation
sudo ./install.sh

# Verify
neutron --version
box --version
```

**Step 5: Create GitHub release**
```bash
gh release create v1.0.4-alpha \
  releases/neutron-*.tar.gz \
  releases/neutron-*.zip \
  --title "Neutron v1.0.4-alpha" \
  --notes-file logs/v1.0.4-alpha.md
```

---

## Troubleshooting

### Build Issues

**Problem:** CMake not found
```bash
# Linux
sudo apt-get install cmake

# macOS
brew install cmake

# Windows
# Add CMake to PATH
```

**Problem:** Compiler not found
```bash
# Linux
sudo apt-get install build-essential

# macOS
xcode-select --install

# Windows MINGW64
pacman -S mingw-w64-x86_64-gcc
```

**Problem:** Build fails with linking errors
```bash
# Clean and rebuild
./scripts/build_all.sh -c
```

### Release Issues

**Problem:** Version not detected from CHANGELOG.md
- Ensure CHANGELOG.md has proper version format: `## [X.Y.Z] - YYYY-MM-DD`

**Problem:** Tests fail during release
```bash
# Skip tests (not recommended for production releases)
./scripts/make_release.sh -s
```

**Problem:** Checksum verification fails
```bash
# Regenerate checksums
sha256sum releases/*.tar.gz > releases/*.sha256
md5sum releases/*.tar.gz > releases/*.md5
```

---

## Script Architecture

### Build Scripts

```
build_all.sh / build_all.ps1
├── Check prerequisites (cmake, compiler)
├── Configure CMake
├── Build Neutron
│   ├── cmake -B build
│   └── cmake --build build
├── Build Box
│   ├── cmake -B nt-box/build
│   └── cmake --build nt-box/build
└── Run tests (optional)
    └── ./run_tests.sh
```

### Release Scripts

```
make_release.sh / make_release.ps1
├── Extract version from CHANGELOG.md
├── Run tests (optional)
├── Build in Release mode
│   ├── Build Neutron
│   └── Build Box
├── Create distribution directory
│   ├── bin/ (binaries)
│   ├── lib/ (standard library)
│   ├── include/ (headers)
│   └── docs/ (documentation)
├── Create archive (.tar.gz or .zip)
└── Generate checksums (SHA256, MD5)
```

---

## Best Practices

### Before Building

1. **Update dependencies:**
   ```bash
   # Linux
   sudo apt-get update
   
   # macOS
   brew update
   
   # Windows MINGW64
   pacman -Syu
   ```

2. **Clean previous builds:**
   ```bash
   ./scripts/build_all.sh -c
   ```

3. **Use Release mode for production:**
   ```bash
   ./scripts/build_all.sh  # Default is Release
   ```

### Before Releasing

1. **Run full test suite:**
   ```bash
   ./scripts/build_all.sh -t
   ```

2. **Update documentation:**
   - Update CHANGELOG.md with version and changes
   - Update README.md if needed
   - Create logs/vX.Y.Z.md with detailed changes

3. **Test on all platforms:**
   - Linux (GCC and Clang)
   - macOS (Clang)
   - Windows (MSVC and MINGW64)

4. **Verify package contents:**
   ```bash
   tar -tzf releases/*.tar.gz
   ```

5. **Test installation:**
   ```bash
   # Extract and install in test environment
   # Verify all binaries work
   ```

---

## Platform-Specific Notes

### Linux

- **Preferred compiler:** GCC 9+ (better compatibility)
- **Alternative:** Clang 10+ (faster builds)
- **Distribution:** .tar.gz
- **Installation prefix:** /usr/local

### macOS

- **Required:** Xcode Command Line Tools
- **Compiler:** Clang (Apple's version)
- **Distribution:** .tar.gz
- **Code signing:** May be required for distribution
- **Installation prefix:** /usr/local

### Windows (MSVC)

- **Required:** Visual Studio 2019+ or Build Tools
- **Generator:** Visual Studio or Ninja
- **Distribution:** .zip
- **Installation prefix:** C:\Program Files\Neutron
- **Runtime:** Requires MSVC runtime (usually included)

### Windows (MINGW64)

- **Required:** MSYS2 with MINGW64 packages
- **Generator:** MSYS Makefiles
- **Distribution:** .zip (same as MSVC)
- **Installation prefix:** C:\Program Files\Neutron or /usr/local (MSYS2)
- **Runtime:** Requires mingw64 DLLs (libgcc, libstdc++)

---

## Contributing

When adding new scripts:

1. **Follow naming convention:**
   - Unix: `script_name.sh`
   - Windows: `script_name.ps1`

2. **Add help/usage:**
   - `-h, --help` for Unix
   - `-Help` for Windows

3. **Use proper error handling:**
   - `set -e` for bash
   - `$ErrorActionPreference = "Stop"` for PowerShell

4. **Add to this documentation**

5. **Test on all platforms**

---

## License

These scripts are part of the Neutron project and follow the same license.

---

## Support

For issues with scripts:
1. Check this documentation
2. Verify prerequisites are installed
3. Open an issue on GitHub with:
   - Script name and command used
   - Error message
   - OS and compiler version
   - CMake version
