#!/bin/bash

# Neutron Installation Script
# Installs Neutron, Box package manager, and all required components to system directories

set -e  # Exit on any error

# Default installation directories
SYSTEM_TYPE=""
if [[ "$OSTYPE" == "darwin"* ]]; then
    SYSTEM_TYPE="macOS"
    DEFAULT_BIN_DIR="/usr/local/bin"
    DEFAULT_INCLUDE_DIR="/usr/local/include"
    DEFAULT_LIB_DIR="/usr/local/lib"
    DEFAULT_SHARE_DIR="/usr/local/share"
elif [[ "$OSTYPE" == "linux-gnu"* ]] || [[ "$OSTYPE" == "linux-musl"* ]]; then
    SYSTEM_TYPE="Linux"
    DEFAULT_BIN_DIR="/usr/local/bin"
    DEFAULT_INCLUDE_DIR="/usr/local/include"
    DEFAULT_LIB_DIR="/usr/local/lib"
    DEFAULT_SHARE_DIR="/usr/local/share"
else
    echo "Unsupported system: $OSTYPE"
    exit 1
fi

# Variables for directories
BIN_DIR="$DEFAULT_BIN_DIR"
INCLUDE_DIR="$DEFAULT_INCLUDE_DIR"
LIB_DIR="$DEFAULT_LIB_DIR"
SHARE_DIR="$DEFAULT_SHARE_DIR"

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
    --share-dir)
      SHARE_DIR="$2"
      shift 2
      ;;
    -h|--help)
      echo "Usage: $0 [OPTIONS]"
      echo "Install Neutron programming language and Box package manager"
      echo ""
      echo "Options:"
      echo "  --bin-dir DIR      Binary installation directory (default: $DEFAULT_BIN_DIR)"
      echo "  --include-dir DIR  Header files installation directory (default: $DEFAULT_INCLUDE_DIR)"
      echo "  --lib-dir DIR      Library files installation directory (default: $DEFAULT_LIB_DIR)"
      echo "  --share-dir DIR    Shared files installation directory (default: $DEFAULT_SHARE_DIR)"
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

echo "Installing Neutron components to system directories:"
echo "  Binaries: $BIN_DIR"
echo "  Headers: $INCLUDE_DIR"
echo "  Libraries: $LIB_DIR"
echo "  Share: $SHARE_DIR"

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

# Check if required binaries exist in current directory
if [ ! -f "./neutron" ]; then
    echo "Neutron binary not found in current directory"
    exit 1
fi

if [ ! -f "./box" ]; then
    echo "Box binary not found in current directory"
    exit 1
fi

if [ ! -d "./include" ]; then
    echo "Include directory not found in current directory"
    exit 1
fi

if [ ! -d "./nt-box" ]; then
    echo "Warning: nt-box directory not found - native module building may not work"
fi

# Create installation directories
echo "Creating installation directories..."
sudo mkdir -p "$BIN_DIR"
sudo mkdir -p "$INCLUDE_DIR"
sudo mkdir -p "$LIB_DIR"
sudo mkdir -p "$SHARE_DIR/neutron"

# Install binaries
echo "Installing binaries..."
sudo cp "./neutron" "$BIN_DIR/"
sudo cp "./box" "$BIN_DIR/"

# Install include files
echo "Installing header files..."
sudo mkdir -p "$INCLUDE_DIR"
sudo cp -r "./include/." "$INCLUDE_DIR/"
# Also ensure core headers are accessible
if [ -d "./include/core" ]; then
    sudo mkdir -p "$INCLUDE_DIR/core"
    sudo cp -r "./include/core/." "$INCLUDE_DIR/core/"
fi

# Install library files (runtime libraries and other library files)
echo "Installing library files..."
# Copy static runtime library
if [ -f "./libneutron_runtime.a" ]; then
    sudo cp "./libneutron_runtime.a" "$LIB_DIR/"
fi

# Copy any shared runtime library files (handling different extensions)
for lib_file in libneutron_runtime.so* libneutron_runtime.dylib* libneutron_runtime.dll*; do
    if [ -f "./$lib_file" ]; then
        sudo cp "./$lib_file" "$LIB_DIR/"
    fi
done

# Install source files for fallback compilation
if [ -d "./src" ]; then
    sudo mkdir -p "$SHARE_DIR/neutron/src"
    sudo cp -r "./src/." "$SHARE_DIR/neutron/src/"
fi

# Install library source files
if [ -d "./libs" ]; then
    sudo mkdir -p "$SHARE_DIR/neutron/libs"
    sudo cp -r "./libs/." "$SHARE_DIR/neutron/libs/"
fi

# Install nt-box (Box package manager source and headers for native modules)
# This should be in the same directory as the box binary
if [ -d "./nt-box" ]; then
    echo "Installing nt-box components for native module building..."
    sudo mkdir -p "$BIN_DIR/nt-box"
    sudo cp -r "./nt-box/." "$BIN_DIR/nt-box/"
    echo "  nt-box installed to: $BIN_DIR/nt-box"
fi

# Set NEUTRON_HOME environment variable hint
echo ""
echo "Note: For global installations, set NEUTRON_HOME:"
echo "  export NEUTRON_HOME=\"$SHARE_DIR/neutron\""
echo "  Add this to your shell profile (~/.bashrc, ~/.zshrc, etc.)"

# Install documentation
if [ -d "./docs" ]; then
    sudo cp -r "./docs/." "$SHARE_DIR/neutron/docs/"
fi

# Install license and README
if [ -f "./LICENSE" ]; then
    sudo cp "./LICENSE" "$SHARE_DIR/neutron/"
fi

if [ -f "./README.md" ]; then
    sudo cp "./README.md" "$SHARE_DIR/neutron/"
fi

echo ""
echo "Installation completed successfully!"
echo ""
echo "Components installed:"
echo "  Executables: $BIN_DIR/neutron, $BIN_DIR/box"
echo "  Headers: $INCLUDE_DIR/"
echo "  Libraries: $LIB_DIR/libneutron_runtime.*"
if [ -d "$BIN_DIR/nt-box" ]; then
    echo "  Box components: $BIN_DIR/nt-box/"
fi
echo "  Documentation: $SHARE_DIR/neutron/docs/"
echo "  License: $SHARE_DIR/neutron/LICENSE"
echo ""
echo "To verify installation:"
echo "  neutron --version"
echo "  box --version"
echo ""