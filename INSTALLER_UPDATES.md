ative module development):**
- `include/core/neutron.h` - Main C API header
- `include/core/capi.h` - C API definitions
- `include/core/vm.h` - VM interface
- `include/core/checkpoint.h` - Checkpoint support

**Import Libraries:**
- `lib/neutron_runtime.lib` - Runtime import library
- `lib/neutron_shared.lib` - Shared library import library (from build/Release)

**Directory Structure:**
- Creates `.box/modules/` directory for user-installed modules

**Documentation:**
- `LICENSE` - License file
- `README.md` - Main readme
- `BOX_README.md` - Box package manager readme
- `docs/*` - Documentation files

### Developer Files Section
Updated to avoid duplicating headers that are already installed in the Core section.

### Uninstaller Section
- Added cleanup for new directories (`.box`, `include`, `lib`)
- Added user prompt before removing `.box/modules` to prevent accidental deletion of installed packages
- Properly removes all installed files

### Component Descriptions
Updated descriptions to reflect that headers and libraries are now included in the Core installation.

## How the Installer Works

1. **Core Installation** (Required):
   - Installs runtime executables and DLLs
   - Installs headers and import libraries needed for native module development
   - Creates `.box/modules/` directory structure
   - Installs documentation

2. **Bundled Runtimes** (Recommended):
   - Installs PowerShell and 7-Zip if bundled
   - Downloads and installs Visual C++ Redistributable

3. **Add to PATH** (Optional):
   - Adds installation directory to system PATH

4. **Build Tools** (Optional):
   - Downloads and installs Microsoft C++ Build Tools (~1.2GB)
   - Creates helper script for setting up MSVC environment

## Building the Installer

Use the `package.py` script to build the installer:

```bash
python package.py --installer --version 1.0.0
```

The script will:
1. Copy binaries from build directory to root
2. Copy DLLs and import libraries
3. Bundle vcpkg tools (PowerShell, 7-Zip)
4. Run NSIS to create the installer

## Testing the Installation

After installing Neutron:

1. Open a new command prompt
2. Navigate to the installation directory (default: `C:\Program Files\Neutron`)
3. Test the runtime: `neutron.exe --version`
4. Test the package manager: `box.exe --version`
5. Test building a native module:
   ```bash
   cd path\to\your\project
   box.exe build native base64
   ```

The native module should build successfully and be installed to `.box/modules/base64/`.
