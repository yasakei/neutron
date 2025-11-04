#!/bin/bash

# Neutron Packaging Script
# Creates release packages locally in the same way as the GitHub release workflow
# This script automatically detects the operating system and creates the appropriate package

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Neutron Packaging Script${NC}"
echo "========================"

# Detect the operating system
OS_TYPE=""
ARCH_TYPE=""
PACKAGE_NAME=""

if [[ "$OSTYPE" == "darwin"* ]]; then
    OS_TYPE="macos"
    # Detect architecture
    if [[ $(uname -m) == "arm64" ]]; then
        ARCH_TYPE="arm64"
        PACKAGE_NAME="neutron-macos-arm64"
    elif [[ $(uname -m) == "x86_64" ]]; then
        ARCH_TYPE="intel"
        PACKAGE_NAME="neutron-macos-intel"
    else
        echo -e "${RED}Unsupported macOS architecture: $(uname -m)${NC}"
        exit 1
    fi
    
    # For macOS, we can also create a universal package if both architectures are available
    echo -e "${YELLOW}Detected macOS ${ARCH_TYPE}${NC}"
    
elif [[ "$OSTYPE" == "linux-gnu"* ]] || [[ "$OSTYPE" == "linux-musl"* ]]; then
    OS_TYPE="linux"
    # Detect architecture
    if [[ $(uname -m) == "x86_64" ]]; then
        ARCH_TYPE="x64"
        PACKAGE_NAME="neutron-linux-x64"
    elif [[ $(uname -m) == "aarch64" ]] || [[ $(uname -m) == "arm64" ]]; then
        ARCH_TYPE="arm64"
        PACKAGE_NAME="neutron-linux-arm64"
    else
        echo -e "${RED}Unsupported Linux architecture: $(uname -m)${NC}"
        exit 1
    fi
    
    echo -e "${YELLOW}Detected Linux ${ARCH_TYPE}${NC}"
    
elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
    OS_TYPE="windows"
    ARCH_TYPE="x64"
    PACKAGE_NAME="neutron-windows-x64"
    echo -e "${YELLOW}Detected Windows${NC}"
else
    echo -e "${RED}Unsupported operating system: $OSTYPE${NC}"
    exit 1
fi

echo -e "${BLUE}Building package for $OS_TYPE $ARCH_TYPE${NC}"

# Build the binaries if they don't exist
if [[ "$OS_TYPE" == "windows" ]]; then
    if [ ! -f "build/neutron.exe" ] || [ ! -f "nt-box/build/box.exe" ]; then
        echo -e "${YELLOW}Building Neutron and Box package manager...${NC}"
        
        # Configure and build Neutron
        echo -e "${BLUE}Configuring and building Neutron...${NC}"
        if [ ! -d "build" ]; then
            mkdir -p build
        fi
        cd build
        cmake -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release .. || { echo -e "${RED}CMake configuration failed${NC}"; exit 1; }
        cmake --build . -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4) || { echo -e "${RED}Neutron build failed${NC}"; exit 1; }
        cd ..
        
        # Build Box package manager
        echo -e "${BLUE}Configuring and building Box package manager...${NC}"
        if [ ! -d "nt-box/build" ]; then
            mkdir -p nt-box/build
        fi
        cd nt-box/build
        cmake -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release .. || { echo -e "${RED}Box CMake configuration failed${NC}"; exit 1; }
        cmake --build . || { echo -e "${RED}Box build failed${NC}"; exit 1; }
        cd ../..
    fi
else
    if [ ! -f "./neutron" ] || [ ! -f "nt-box/build/box" ]; then
        echo -e "${YELLOW}Building Neutron and Box package manager...${NC}"
        
        # Configure and build Neutron
        echo -e "${BLUE}Configuring and building Neutron...${NC}"
        if [ ! -d "build" ]; then
            mkdir -p build
        fi
        if [[ "$OS_TYPE" == "macos" ]]; then
            # For macOS, detect and set the architecture appropriately
            # Also detect various installation locations for jsoncpp (Homebrew, conda, etc.)
            ORIGINAL_PKG_CONFIG_PATH="$PKG_CONFIG_PATH"
            
            # List of potential locations for jsoncpp
            JSONCPP_LOCATIONS=(
                "/opt/homebrew"           # Homebrew on Apple Silicon
                "/usr/local"              # Homebrew on Intel, and some other installations
                "/usr/local/Caskroom/miniconda/base"  # Conda in miniconda
                "$CONDA_PREFIX"           # Current conda environment
                "$CONDA_PREFIX_1"         # Alternative conda environment variable
            )
            
            # Try to find a valid jsoncpp installation location
            VALID_JSONCPP_PATH=""
            for path in "${JSONCPP_LOCATIONS[@]}"; do
                if [ -n "$path" ] && [ -d "$path/lib/pkgconfig" ] && [ -f "$path/lib/pkgconfig/jsoncpp.pc" ]; then
                    VALID_JSONCPP_PATH="$path"
                    break
                fi
            done
            
            # Set the PKG_CONFIG_PATH and explicit variables to help CMake find jsoncpp if found
            if [ -n "$VALID_JSONCPP_PATH" ]; then
                export PKG_CONFIG_PATH="$VALID_JSONCPP_PATH/lib/pkgconfig:$PKG_CONFIG_PATH"
                # For macOS, the CMakeLists.txt uses find_library and find_path instead of pkg-config
                # So we need to set the variables directly
                # Check if the versioned library exists first
                if [ -f "$VALID_JSONCPP_PATH/lib/libjsoncpp.1.9.6.dylib" ]; then
                    JSONCPP_LIB_PATH="$VALID_JSONCPP_PATH/lib/libjsoncpp.1.9.6.dylib"
                elif [ -f "$VALID_JSONCPP_PATH/lib/libjsoncpp.dylib" ]; then
                    JSONCPP_LIB_PATH="$VALID_JSONCPP_PATH/lib/libjsoncpp.dylib"
                else
                    # Fallback - might not work if file doesn't exist
                    JSONCPP_LIB_PATH="$VALID_JSONCPP_PATH/lib/libjsoncpp.dylib"
                fi
                CMAKE_CMD="cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DJSONCPP_INCLUDE_DIR=$VALID_JSONCPP_PATH/include -DJSONCPP_LIBRARY=$JSONCPP_LIB_PATH"
                echo -e "${YELLOW}Using explicit paths to help find jsoncpp: $VALID_JSONCPP_PATH${NC}"
            else
                echo -e "${YELLOW}Warning: jsoncpp.pc not found in common locations. CMake may fail to find jsoncpp.${NC}"
                CMAKE_CMD="cmake -B build -S . -DCMAKE_BUILD_TYPE=Release"
            fi
            
            if [[ "$ARCH_TYPE" == "arm64" ]]; then
                CMAKE_CMD="$CMAKE_CMD -DCMAKE_OSX_ARCHITECTURES=arm64"
            elif [[ "$ARCH_TYPE" == "intel" ]]; then
                CMAKE_CMD="$CMAKE_CMD -DCMAKE_OSX_ARCHITECTURES=x86_64"
            fi
            
            eval $CMAKE_CMD || { 
                # Restore original PKG_CONFIG_PATH before exiting
                export PKG_CONFIG_PATH="$ORIGINAL_PKG_CONFIG_PATH"
                { echo -e "${RED}CMake configuration failed${NC}"; exit 1; }
            }
            
            # Restore original PKG_CONFIG_PATH after successful configuration
            export PKG_CONFIG_PATH="$ORIGINAL_PKG_CONFIG_PATH"
        else
            # For Linux and other Unix-like systems
            cmake -B build -S . -DCMAKE_BUILD_TYPE=Release || { echo -e "${RED}CMake configuration failed${NC}"; exit 1; }
        fi
        
        cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4) || { echo -e "${RED}Neutron build failed${NC}"; exit 1; }
        
        # Build Box package manager
        echo -e "${BLUE}Configuring and building Box package manager...${NC}"
        if [ ! -d "nt-box/build" ]; then
            mkdir -p nt-box/build
        fi
        cd nt-box
        
        if [[ "$OS_TYPE" == "macos" ]]; then
            # For macOS, also detect various installation locations for jsoncpp for Box build
            # List of potential locations for jsoncpp
            JSONCPP_LOCATIONS=(
                "/opt/homebrew"           # Homebrew on Apple Silicon
                "/usr/local"              # Homebrew on Intel, and some other installations
                "/usr/local/Caskroom/miniconda/base"  # Conda in miniconda
                "$CONDA_PREFIX"           # Current conda environment
                "$CONDA_PREFIX_1"         # Alternative conda environment variable
            )
            
            # Try to find a valid jsoncpp installation location
            VALID_JSONCPP_PATH=""
            for path in "${JSONCPP_LOCATIONS[@]}"; do
                if [ -n "$path" ] && [ -d "$path/lib/pkgconfig" ] && [ -f "$path/lib/pkgconfig/jsoncpp.pc" ]; then
                    VALID_JSONCPP_PATH="$path"
                    break
                fi
            done
            
            # Set the CMAKE_PREFIX_PATH and explicit variables to help find jsoncpp if found
            if [ -n "$VALID_JSONCPP_PATH" ]; then
                # Check if the versioned library exists first
                if [ -f "$VALID_JSONCPP_PATH/lib/libjsoncpp.1.9.6.dylib" ]; then
                    JSONCPP_LIB_PATH="$VALID_JSONCPP_PATH/lib/libjsoncpp.1.9.6.dylib"
                elif [ -f "$VALID_JSONCPP_PATH/lib/libjsoncpp.dylib" ]; then
                    JSONCPP_LIB_PATH="$VALID_JSONCPP_PATH/lib/libjsoncpp.dylib"
                else
                    # Fallback - might not work if file doesn't exist
                    JSONCPP_LIB_PATH="$VALID_JSONCPP_PATH/lib/libjsoncpp.dylib"
                fi
                cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$VALID_JSONCPP_PATH -DJSONCPP_INCLUDE_DIR=$VALID_JSONCPP_PATH/include -DJSONCPP_LIBRARY=$JSONCPP_LIB_PATH || { echo -e "${RED}Box CMake configuration failed${NC}"; exit 1; }
            else
                cmake -B build -S . -DCMAKE_BUILD_TYPE=Release || { echo -e "${RED}Box CMake configuration failed${NC}"; exit 1; }
            fi
        else
            cmake -B build -S . -DCMAKE_BUILD_TYPE=Release || { echo -e "${RED}Box CMake configuration failed${NC}"; exit 1; }
        fi
        
        cmake --build build || { echo -e "${RED}Box build failed${NC}"; exit 1; }
        cd ..
    fi
fi

echo -e "${GREEN}Packaging Neutron...${NC}"

# Determine target name for package
if [[ "$OS_TYPE" == "macos" ]]; then
    if [[ "$ARCH_TYPE" == "arm64" ]]; then
        TARGET_NAME="neutron-macos-arm64"
    elif [[ "$ARCH_TYPE" == "intel" ]]; then
        TARGET_NAME="neutron-macos-intel"
    fi
elif [[ "$OS_TYPE" == "linux" ]]; then
    TARGET_NAME="neutron-linux-$ARCH_TYPE"
elif [[ "$OS_TYPE" == "windows" ]]; then
    TARGET_NAME="neutron-windows-$ARCH_TYPE"
fi

# Create temporary packaging directory
echo -e "${BLUE}Creating package directory: $TARGET_NAME${NC}"
rm -rf "$TARGET_NAME"
mkdir -p "$TARGET_NAME"

# Copy binaries to package directory based on OS
if [[ "$OS_TYPE" == "windows" ]]; then
    # Windows specific: copy binaries from build directory
    cp build/neutron.exe "$TARGET_NAME/"
    cp build/*.dll "$TARGET_NAME/" 2>/dev/null || true  # Copy any DLL files, ignore if none exist
    cp build/*.lib "$TARGET_NAME/" 2>/dev/null || true  # Copy static libraries for -b flag functionality
    cp build/*.a "$TARGET_NAME/" 2>/dev/null || true  # Copy static libraries for MinGW builds
    cp nt-box/build/box.exe "$TARGET_NAME/"
else
    # Unix-like systems: copy from build directory
    cp build/neutron "$TARGET_NAME/"
    # Copy library files with different extensions for different systems
    cp build/libneutron_runtime.so* "$TARGET_NAME/" 2>/dev/null || true
    cp build/libneutron_runtime.dylib* "$TARGET_NAME/" 2>/dev/null || true
    cp build/libneutron_runtime.dll* "$TARGET_NAME/" 2>/dev/null || true
    # Copy build directory for -b flag functionality
    cp -r build "$TARGET_NAME/" 2>/dev/null || true
    cp nt-box/build/box "$TARGET_NAME/"
fi

# Copy other files to package directory
cp README.md LICENSE "$TARGET_NAME/" 2>/dev/null || true
cp -r docs "$TARGET_NAME/" 2>/dev/null || true
cp -r include "$TARGET_NAME/" 2>/dev/null || true
cp scripts/install.sh "$TARGET_NAME/" 2>/dev/null || true

# Create the compressed archive
VERSION="unknown"
if [[ "$OS_TYPE" == "windows" ]]; then
    if [ -f "build/neutron.exe" ]; then
        # Try to get version from neutron if it supports --version
        VERSION=$(./build/neutron.exe --version 2>/dev/null | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+*' || echo "unknown")
        if [[ "$VERSION" == "unknown" ]]; then
            VERSION="latest"
        fi
    fi
else
    if [ -f "build/neutron" ]; then
        # Try to get version from neutron if it supports --version
        VERSION=$(./build/neutron --version 2>/dev/null | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+*' || echo "unknown")
        if [[ "$VERSION" == "unknown" ]]; then
            VERSION="latest"
        fi
    fi
fi

if [[ "$OS_TYPE" == "windows" ]]; then
    PACKAGE_FILE="neutron-${VERSION}-${OS_TYPE}-${ARCH_TYPE}.zip"
    echo -e "${BLUE}Creating zip archive: $PACKAGE_FILE${NC}"
    # Use zip on Windows/MSYS2, tar on Unix systems
    if command -v zip &> /dev/null; then
        cd "$TARGET_NAME" && zip -r "../$PACKAGE_FILE" . && cd ..
    else
        echo -e "${RED}zip command not found. Please install zip to package for Windows${NC}"
        exit 1
    fi
else
    # Use tar for Unix-like systems
    PACKAGE_FILE="neutron-${VERSION}-${OS_TYPE}-${ARCH_TYPE}.tar.gz"
    echo -e "${BLUE}Creating tar.gz archive: $PACKAGE_FILE${NC}"
    tar -czf "$PACKAGE_FILE" "$TARGET_NAME"
fi

# Clean up the temporary directory if requested
echo -e "${GREEN}Package created: $PACKAGE_FILE${NC}"

echo -e "${GREEN}Packaging completed successfully!${NC}"
echo ""
echo -e "${BLUE}Package information:${NC}"
echo "  Created file: $PACKAGE_FILE"
echo "  Size: $(ls -lah "$PACKAGE_FILE" | awk '{print $5}')"
echo "  Target system: $TARGET_NAME"
echo ""
echo -e "${YELLOW}To extract and use:${NC}"
if [[ "$OS_TYPE" == "windows" ]]; then
    echo "  unzip $PACKAGE_FILE"
else
    echo "  tar -xzf $PACKAGE_FILE"
fi
echo "  cd $TARGET_NAME"
echo "  # Binaries are now available: neutron, box"
echo ""