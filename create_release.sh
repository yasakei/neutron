#!/bin/bash

# Neutron Release Script
# Creates a release package with binary and required files

set -e

echo "Building Neutron release..."

# Build the project
make clean 2>/dev/null || echo "Clean not needed"
make all

# Create release directory
RELEASE_DATE=$(date +%Y%m%d)
RELEASE_NAME="neutron-release-${RELEASE_DATE}"
echo "Creating release directory: $RELEASE_NAME"
mkdir -p "$RELEASE_NAME"

# Copy the main binary
echo "Copying neutron binary..."
cp neutron "$RELEASE_NAME/"

# Copy required runtime library
echo "Copying runtime library..."
cp libneutron_runtime.so "$RELEASE_NAME/"

# Copy essential directories
echo "Copying lib directory..."
cp -r lib "$RELEASE_NAME/"  # Built-in modules

echo "Copying box directory (if exists)..."
if [ -d "box" ]; then
    cp -r box "$RELEASE_NAME/"  # External modules
fi

# Copy documentation
echo "Copying documentation..."
cp README.md "$RELEASE_NAME/"
cp DOCS.md "$RELEASE_NAME/"
cp CHANGELOG.md "$RELEASE_NAME/"
cp TECHNICAL_DOCS.md "$RELEASE_NAME/"
cp CONTRIBUTING.md "$RELEASE_NAME/"

# Copy examples if they exist
echo "Checking for examples..."
if [ -d "examples" ]; then
    cp -r examples "$RELEASE_NAME/"
fi

# Copy dev_tests for verification
echo "Copying dev_tests..."
if [ -d "dev_tests" ]; then
    cp -r dev_tests "$RELEASE_NAME/"
fi

# Create a simple usage example
cat > "$RELEASE_NAME/USAGE.txt" << EOF
NEUTRON USAGE

To run a Neutron script:
    ./neutron script.nt

To run development tests:
    ./neutron dev_tests/feature_test.nt
    ./neutron dev_tests/module_test.nt
    ./neutron dev_tests/class_test.nt
    ./neutron dev_tests/binary_conversion_feature_test.nt

Examples:
    ./neutron examples/hello.nt

Note: Make sure libneutron_runtime.so is in the same directory as the neutron binary.
EOF

# Create a build info
COMPILER_INFO=$(gcc --version 2>/dev/null | head -n1 || echo "gcc unknown version")
cat > "$RELEASE_NAME/BUILD_INFO.txt" << EOF
Neutron Release Build Information
Built on: $(date)
Compiler: ${COMPILER_INFO}
Platform: $(uname -a)
Features: Enhanced stack management, improved module system, dual access modules
EOF

echo "Release package created: $RELEASE_NAME/"
echo "Contents:"
ls -la "$RELEASE_NAME/"

# Create a zip for distribution
ZIP_NAME="neutron-${RELEASE_DATE}.zip"
echo "Creating ZIP archive: $ZIP_NAME"
zip -r "$ZIP_NAME" "$RELEASE_NAME"

echo "Release ZIP created: $ZIP_NAME"
echo ""
echo "To upload to GitHub:"
echo "1. Go to https://github.com/yasakei/neutron/releases"
echo "2. Click 'Draft a new release'"
echo "3. Upload $ZIP_NAME as a binary asset"
echo ""
echo "GitHub CLI command (if installed):"
echo "gh release create v1.0.0 $ZIP_NAME --title \"Neutron v1.0.0\" --notes \"Enhanced build with stack management fixes and improved module system\""