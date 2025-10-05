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
cp LICENSE "$RELEASE_NAME/"
cp CONTRIBUTING.md "$RELEASE_NAME/"
mkdir -p "$RELEASE_NAME/docs"
cp -r docs/* "$RELEASE_NAME/docs/" 2>/dev/null || echo "No docs directory found"

# Copy examples if they exist
echo "Checking for examples..."
if [ -d "examples" ]; then
    cp -r examples "$RELEASE_NAME/"
fi

# Copy programs directory
echo "Copying programs..."
if [ -d "programs" ]; then
    cp -r programs "$RELEASE_NAME/"
fi

# Copy tests for verification
echo "Copying tests..."
if [ -d "tests" ]; then
    cp -r tests "$RELEASE_NAME/"
fi

# Copy test runners
echo "Copying test runners..."
if [ -f "run_tests.sh" ]; then
    cp run_tests.sh "$RELEASE_NAME/"
    chmod +x "$RELEASE_NAME/run_tests.sh"
fi
if [ -f "run_tests.ps1" ]; then
    cp run_tests.ps1 "$RELEASE_NAME/"
fi

# Create a simple usage example
cat > "$RELEASE_NAME/USAGE.txt" << EOF
NEUTRON USAGE

To run a Neutron script:
    ./neutron script.nt

To run the test suite:
    ./run_tests.sh          # Linux/macOS
    ./run_tests.ps1         # Windows (PowerShell)

To run individual tests:
    ./neutron tests/test_break_continue.nt
    ./neutron tests/test_modulo.nt
    ./neutron tests/test_string_interpolation.nt
    ./neutron tests/test_array_operations.nt
    ./neutron tests/test_command_line_args.nt arg1 arg2
    ./neutron tests/test_cross_platform.nt

Example programs:
    ./neutron programs/fibonacci.nt
    ./neutron programs/quiz.nt
    ./neutron programs/sys_example.nt

Note: 
- On Linux/macOS, make sure libneutron_runtime.so is in the same directory
- On Windows, make sure neutron_runtime.dll is in the same directory
- Run chmod +x run_tests.sh on Linux/macOS to make test runner executable
EOF

# Create a build info
COMPILER_INFO=$(gcc --version 2>/dev/null | head -n1 || echo "gcc unknown version")
cat > "$RELEASE_NAME/BUILD_INFO.txt" << EOF
Neutron Release Build Information
Built on: $(date)
Compiler: ${COMPILER_INFO}
Platform: $(uname -a)

Features:
- Break/Continue statements
- Command line arguments (sys.args())
- Modulo operator (%)
- String interpolation (\${variable})
- Enhanced array operations (map, filter, find, forEach, etc.)
- Cross-platform support (Windows, macOS, Linux, BSD)
- Platform abstraction layer
- Native system module with file/directory operations
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