#!/bin/bash

# Neutron Installation Script
# Installs Neutron, Box package manager, and all required components globally from packaged release

set -e  # Exit on any error

# Default installation directories
SYSTEM_TYPE=""
if [[ "$OSTYPE" == "darwin"* ]]; then
    SYSTEM_TYPE="macOS"
    DEFAULT_BIN_DIR="/usr/local/bin"
    DEFAULT_INCLUDE_DIR="/usr/local/include"
    DEFAULT_LIB_DIR="/usr/local/lib"
    USER_DOCUMENTS_DIR="$HOME/Documents"
elif [[ "$OSTYPE" == "linux-gnu"* ]] || [[ "$OSTYPE" == "linux-musl"* ]]; then
    SYSTEM_TYPE="Linux"
    DEFAULT_BIN_DIR="/usr/local/bin"
    DEFAULT_INCLUDE_DIR="/usr/local/include" 
    DEFAULT_LIB_DIR="/usr/local/lib"
    USER_DOCUMENTS_DIR="$HOME/Documents"
else
    echo "Unsupported system: $OSTYPE"
    exit 1
fi

# Variables for directories
BIN_DIR="$DEFAULT_BIN_DIR"
INCLUDE_DIR="$DEFAULT_INCLUDE_DIR"
LIB_DIR="$DEFAULT_LIB_DIR"
DOCS_DIR="$USER_DOCUMENTS_DIR"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --bin-dir)
      BIN_DIR="$2"
      shift 2
      ;;
    --include-dir)
      INCLUDE_DIR="$2"
      shift 2
      ;;
    --lib-dir)
      LIB_DIR="$2"
      shift 2
      ;;
    --docs-dir)
      DOCS_DIR="$2"
      shift 2
      ;;
    -h|--help)
      echo "Usage: $0 [OPTIONS]"
      echo "Install Neutron programming language and Box package manager globally"
      echo ""
      echo "Options:"
      echo "  --bin-dir DIR     Binary installation directory (default: $DEFAULT_BIN_DIR)"
      echo "  --include-dir DIR Include files directory (default: $DEFAULT_INCLUDE_DIR)"
      echo "  --lib-dir DIR     Library files directory (default: $DEFAULT_LIB_DIR)"
      echo "  --docs-dir DIR    Documentation directory (default: $USER_DOCUMENTS_DIR)"
      echo "  -h, --help        Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --help for usage information"
      exit 1
      ;;
  esac
done

# Check if we can write to the installation directories
if [ ! -w "$BIN_DIR" ] && [ "$EUID" -ne 0 ]; then
    echo "Warning: Cannot write to $BIN_DIR. You may need to run this script with sudo."
fi

echo "Installing Neutron system-wide..."
echo "  Binaries: $BIN_DIR"
echo "  Include:  $INCLUDE_DIR" 
echo "  Lib:      $LIB_DIR"
echo "  Docs:     $DOCS_DIR"

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

# Check if required binaries exist in the same directory as the script
NEUTRON_BIN="$SCRIPT_DIR/neutron"
if [ ! -f "$NEUTRON_BIN" ]; then
    echo "Neutron binary not found: $NEUTRON_BIN"
    exit 1
fi

# Create installation directories
sudo mkdir -p "$BIN_DIR"
sudo mkdir -p "$INCLUDE_DIR"
sudo mkdir -p "$LIB_DIR"

# Copy neutron binary to system binary directory
sudo cp "$NEUTRON_BIN" "$BIN_DIR/"
echo "Copied neutron binary to $BIN_DIR"

# Copy runtime library (platform-specific) to lib directory
if [ -f "$SCRIPT_DIR/libneutron_runtime.dylib" ]; then
    sudo cp "$SCRIPT_DIR/libneutron_runtime.dylib" "$LIB_DIR/"
    echo "Copied libneutron_runtime.dylib to $LIB_DIR"
elif [ -f "$SCRIPT_DIR/libneutron_runtime.so" ]; then
    sudo cp "$SCRIPT_DIR/libneutron_runtime.so" "$LIB_DIR/"
    echo "Copied libneutron_runtime.so to $LIB_DIR"
fi

# Copy include directory to system include directory
if [ -d "$SCRIPT_DIR/include" ]; then
    sudo mkdir -p "$INCLUDE_DIR/neutron"
    sudo cp -r "$SCRIPT_DIR/include/"* "$INCLUDE_DIR/neutron/"
    echo "Copied include/ files to $INCLUDE_DIR/neutron"
else
    echo "WARNING: include directory not found. Box may not work properly."
fi

# Copy lib directory (Neutron standard library files) to system lib directory
if [ -d "$SCRIPT_DIR/lib" ]; then
    sudo mkdir -p "$LIB_DIR/neutron"
    sudo cp -r "$SCRIPT_DIR/lib/"* "$LIB_DIR/neutron/"
    echo "Copied standard library files to $LIB_DIR/neutron"
fi

# Look for and copy Box package manager
BOX_FOUND=false
if [ -f "$SCRIPT_DIR/box" ]; then
    sudo cp "$SCRIPT_DIR/box" "$BIN_DIR/"
    BOX_FOUND=true
    echo "Copied Box package manager from $SCRIPT_DIR/ to $BIN_DIR"
fi

if [ "$BOX_FOUND" = false ]; then
    echo "WARNING: Box package manager not found in the package. It may need to be built separately."
fi

# Handle docs folder - create "Neutron Docs" and move to Documents/ folder
if [ -d "$SCRIPT_DIR/docs" ]; then
    # Create Neutron Docs directory in Documents folder
    mkdir -p "$DOCS_DIR/Neutron Docs"
    cp -r "$SCRIPT_DIR/docs/"* "$DOCS_DIR/Neutron Docs/"
    echo "Copied documentation to $DOCS_DIR/Neutron Docs"
else
    echo "Documentation directory not found in the package"
fi

# Create .box directory structure for user modules
mkdir -p "$HOME/.box/modules"

echo ""
echo "Installation completed successfully!"
echo ""
echo "Installed components:"
echo "  - neutron executable: $BIN_DIR/neutron"
if [ "$BOX_FOUND" = true ]; then
    echo "  - box executable: $BIN_DIR/box"
fi
echo "  - include files: $INCLUDE_DIR/neutron"
echo "  - library files: $LIB_DIR/neutron"
echo "  - documentation: $DOCS_DIR/Neutron Docs"
echo ""
echo "To verify installation:"
echo "  neutron --version"
if [ "$BOX_FOUND" = true ]; then
    echo "  box --version"
fi
echo ""
echo "Binaries are now available globally in your PATH."