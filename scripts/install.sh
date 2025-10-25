#!/bin/bash
# Installation script for Neutron
# Supports both Linux and macOS

set -e

PREFIX="${PREFIX:-/usr/local}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_msg() {
    echo -e "${2}${1}${NC}"
}

# Detect OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
    LIB_EXT="so"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
    LIB_EXT="dylib"
else
    print_msg "Unsupported OS: $OSTYPE" "$RED"
    exit 1
fi

print_msg "Installing Neutron to ${PREFIX}..." "$GREEN"
print_msg "Detected OS: ${OS}" "$YELLOW"

# Copy binaries
sudo cp neutron "${PREFIX}/bin/"
sudo cp box "${PREFIX}/bin/"

# Copy runtime libraries to both bin and lib (for different rpath configurations)
if [ "$OS" = "linux" ]; then
    sudo cp libneutron_runtime*.so* "${PREFIX}/lib/" 2>/dev/null || true
    sudo cp libneutron_runtime*.so* "${PREFIX}/bin/" 2>/dev/null || true
    # Update library cache on Linux
    sudo ldconfig 2>/dev/null || true
elif [ "$OS" = "macos" ]; then
    sudo cp libneutron_runtime*.dylib "${PREFIX}/lib/" 2>/dev/null || true
    sudo cp libneutron_runtime*.dylib "${PREFIX}/bin/" 2>/dev/null || true
fi

# Copy standard library modules
sudo mkdir -p "${PREFIX}/lib/neutron"
if [ -d "lib" ]; then
    sudo cp lib/*.nt "${PREFIX}/lib/neutron/" 2>/dev/null || true
fi

# Copy headers
sudo mkdir -p "${PREFIX}/bin/include"
sudo cp -r include/* "${PREFIX}/bin/include"

# Set permissions
sudo chmod +x "${PREFIX}/bin/neutron"
sudo chmod +x "${PREFIX}/bin/box"

print_msg "Installation complete!" "$GREEN"
print_msg "" "$NC"
print_msg "Verify installation:" "$YELLOW"
print_msg "  neutron --version" "$NC"
print_msg "  box --help" "$NC"
print_msg "" "$NC"
print_msg "Quick start:" "$YELLOW"
print_msg "  echo 'say(\"Hello, Neutron!\");' > hello.nt" "$NC"
print_msg "  neutron hello.nt" "$NC"
