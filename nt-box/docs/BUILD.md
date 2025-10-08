# Box Build Guide

## Quick Start

```bash
cd nt-box
mkdir build && cd build
cmake ..
cmake --build .
```

The `box` binary will be created in `build/` and automatically copied to the project root.

## Platform-Specific Notes

### Linux

**Install dependencies:**
```bash
# Ubuntu/Debian
sudo apt-get install build-essential libcurl4-openssl-dev cmake

# Fedora/RHEL
sudo dnf install gcc-c++ libcurl-devel cmake

# Arch
sudo pacman -S base-devel curl cmake
```

**Build:**
```bash
cd nt-box
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Windows

**Prerequisites:**
- Visual Studio 2019 or later (with C++ Desktop Development workload)
- CMake (install from cmake.org or use Visual Studio's CMake)
- WinINet (included with Windows SDK)

**Build with Visual Studio:**
```powershell
cd nt-box
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

**Or use Visual Studio directly:**
1. Open `nt-box` folder in Visual Studio
2. CMake will auto-configure
3. Build → Build All

### macOS

**Install dependencies:**
```bash
brew install cmake curl
```

**Build:**
```bash
cd nt-box
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

## Testing Box

After building, test Box:

```bash
# Check version
./box version

# Fetch NUR registry
./box search base

# Install a module (will fail if module doesn't exist yet)
./box install base64

# Show help
./box help
```

## Integration with Neutron

Place the `box` binary alongside `neutron`:

```bash
# Linux/macOS
sudo cp box /usr/local/bin/

# Or add to your project
cp box ../neutron/
```

## Development

### Project Structure

```
nt-box/
├── CMakeLists.txt           # Build configuration
├── README.md                # User documentation
├── docs/
│   └── ARCHITECTURE.md      # Technical architecture
├── include/                 # Header files
│   ├── builder.h           # Native module builder
│   ├── installer.h         # Module installer
│   ├── platform.h          # Platform detection
│   └── registry.h          # NUR registry client
└── src/                    # Implementation files
    ├── builder.cpp
    ├── installer.cpp
    ├── main.cpp            # CLI entry point
    ├── platform.cpp
    └── registry.cpp
```

### Adding Features

1. **Add header** to `include/`
2. **Implement** in `src/`
3. **Update CMakeLists.txt** if needed
4. **Rebuild:**
   ```bash
   cd build
   cmake --build .
   ```

### Dependencies

- **Linux/macOS:** libcurl (HTTP requests)
- **Windows:** WinINet (HTTP requests, built-in)
- **All:** C++17 compiler, CMake 3.15+

## Troubleshooting

### "curl not found" on Linux

```bash
sudo apt-get install libcurl4-openssl-dev
```

### CMake version too old

```bash
# Install newer CMake
pip install cmake --upgrade
```

### Build fails on Windows

- Ensure Visual Studio C++ workload is installed
- Run from "Developer Command Prompt for VS"
- Check that CMake can find MSVC

### Module installation fails

- Check internet connection
- Verify NUR registry is accessible:
  ```bash
  curl https://raw.githubusercontent.com/neutron-modules/nur/refs/heads/main/nur.json
  ```
- Ensure `~/.box/modules/` directory is writable

## Cross-Compiling

Not yet supported. Build on target platform for now.

## Next Steps

After building Box:

1. ✅ Test basic commands (`box version`, `box help`)
2. ✅ Set up NUR module repository
3. ✅ Create and publish test modules
4. ✅ Integrate with Neutron runtime
5. ✅ Document module creation process
