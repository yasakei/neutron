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

# Function to check and install dependencies
check_dependencies() {
    echo "Checking system dependencies..."
    
    if [[ "$SYSTEM_TYPE" == "Linux" ]]; then
        # Check for package manager and install dependencies
        if command -v apt-get >/dev/null 2>&1; then
            echo "Detected apt package manager"
            echo "Installing dependencies with apt..."
            sudo apt-get update
            sudo apt-get install -y libjsoncpp-dev libcurl4-openssl-dev
        elif command -v yum >/dev/null 2>&1; then
            echo "Detected yum package manager"
            echo "Installing dependencies with yum..."
            sudo yum install -y jsoncpp-devel libcurl-devel
        elif command -v dnf >/dev/null 2>&1; then
            echo "Detected dnf package manager"
            echo "Installing dependencies with dnf..."
            sudo dnf install -y jsoncpp-devel libcurl-devel
        elif command -v pacman >/dev/null 2>&1; then
            echo "Detected pacman package manager"
            echo "Installing dependencies with pacman..."
            sudo pacman -S --noconfirm jsoncpp curl
        elif command -v zypper >/dev/null 2>&1; then
            echo "Detected zypper package manager"
            echo "Installing dependencies with zypper..."
            sudo zypper install -y libjsoncpp-devel libcurl-devel
        else
            echo "Warning: Could not detect package manager. Please install manually:"
            echo "  - libjsoncpp (development package)"
            echo "  - libcurl (development package)"
        fi
    elif [[ "$SYSTEM_TYPE" == "macOS" ]]; then
        if command -v brew >/dev/null 2>&1; then
            echo "Installing dependencies with Homebrew..."
            brew install jsoncpp curl
        else
            echo "Warning: Homebrew not found. Please install manually:"
            echo "  - jsoncpp"
            echo "  - curl"
        fi
    fi
}

# Function to check if a binary works
check_binary() {
    local binary_path="$1"
    local binary_name="$2"
    
    if [ -f "$binary_path" ]; then
        echo "Checking $binary_name dependencies..."
        if [[ "$SYSTEM_TYPE" == "Linux" ]]; then
            if ldd "$binary_path" | grep -q "not found"; then
                echo "Warning: $binary_name has missing dependencies:"
                ldd "$binary_path" | grep "not found"
                return 1
            fi
        elif [[ "$SYSTEM_TYPE" == "macOS" ]]; then
            if ! otool -L "$binary_path" >/dev/null 2>&1; then
                echo "Warning: $binary_name has dependency issues"
                return 1
            fi
        fi
        echo "$binary_name dependencies OK"
        return 0
    fi
    return 1
}

# Check dependencies first
check_dependencies

# Check if required binaries exist in current directory
if [ ! -f "./neutron" ]; then
    echo "Neutron binary not found in current directory"
    exit 1
fi

if [ ! -f "./box" ]; then
    echo "Box binary not found in current directory"
    exit 1
fi

# Check for neutron-lsp in build directory first, then current directory
NEUTRON_LSP_PATH=""
if [ -f "./build/neutron-lsp" ]; then
    NEUTRON_LSP_PATH="./build/neutron-lsp"
elif [ -f "./neutron-lsp" ]; then
    NEUTRON_LSP_PATH="./neutron-lsp"
fi

if [ -z "$NEUTRON_LSP_PATH" ]; then
    echo "Warning: neutron-lsp binary not found in current directory or build/"
else
    echo "Found neutron-lsp at: $NEUTRON_LSP_PATH"
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

# Install neutron-lsp if found and working
if [ -n "$NEUTRON_LSP_PATH" ]; then
    echo "Installing neutron-lsp..."
    if check_binary "$NEUTRON_LSP_PATH" "neutron-lsp"; then
        sudo cp "$NEUTRON_LSP_PATH" "$BIN_DIR/"
        echo "neutron-lsp installed successfully"
    else
        echo "Warning: neutron-lsp has dependency issues. Installing anyway..."
        sudo cp "$NEUTRON_LSP_PATH" "$BIN_DIR/"
        echo "You may need to install missing dependencies manually"
    fi
fi

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

# Copy shared library from build directory if it exists
if [ -f "./build/libneutron_shared.so" ]; then
    sudo cp "./build/libneutron_shared.so" "$LIB_DIR/"
fi

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

# Update library cache on Linux
if [[ "$SYSTEM_TYPE" == "Linux" ]]; then
    echo "Updating library cache..."
    sudo ldconfig
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
if [ -f "$BIN_DIR/neutron-lsp" ]; then
    echo "  LSP Server: $BIN_DIR/neutron-lsp"
fi
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
if [ -f "$BIN_DIR/neutron-lsp" ]; then
    echo "  neutron-lsp --version"
fi
echo ""

# Final dependency check
echo "Final dependency check:"
check_binary "$BIN_DIR/neutron" "neutron"
check_binary "$BIN_DIR/box" "box"
if [ -f "$BIN_DIR/neutron-lsp" ]; then
    if check_binary "$BIN_DIR/neutron-lsp" "neutron-lsp"; then
        echo "All binaries are working correctly!"
    else
        echo ""
        echo "WARNING: neutron-lsp may not work due to missing dependencies."
        echo "Try running: sudo apt-get install libjsoncpp-dev libcurl4-openssl-dev"
        echo "Or install the equivalent packages for your distribution."
    fi
fi