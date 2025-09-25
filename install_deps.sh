#!/bin/bash

# This script checks for the required dependencies to build the project.
# It supports both macOS and Linux.

echo "Checking for required dependencies..."

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Detect OS
OS="$(uname -s)"

# Common dependencies
COMMON_DEPS=("make" "git" "curl")

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
    elif [ "$OS" = "Linux" ]; then
        echo "  $INSTALL_CMD ${MISSING_DEPS[*]}"
    fi
    exit 1
else
    echo "All dependencies are installed."
fi

exit 0
