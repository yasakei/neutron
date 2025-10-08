#!/bin/bash
# Neutron Release Builder for Linux/macOS
# Creates release packages with Neutron + Box binaries

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print colored message
print_msg() {
    echo -e "${2}${1}${NC}"
}

# Detect OS
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    else
        print_msg "Unsupported OS: $OSTYPE" "$RED"
        exit 1
    fi
}

# Get version from CHANGELOG.md
get_version() {
    local version=$(grep -m 1 "^## \[" CHANGELOG.md | sed 's/## \[\([^]]*\)\].*/\1/')
    if [ -z "$version" ]; then
        print_msg "Could not extract version from CHANGELOG.md" "$RED"
        print_msg "Expected format: ## [1.0.4-alpha] - YYYY-MM-DD" "$YELLOW"
        exit 1
    fi
    echo "$version"
}

# Build Neutron
build_neutron() {
    print_msg "Building Neutron..." "$BLUE"
    
    # Clean build directory
    rm -rf build
    
    # Configure and build
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    
    print_msg "Neutron built successfully!" "$GREEN"
}

# Build Box package manager
build_box() {
    print_msg "Building Box..." "$BLUE"
    
    cd nt-box
    
    # Clean build directory
    rm -rf build
    
    # Configure and build
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    
    cd ..
    
    print_msg "Box built successfully!" "$GREEN"
}

# Run tests
run_tests() {
    print_msg "Running tests..." "$BLUE"
    
    # Run test suite
    if [ -f "run_tests.sh" ]; then
        bash run_tests.sh
        print_msg "All tests passed!" "$GREEN"
    else
        print_msg "No test script found, skipping tests" "$YELLOW"
    fi
}

# Create release package
create_package() {
    local os=$1
    local version=$2
    local package_name="neutron-v${version}-${os}-x64"
    local package_dir="releases/${package_name}"
    
    print_msg "Creating release package: ${package_name}" "$BLUE"
    
    # Create directory structure
    mkdir -p "${package_dir}/bin"
    mkdir -p "${package_dir}/lib"
    mkdir -p "${package_dir}/include/neutron"
    mkdir -p "${package_dir}/docs"
    
    # Copy Neutron binary
    if [ "$os" = "linux" ]; then
        cp build/neutron "${package_dir}/bin/"
        strip "${package_dir}/bin/neutron"
    elif [ "$os" = "macos" ]; then
        cp build/neutron "${package_dir}/bin/"
        strip -u -r "${package_dir}/bin/neutron"
    fi
    
    # Copy Box binary
    if [ -f "nt-box/build/box" ]; then
        cp nt-box/build/box "${package_dir}/bin/"
        if [ "$os" = "linux" ]; then
            strip "${package_dir}/bin/box"
        elif [ "$os" = "macos" ]; then
            strip -u -r "${package_dir}/bin/box"
        fi
    fi
    
    # Copy standard library modules (.nt files)
    if [ -d "lib" ]; then
        cp lib/*.nt "${package_dir}/lib/" 2>/dev/null || true
    fi
    
    # Copy headers
    cp -r include/* "${package_dir}/include/neutron/"
    
    # Copy documentation
    cp README.md "${package_dir}/"
    cp LICENSE "${package_dir}/"
    cp CHANGELOG.md "${package_dir}/"
    cp -r docs "${package_dir}/"
    
    # Create installation script
    cat > "${package_dir}/install.sh" << 'INSTALL_EOF'
#!/bin/bash
# Installation script for Neutron

PREFIX="${PREFIX:-/usr/local}"

echo "Installing Neutron to ${PREFIX}..."

# Copy binaries
sudo cp bin/neutron "${PREFIX}/bin/"
sudo cp bin/box "${PREFIX}/bin/"

# Copy libraries
sudo mkdir -p "${PREFIX}/lib/neutron"
sudo cp lib/*.nt "${PREFIX}/lib/neutron/" 2>/dev/null || true

# Copy headers
sudo mkdir -p "${PREFIX}/include/neutron"
sudo cp -r include/neutron/* "${PREFIX}/include/neutron/"

# Set permissions
sudo chmod +x "${PREFIX}/bin/neutron"
sudo chmod +x "${PREFIX}/bin/box"

echo "Installation complete!"
echo "Run 'neutron --version' to verify installation"
INSTALL_EOF
    
    chmod +x "${package_dir}/install.sh"
    
    # Create README for package
    cat > "${package_dir}/README.txt" << README_EOF
Neutron Programming Language v${version}
====================================

This package contains:
- neutron: The Neutron interpreter and compiler
- box: The Box package manager for native modules
- Standard library (.nt modules)
- C API headers for module development
- Documentation

Installation
------------

Run the installation script:
    ./install.sh

Or manually copy:
    sudo cp bin/* /usr/local/bin/
    sudo mkdir -p /usr/local/lib/neutron
    sudo cp lib/*.nt /usr/local/lib/neutron/
    sudo mkdir -p /usr/local/include/neutron
    sudo cp -r include/neutron/* /usr/local/include/neutron/

Verify installation:
    neutron --version
    box --help

Quick Start
-----------

1. Create a file hello.nt:
    say("Hello, Neutron!");

2. Run it:
    neutron hello.nt

3. Install modules:
    box install base64

4. Use modules:
    use base64;
    say(base64.encode("Hello!"));

Documentation
-------------

See the docs/ directory or visit:
https://github.com/yasakei/neutron

License
-------

See LICENSE file for details.
README_EOF
    
    # Create tarball
    print_msg "Creating tarball..." "$BLUE"
    cd releases
    tar -czf "${package_name}.tar.gz" "${package_name}"
    cd ..
    
    # Calculate checksums
    print_msg "Generating checksums..." "$BLUE"
    cd releases
    sha256sum "${package_name}.tar.gz" > "${package_name}.tar.gz.sha256"
    md5sum "${package_name}.tar.gz" > "${package_name}.tar.gz.md5"
    cd ..
    
    print_msg "Package created: releases/${package_name}.tar.gz" "$GREEN"
    print_msg "SHA256: $(cat releases/${package_name}.tar.gz.sha256)" "$YELLOW"
}

# Main function
main() {
    print_msg "=== Neutron Release Builder ===" "$BLUE"
    
    # Check we're in the right directory
    if [ ! -f "CMakeLists.txt" ] || [ ! -d "nt-box" ]; then
        print_msg "Error: Must run from Neutron root directory" "$RED"
        exit 1
    fi
    
    # Detect OS
    OS=$(detect_os)
    print_msg "Detected OS: ${OS}" "$YELLOW"
    
    # Get version
    VERSION=$(get_version)
    print_msg "Version: ${VERSION}" "$YELLOW"
    
    # Clean previous releases
    mkdir -p releases
    
    # Build everything
    build_neutron
    build_box
    
    # Run tests (optional, comment out to skip)
    # run_tests
    
    # Create release package
    create_package "$OS" "$VERSION"
    
    print_msg "=== Release build complete! ===" "$GREEN"
    print_msg "Package: releases/neutron-v${VERSION}-${OS}-x64.tar.gz" "$GREEN"
    print_msg "" "$NC"
    print_msg "To upload to GitHub:" "$BLUE"
    print_msg "  gh release create v${VERSION} releases/neutron-v${VERSION}-${OS}-x64.tar.gz --title \"Neutron v${VERSION}\" --notes-file CHANGELOG.md" "$YELLOW"
}

# Run main
main "$@"
