#!/bin/bash

# Neutron Installation Script
# Installs Neutron, Box package manager, and all required components in a single directory structure

set -e  # Exit on any error

# Default installation directory
INSTALL_DIR="$HOME/.neutron"  # Default to user's home directory
NEUTRON_BUILD_DIR="./build"
SYSTEM_TYPE=""

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --prefix)
      INSTALL_DIR="$2"
      shift 2
      ;;
    --build-dir)
      NEUTRON_BUILD_DIR="$2"
      shift 2
      ;;
    -h|--help)
      echo "Usage: $0 [OPTIONS]"
      echo "Install Neutron programming language and Box package manager"
      echo ""
      echo "Options:"
      echo "  --prefix DIR      Installation directory (default: $HOME/.neutron)"
      echo "  --build-dir DIR   Directory containing built binaries (default: ./build)"
      echo "  -h, --help       Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --help for usage information"
      exit 1
      ;;
  esac
done

echo "Installing Neutron to: $INSTALL_DIR"

# Detect system type
if [[ "$OSTYPE" == "darwin"* ]]; then
    SYSTEM_TYPE="macOS"
    echo "Detected macOS"
elif [[ "$OSTYPE" == "linux-gnu"* ]] || [[ "$OSTYPE" == "linux-musl"* ]]; then
    SYSTEM_TYPE="Linux"
    echo "Detected Linux"
else
    echo "Unsupported system: $OSTYPE"
    exit 1
fi

# Check if build directory exists
if [ ! -d "$NEUTRON_BUILD_DIR" ]; then
    echo "Build directory does not exist: $NEUTRON_BUILD_DIR"
    echo "Please build Neutron first using: cmake -B build && cmake --build build"
    exit 1
fi

# Check if required binaries exist
NEUTRON_BIN="$NEUTRON_BUILD_DIR/neutron"
if [ ! -f "$NEUTRON_BIN" ]; then
    echo "Neutron binary not found: $NEUTRON_BIN"
    exit 1
fi

# Create installation directory
mkdir -p "$INSTALL_DIR"

# Copy the main installation files to the target directory
echo "Creating unified installation directory structure..."

# Main installation directory will contain all components
MAIN_INSTALL_DIR="$INSTALL_DIR/neutron"
mkdir -p "$MAIN_INSTALL_DIR"

# Copy neutron executable
cp "$NEUTRON_BUILD_DIR/neutron" "$MAIN_INSTALL_DIR/"

# Copy runtime library (platform-specific)
if [ -f "$NEUTRON_BUILD_DIR/libneutron_runtime.dylib" ]; then
    cp "$NEUTRON_BUILD_DIR/libneutron_runtime.dylib" "$MAIN_INSTALL_DIR/"
elif [ -f "$NEUTRON_BUILD_DIR/libneutron_runtime.so" ]; then
    cp "$NEUTRON_BUILD_DIR/libneutron_runtime.so" "$MAIN_INSTALL_DIR/"
elif [ -f "$NEUTRON_BUILD_DIR/libneutron_runtime.dll" ]; then
    cp "$NEUTRON_BUILD_DIR/libneutron_runtime.dll" "$MAIN_INSTALL_DIR/"
fi

# Copy include directory - CRITICAL for Box to work
if [ -d "include" ]; then
    mkdir -p "$MAIN_INSTALL_DIR/include"
    cp -r include/* "$MAIN_INSTALL_DIR/include/"
    echo "Copied include directory for Box package manager"
else
    echo "WARNING: include directory not found. Box may not work properly."
fi

# Copy lib directory (Neutron standard library files)
if [ -d "lib" ]; then
    mkdir -p "$MAIN_INSTALL_DIR/lib"
    cp -r lib/* "$MAIN_INSTALL_DIR/lib/"
    echo "Copied standard library files"
fi

# Look for and copy Box package manager
BOX_FOUND=false
if [ -f "./box/box" ]; then
    cp "./box/box" "$MAIN_INSTALL_DIR/"
    BOX_FOUND=true
    echo "Copied Box package manager from ./box/"
elif [ -f "./nt-box/build/box" ]; then
    cp "./nt-box/build/box" "$MAIN_INSTALL_DIR/"
    BOX_FOUND=true
    echo "Copied Box package manager from ./nt-box/build/"
elif [ -f "./nt-box/box" ]; then
    cp "./nt-box/box" "$MAIN_INSTALL_DIR/"
    BOX_FOUND=true
    echo "Copied Box package manager from ./nt-box/"
elif [ -f "box" ]; then
    cp "box" "$MAIN_INSTALL_DIR/"
    BOX_FOUND=true
    echo "Copied Box package manager from current directory"
fi

if [ "$BOX_FOUND" = false ]; then
    echo "WARNING: Box package manager not found. It may need to be built separately."
    echo "To build Box, navigate to the nt-box directory and run:"
    echo "  cmake -B build && cmake --build build"
fi

# Create .box directory structure for user modules
mkdir -p "$MAIN_INSTALL_DIR/.box/modules"

# Create neutron command script that sets up environment
cat > "$INSTALL_DIR/neutron" << EOF
#!/bin/bash
# Neutron launcher script

SCRIPT_DIR="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"
NEUTRON_DIR="\$SCRIPT_DIR/neutron"

# Set up environment variables for Neutron
export NEUTRON_HOME="\$NEUTRON_DIR"
export LD_LIBRARY_PATH="\$NEUTRON_DIR:\$LD_LIBRARY_PATH"  # Linux
export DYLD_LIBRARY_PATH="\$NEUTRON_DIR:\$DYLD_LIBRARY_PATH"  # macOS

# Execute neutron with proper library path
exec "\$NEUTRON_DIR/neutron" "\$@"
EOF

chmod +x "$INSTALL_DIR/neutron"

# Create box command script if Box was found
if [ "$BOX_FOUND" = true ]; then
    cat > "$INSTALL_DIR/box" << EOF
#!/bin/bash
# Box package manager launcher script

SCRIPT_DIR="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"
NEUTRON_DIR="\$SCRIPT_DIR/neutron"

# Set up environment variables for Box (which needs access to Neutron's include/)
export NEUTRON_HOME="\$NEUTRON_DIR"

# Execute box with proper environment
exec "\$NEUTRON_DIR/box" "\$@"
EOF

chmod +x "$INSTALL_DIR/box"
fi

# Create setup script for user's shell profile
cat > "$INSTALL_DIR/setup.sh" << EOF
# Neutron Setup Script
# Add this to your shell profile (~/.bashrc, ~/.zshrc, etc.) or source it directly:
#   source $INSTALL_DIR/setup.sh

export PATH="\$PATH:$INSTALL_DIR"
EOF

# Create a sample .box configuration
mkdir -p "$MAIN_INSTALL_DIR/.box"
cat > "$MAIN_INSTALL_DIR/.box/config.json" << EOF
{
  "registry": "https://registry.neutron-lang.org",
  "modules_dir": ".box/modules",
  "include_path": "./include"
}
EOF

# Set up environment for the installed package
echo "Creating installation manifest..."
cat > "$MAIN_INSTALL_DIR/MANIFEST" << EOF
Neutron Installation Manifest
============================

Installed on: $(date)
System: $SYSTEM_TYPE
Installation directory: $INSTALL_DIR
Main binaries directory: $MAIN_INSTALL_DIR
Binaries: neutron$(if [ "$BOX_FOUND" = true ]; then echo ", box"; fi)
Runtime library: $(basename $(find "$MAIN_INSTALL_DIR" -name "libneutron_runtime.*" 2>/dev/null | head -n1) 2>/dev/null || echo "not found")
Include directory: $(if [ -d "$MAIN_INSTALL_DIR/include" ]; then echo "present"; else echo "not present"; fi)
Standard library: $(if [ -d "$MAIN_INSTALL_DIR/lib" ]; then echo "present"; else echo "not present"; fi)

Directory structure:
  $INSTALL_DIR/
  ├── neutron           # Launcher script
  $(if [ "$BOX_FOUND" = true ]; then echo "├── box               # Box launcher script"; fi)
  ├── setup.sh          # Setup script for shell
  └── neutron/          # Main installation directory
      ├── neutron       # Main executable
      ├── libneutron_runtime.*  # Runtime library
      ├── include/      # C++ headers needed by Box
      ├── lib/          # Standard library files (.nt files)
      ├── .box/         # Box configuration and modules directory
      $(if [ "$BOX_FOUND" = true ]; then echo "├── box             # Box package manager"; fi)
      └── MANIFEST      # This manifest

To use Neutron and Box from anywhere, add $INSTALL_DIR to your PATH:
  export PATH="\$PATH:$INSTALL_DIR"
Or run: source $INSTALL_DIR/setup.sh
EOF

echo ""
echo "Installation completed successfully!"
echo ""
echo "Installation location: $INSTALL_DIR"
echo ""
echo "Binaries installed in: $MAIN_INSTALL_DIR"
echo ""
echo "To use Neutron and Box, add to your PATH:"
echo "  export PATH=\"\$PATH:$INSTALL_DIR\""
echo ""
echo "Or source the setup script:"
echo "  source $INSTALL_DIR/setup.sh"
echo ""
echo "To verify installation:"
echo "  neutron --version"
if [ "$BOX_FOUND" = true ]; then
    echo "  box --version"
fi
echo ""
echo "The include/ directory is now available for Box package manager at:"
echo "  $MAIN_INSTALL_DIR/include"