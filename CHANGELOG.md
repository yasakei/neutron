# Neutron Changelog

All notable changes to the Neutron programming language will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.0.4-alpha] - 2025-10-08

### Added
- **Centralized Version Management**
  - New `Version` class in `types/version.h` for centralized version control
  - Single source of truth for version information across entire project
  - Version components: MAJOR, MINOR, PATCH, STAGE (e.g., "1.0.4-alpha")
  - Platform detection: Reports "Linux", "macOS", "Windows MSVC", "Windows MINGW64"
  - Build date/time tracking via `__DATE__` and `__TIME__` macros
  - Methods: `getVersion()`, `getFullVersion()`, `getMajorMinorPatch()`, `getPlatform()`, `getBuildDate()`

- **Box Package Manager MINGW64 Support**
  - Added full MSYS2/MINGW64 toolchain support for building native modules on Windows
  - Box now automatically detects MINGW64 environment via `MSYSTEM` variable
  - Uses GCC (`g++`) in MINGW64, MSVC (`cl`) otherwise
  - Added MINGW64-specific include paths: `/mingw64/neutron`, `/usr/local/neutron`, `/opt/neutron`
  - Proper flag selection: `-shared -fPIC` for MINGW64, `/LD /MD` for MSVC
  - Skips `-rpath` flag on Windows (not supported by MINGW64)
  - Enables Windows module development without Visual Studio

- **Shared Runtime Library**
  - Added `libneutron_runtime.so` (Linux), `libneutron_runtime.dylib` (macOS), `neutron_runtime.dll` (Windows)
  - Static library (`libneutron_runtime.a`) still built for executable linking
  - Shared library with proper versioning (SOVERSION 1, VERSION 1.0.4)
  - Both libraries built from same source with appropriate flags
  - Enables third-party module developers to link against Neutron runtime

### Changed
- **Version Information Display**
  - `--version` flag now shows detailed version with platform and build date
  - Added `-v` as shorthand for `--version`
  - REPL startup now displays: version, platform, and helpful instructions
  - All version strings now sourced from centralized `Version` class
  - CMakeLists.txt project version updated to 1.0.4
  - Shared library version updated to 1.0.4

- **Box Builder Architecture**
  - `getCompiler()` now checks `MSYSTEM` environment variable
  - `getLinkerFlags()` returns compiler-appropriate flags
  - `generateBuildCommand()` generates correct commands for MSVC or GCC
  - `findNeutronDir()` searches MINGW64 paths when in MINGW environment

- **Binary Conversion (-b) Cross-Platform Support**
  - Binary conversion now works on Windows (MSVC + MINGW64), macOS, and Linux
  - Automatically detects platform and selects appropriate compiler (MSVC/GCC/Clang)
  - Uses correct object file extensions (`.obj` for MSVC, `.o` for GCC)
  - Applies platform-specific flags: `/EHsc /W4` (MSVC), `-Wall -Wextra` (GCC)
  - Links with static runtime library instead of individual object files
  - Automatically adds `.exe` extension on Windows
  - Proper include paths in generated code (`compiler/scanner.h` vs `scanner.h`)
  - macOS support includes CoreFoundation framework
  - Native module compilation within binary conversion respects platform

### Fixed
- **Build Warnings**
  - Fixed unused parameter warnings in `Compiler::visitThisExpr()`, `Compiler::visitBreakStmt()`, `Compiler::visitContinueStmt()`
  - Fixed unused parameter warnings in `Class::call()` for VM and arguments
  - Fixed macro redefinition warning for `PLATFORM_LINUX` in Box (now checks if already defined)
  - Clean build with zero warnings on GCC 15.2.1

- **REPL Error Handling**
  - REPL no longer crashes on syntax errors (e.g., unterminated strings)
  - Errors are caught and displayed gracefully: `Error: <message>`
  - User can continue using REPL after errors
  - Fixes crash with signal: `IOT instruction (core dumped)`

### Documentation
- **New Documentation Files**
  - Added `nt-box/docs/MINGW64_SUPPORT.md` - Comprehensive MINGW64 guide (300+ lines)
  - Added `nt-box/docs/IMPLEMENTATION_MINGW64.md` - Technical implementation details
  - Added `nt-box/docs/README.md` - Documentation index with quick links
  - Added `scripts/make_release.sh` - Linux/macOS release builder
  - Added `scripts/make_release.ps1` - Windows release builder
  - Added `RELEASE_NOTES.md` - Release workflow documentation

- **Updated Documentation**
  - Updated `nt-box/docs/CROSS_PLATFORM.md` - Added MINGW64 compiler section
  - Updated `nt-box/docs/MODULE_DEVELOPMENT.md` - Added MINGW64 compilation examples
  - Updated `nt-box/README.md` - Added MINGW64 to features list
  - Updated `README.md` - Added Box Package Manager section
  - Updated build matrix table to include MINGW64 toolchain

- **Cross-References**
  - Main Neutron docs now reference `nt-box/docs/` for module documentation
  - Centralized Box documentation in nt-box subdirectory
  - Clear separation between language docs and package manager docs

### Release Infrastructure
- **Automated Release Scripts**
  - `scripts/make_release.sh` - Bash script for Linux/macOS releases
    - Auto-detects OS, extracts version from CHANGELOG
    - Builds Neutron + Box in Release mode
    - Creates distribution packages with binaries, libraries, headers, docs
    - Generates tarball with SHA256/MD5 checksums
    - Includes installation script
  - `scripts/make_release.ps1` - PowerShell script for Windows releases
    - Visual Studio/MSVC support
    - Creates ZIP package with checksums
    - Includes Windows installer (.bat)
  - Both scripts support optional test running

---

## [1.0.3-alpha] - 2025-10-07

### Fixed
- **Windows MSYS/MinGW Build Support** - Fixed dlfcn-win32 dependency issue
  - Made `dlfcn-win32` package optional on Windows builds
  - Added internal Windows compatibility shim for `dlopen`/`dlsym`/`dlclose` 
  - Shim uses native Windows API (`LoadLibrary`/`GetProcAddress`/`FreeLibrary`)
  - Automatic fallback when `dlfcn-win32` not found
  - Files added: `include/cross-platfrom/dlfcn_compat.h`, `src/platform/dlfcn_compat_win.cpp`
  - Updated `src/vm.cpp` to use compatibility header on Windows
  
- **Windows Runtime DLL Dependencies**
  - MinGW runtime DLLs now automatically copied to build directory
  - Enables executable to run from PowerShell without MSYS environment
  - DLLs: `libgcc_s_seh-1.dll`, `libwinpthread-1.dll`, `libstdc++-6.dll`

- **Test Runner Cross-Platform Support**
  - Updated `run_tests.ps1` to detect executable in multiple locations
  - Supports both MSYS (`build/`) and MSVC (`build/Release/`) directory structures
  - Simplified script without color formatting to prevent file corruption issues

### Documentation
- Updated `docs/BUILD.md` with clarification that `dlfcn-win32` is optional
- Added notes about automatic MinGW DLL copying for Windows builds

### Build System
- Updated `CMakeLists.txt` to handle optional `dlfcn-win32` dependency
- Added post-build command to copy MinGW DLLs on Windows
- Maintains full Linux and macOS compatibility (no changes to Unix builds)

### Testing
- All 26 tests passing on Windows MSYS/MinGW64

---

## [1.0.2-alpha] - 2025-10-06

### Added
- **Match Statement** - Pattern matching for cleaner conditional logic
  - Support for numbers, strings, booleans, and expressions
  - Arrow operator (`=>`) for single-line cases
  - Block statement cases with curly braces
  - Optional default case
  - Nested match statements
  - Test suite: `tests/test_match.nt`
  
- **Lambda Functions** - Anonymous function expressions
  - Inline function definitions with `fun(params) { body }` syntax
  - Lambdas stored in variables and arrays
  - Lambdas as function arguments
  - Immediately invoked lambda expressions
  - Test suite: `tests/test_lambda_comprehensive.nt`
  
- **Try-Catch Infrastructure** - Foundation for exception handling
  - Tokens: `try`, `catch`, `finally`, `throw`
  - AST structures for exception handling
  - Parser support (VM implementation deferred)

- **New Bytecode Operations**
  - `OP_DUP` - Duplicates value on stack top
  - `OP_CLOSURE` - Creates closure from function

### Fixed
- Match statement stack underflow with multiple cases
  - Fixed duplicate POP after `OP_JUMP_IF_FALSE` which already pops

### Documentation
- Updated `docs/ROADMAP.md` with task completion status
- Updated `docs/language_reference.md` with match and lambda documentation
- Added `docs/IMPLEMENTATION_SUMMARY.md` with technical details
- Added `logs/v1.0.2-alpha.md` with detailed changelog

### Testing
- 10 comprehensive match statement tests
- 8 comprehensive lambda function tests
- All tests passing

---

## [1.0.1-alpha] - Previous Release

_Documentation for previous releases to be added_

---

## [1.0.0-alpha] - Initial Release

_Documentation for initial release to be added_

---

For detailed information about each release, see the `logs/` directory.
