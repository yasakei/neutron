#!/bin/bash

# Script to create a release version of Neutron with all necessary files
# in a release folder

set -e  # Exit on any error

echo "Creating release version of Neutron..."

# Define directories
RELEASE_DIR="release"
BINARY_NAME="neutron"

# Clean previous release directory if it exists
if [ -d "$RELEASE_DIR" ]; then
    echo "Removing previous release directory..."
    rm -rf "$RELEASE_DIR"
fi

# Create release directory
mkdir -p "$RELEASE_DIR"
echo "Created release directory: $RELEASE_DIR"

# Build the project in release mode
echo "Building project in release mode..."
make release

# Copy the main binary to the release directory
echo "Copying main binary..."
cp "$BINARY_NAME" "$RELEASE_DIR/"

# Copy the runtime library to the release directory
echo "Copying runtime library..."
if [ -f "libneutron_runtime.dylib" ]; then
    # macOS
    cp "libneutron_runtime.dylib" "$RELEASE_DIR/"
elif [ -f "libneutron_runtime.so" ]; then
    # Linux
    cp "libneutron_runtime.so" "$RELEASE_DIR/"
fi

# Check for and copy any box modules that exist
echo "Checking for box modules..."
if [ -d "box" ]; then
    for module_dir in box/*/; do
        if [ -d "$module_dir" ]; then
            module_name=$(basename "$module_dir")
            # Look for shared library files in the module directory
            if ls "$module_dir$module_name.dylib" "$module_dir$module_name.so" 2>/dev/null 1>&2; then
                for lib_file in "$module_dir$module_name".*; do
                    if [[ "$lib_file" == *".dylib" || "$lib_file" == *".so" ]]; then
                        echo "Copying module: $lib_file"
                        mkdir -p "$RELEASE_DIR/modules"
                        cp "$lib_file" "$RELEASE_DIR/modules/"
                    fi
                done
            fi
        fi
    done
fi

# Copy README and other documentation
echo "Copying documentation..."
if [ -f "README.md" ]; then
    cp "README.md" "$RELEASE_DIR/"
fi

if [ -f "LICENSE" ]; then
    cp "LICENSE" "$RELEASE_DIR/"
fi

if [ -f "CONTRIBUTING.md" ]; then
    cp "CONTRIBUTING.md" "$RELEASE_DIR/"
fi

# Create a sample config or startup script if needed
cat > "$RELEASE_DIR/README_RELEASE.md" << EOF
# Neutron Release

This directory contains the Neutron runtime and all necessary files to run applications.

## Contents:
- \$(basename neutron) - Main Neutron executable
- libneutron_runtime.(dylib|so) - Runtime library
- modules/ - Optional modules (if any)

## Usage:
Run the binary directly: ./neutron [options]

## Dependencies:
This release may require system libraries like libcurl and libjsoncpp to be installed.
EOF

echo "Release created in: $RELEASE_DIR/"
echo "Contents:"
ls -la "$RELEASE_DIR/"

echo "Release creation completed successfully!"