#!/bin/bash
# Fix neutron-lsp dependency issues by rebuilding and reinstalling

set -e

echo "Neutron LSP Dependency Fix Script"
echo "================================="

# Check if we're in the neutron source directory
if [ ! -f "CMakeLists.txt" ] || [ ! -d "build" ]; then
    echo "Error: This script must be run from the Neutron source directory"
    echo "Make sure you're in the directory containing CMakeLists.txt"
    exit 1
fi

echo "Rebuilding neutron-lsp with current system libraries..."

# Rebuild neutron-lsp
cd build
make neutron-lsp

# Check if it was built successfully
if [ ! -f "neutron-lsp" ]; then
    echo "Error: neutron-lsp build failed"
    exit 1
fi

echo "Testing neutron-lsp dependencies..."
if ldd neutron-lsp | grep -q "not found"; then
    echo "Warning: neutron-lsp still has missing dependencies:"
    ldd neutron-lsp | grep "not found"
    echo ""
    echo "Installing missing dependencies..."
    
    # Try to install dependencies based on the distribution
    if command -v apt-get >/dev/null 2>&1; then
        sudo apt-get update
        sudo apt-get install -y libjsoncpp-dev libcurl4-openssl-dev
    elif command -v yum >/dev/null 2>&1; then
        sudo yum install -y jsoncpp-devel libcurl-devel
    elif command -v dnf >/dev/null 2>&1; then
        sudo dnf install -y jsoncpp-devel libcurl-devel
    elif command -v pacman >/dev/null 2>&1; then
        sudo pacman -S --noconfirm jsoncpp curl
    else
        echo "Could not detect package manager. Please install manually:"
        echo "  - libjsoncpp (development package)"
        echo "  - libcurl (development package)"
        exit 1
    fi
    
    # Rebuild after installing dependencies
    echo "Rebuilding after dependency installation..."
    make clean
    make neutron-lsp
fi

cd ..

echo "Installing updated neutron-lsp..."
sudo cp build/neutron-lsp /usr/local/bin/

echo "Verifying installation..."
if /usr/local/bin/neutron-lsp --version >/dev/null 2>&1; then
    echo "✓ neutron-lsp installed successfully!"
else
    echo "Testing basic execution..."
    if ldd /usr/local/bin/neutron-lsp | grep -q "not found"; then
        echo "✗ neutron-lsp still has missing dependencies:"
        ldd /usr/local/bin/neutron-lsp | grep "not found"
        exit 1
    else
        echo "✓ neutron-lsp dependencies resolved!"
    fi
fi

echo ""
echo "LSP server should now work with VS Code extension."
echo "Try reloading VS Code or restarting the Neutron extension."