#!/bin/bash

# This script checks for the required dependencies to build the project.
# It supports macOS, Linux, and Windows (via Git Bash/MSYS2).

echo "Checking for required dependencies..."

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Detect OS
OS="$(uname -s)"

# Common dependencies
COMMON_DEPS=("make" "git" "cmake")

# OS-specific dependencies
if [ "$OS" = "Darwin" ]; then
    # macOS
    echo "Detected macOS."
    DEPS=("${COMMON_DEPS[@]}" "clang++")
    INSTALL_CMD="brew install"
    JSONCPP_INSTALLED=false
    if [ -f "/opt/homebrew/lib/libjsoncpp.dylib" ] || [ -f "/usr/local/lib/libjsoncpp.dylib" ]; then
        JSONCPP_INSTALLED=true
    fi
elif [[ "$OS" == MINGW* ]] || [[ "$OS" == MSYS* ]] || [[ "$OS" == CYGWIN* ]]; then
    # Windows (Git Bash/MSYS2)
    echo "Detected Windows."
    DEPS=("${COMMON_DEPS[@]}" "g++")
    INSTALL_CMD="pacman -S --noconfirm"  # MSYS2 package manager
    JSONCPP_INSTALLED=false
    # On Windows, jsoncpp might be in MSYS2 packages
    if command_exists pkg-config && pkg-config --exists jsoncpp; then
        JSONCPP_INSTALLED=true
    fi
elif [ "$OS" = "Linux" ]; then
    # Linux
    echo "Detected Linux."
    DEPS=("${COMMON_DEPS[@]}" "g++" "pkg-config")
    
    # Detect Linux distribution
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        ID=$ID
    else
        echo "Cannot determine Linux distribution."
        exit 1
    fi

    case $ID in
        ubuntu|debian)
            INSTALL_CMD="sudo apt-get install -y"
            JSONCPP_PKG="libjsoncpp-dev"
            ;;
        arch)
            INSTALL_CMD="sudo pacman -S --noconfirm"
            JSONCPP_PKG="jsoncpp"
            ;;
        fedora)
            INSTALL_CMD="sudo dnf install -y"
            JSONCPP_PKG="jsoncpp-devel"
            ;;
        *)
            echo "Unsupported Linux distribution: $ID"
            exit 1
            ;;
    esac

    JSONCPP_INSTALLED=false
    if command_exists pkg-config && pkg-config --exists jsoncpp; then
        JSONCPP_INSTALLED=true
    fi
else
    echo "Unsupported OS: $OS"
    echo "Supported platforms: macOS, Linux, Windows (Git Bash/MSYS2)"
    exit 1
fi

# Check for dependencies
MISSING_DEPS=()
for dep in "${DEPS[@]}"; do
    if ! command_exists "$dep"; then
        MISSING_DEPS+=("$dep")
    fi
done

# Check for jsoncpp
if ! $JSONCPP_INSTALLED; then
    if [ "$OS" = "Darwin" ]; then
        MISSING_DEPS+=("jsoncpp")
    elif [[ "$OS" == MINGW* ]] || [[ "$OS" == MSYS* ]] || [[ "$OS" == CYGWIN* ]]; then
        MISSING_DEPS+=("mingw-w64-x86_64-jsoncpp")
    elif [ "$OS" = "Linux" ]; then
        MISSING_DEPS+=("$JSONCPP_PKG")
    fi
fi

# Report missing dependencies and suggest installation
if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo "The following dependencies are missing:"
    for dep in "${MISSING_DEPS[@]}"; do
        echo "  - $dep"
    done
    echo ""
    echo "You can try to install them by running the following command:"
    if [ "$OS" = "Darwin" ]; then
        echo "  brew install ${MISSING_DEPS[*]}"
    elif [[ "$OS" == MINGW* ]] || [[ "$OS" == MSYS* ]] || [[ "$OS" == CYGWIN* ]]; then
        echo "  pacman -S --noconfirm ${MISSING_DEPS[*]}"
        echo ""
        echo "Note: On Windows, you may also need Visual Studio Build Tools"
        echo "      or install via MSYS2: https://www.msys2.org/"
    elif [ "$OS" = "Linux" ]; then
        echo "  $INSTALL_CMD ${MISSING_DEPS[*]}"
    fi
    exit 1
else
    echo "All dependencies are installed."
    echo ""
    echo "Build instructions:"
    echo "  1. Build with CMake:"
    echo "     mkdir -p build && cd build"
    echo "     cmake .."
    echo "     make"
    echo ""
    echo "  2. Or use the Makefile:"
    echo "     make"
    echo ""
    echo "  3. Run tests:"
    echo "     ./run_tests.sh          # Linux/macOS"
    echo "     ./run_tests.ps1         # Windows (PowerShell)"
fi

exit 0
