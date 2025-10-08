#!/bin/bash
# build_all.sh - Build Neutron and Box Package Manager
# Supports Linux and macOS with automatic platform detection

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
BUILD_TYPE="Release"
CLEAN_BUILD=false
RUN_TESTS=false
VERBOSE=false
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Print colored message
print_msg() {
    local color=$1
    shift
    echo -e "${color}$@${NC}"
}

print_header() {
    echo ""
    print_msg "$BLUE" "═══════════════════════════════════════════════════════════════"
    print_msg "$BLUE" "  $1"
    print_msg "$BLUE" "═══════════════════════════════════════════════════════════════"
    echo ""
}

print_success() {
    print_msg "$GREEN" "✓ $1"
}

print_error() {
    print_msg "$RED" "✗ $1"
}

print_warning() {
    print_msg "$YELLOW" "⚠ $1"
}

print_info() {
    print_msg "$BLUE" "ℹ $1"
}

# Show usage
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Build Neutron interpreter and Box package manager.

OPTIONS:
    -h, --help          Show this help message
    -d, --debug         Build in Debug mode (default: Release)
    -c, --clean         Clean build directories before building
    -t, --test          Run test suite after building
    -v, --verbose       Verbose build output
    -j, --jobs N        Number of parallel jobs (default: $JOBS)

EXAMPLES:
    $0                  # Build everything in Release mode
    $0 -d               # Build in Debug mode
    $0 -c -t            # Clean build and run tests
    $0 -d -v -j 8       # Debug build, verbose, 8 parallel jobs

EOF
    exit 0
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -t|--test)
            RUN_TESTS=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        *)
            print_error "Unknown option: $1"
            usage
            ;;
    esac
done

# Detect OS
OS="unknown"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="Linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macOS"
else
    print_error "Unsupported OS: $OSTYPE"
    exit 1
fi

# Get project root (script is in scripts/)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

print_header "Build Configuration"
print_info "OS: $OS"
print_info "Build Type: $BUILD_TYPE"
print_info "Jobs: $JOBS"
print_info "Clean Build: $CLEAN_BUILD"
print_info "Run Tests: $RUN_TESTS"
print_info "Verbose: $VERBOSE"
print_info "Project Root: $PROJECT_ROOT"

# Check for required tools
print_header "Checking Prerequisites"

check_command() {
    if command -v "$1" &> /dev/null; then
        print_success "$1 found: $(command -v $1)"
        return 0
    else
        print_error "$1 not found"
        return 1
    fi
}

MISSING_DEPS=false
check_command cmake || MISSING_DEPS=true
check_command make || MISSING_DEPS=true

if [[ "$OS" == "macOS" ]]; then
    check_command clang++ || MISSING_DEPS=true
else
    if ! check_command g++ && ! check_command clang++; then
        print_error "Neither g++ nor clang++ found"
        MISSING_DEPS=true
    fi
fi

if [[ "$MISSING_DEPS" == true ]]; then
    print_error "Missing required dependencies. Please install them first."
    echo ""
    if [[ "$OS" == "Linux" ]]; then
        print_info "On Ubuntu/Debian: sudo apt-get install build-essential cmake"
        print_info "On Fedora: sudo dnf install gcc-c++ cmake make"
        print_info "On Arch: sudo pacman -S base-devel cmake"
    else
        print_info "On macOS: xcode-select --install"
        print_info "         brew install cmake"
    fi
    exit 1
fi

# Set CMake verbose flag if requested
CMAKE_VERBOSE=""
if [[ "$VERBOSE" == true ]]; then
    CMAKE_VERBOSE="--verbose"
fi

# Build Neutron
build_neutron() {
    print_header "Building Neutron Interpreter"
    
    cd "$PROJECT_ROOT"
    
    if [[ "$CLEAN_BUILD" == true ]] && [[ -d "build" ]]; then
        print_warning "Cleaning Neutron build directory..."
        rm -rf build
    fi
    
    print_info "Configuring Neutron..."
    cmake -B build -DCMAKE_BUILD_TYPE="$BUILD_TYPE" || {
        print_error "Neutron configuration failed"
        return 1
    }
    
    print_info "Building Neutron with $JOBS jobs..."
    cmake --build build --config "$BUILD_TYPE" -j "$JOBS" $CMAKE_VERBOSE || {
        print_error "Neutron build failed"
        return 1
    }
    
    # Check if binary was created
    if [[ -f "build/neutron" ]]; then
        print_success "Neutron built successfully: build/neutron"
        
        # Show version
        NEUTRON_VERSION=$(./build/neutron --version 2>/dev/null || echo "unknown")
        print_info "Version: $NEUTRON_VERSION"
    else
        print_error "Neutron binary not found after build"
        return 1
    fi
    
    return 0
}

# Build Box
build_box() {
    print_header "Building Box Package Manager"
    
    local BOX_DIR="$PROJECT_ROOT/nt-box"
    
    if [[ ! -d "$BOX_DIR" ]]; then
        print_error "Box directory not found: $BOX_DIR"
        return 1
    fi
    
    cd "$BOX_DIR"
    
    if [[ "$CLEAN_BUILD" == true ]] && [[ -d "build" ]]; then
        print_warning "Cleaning Box build directory..."
        rm -rf build
    fi
    
    print_info "Configuring Box..."
    cmake -B build -DCMAKE_BUILD_TYPE="$BUILD_TYPE" || {
        print_error "Box configuration failed"
        return 1
    }
    
    print_info "Building Box with $JOBS jobs..."
    cmake --build build --config "$BUILD_TYPE" -j "$JOBS" $CMAKE_VERBOSE || {
        print_error "Box build failed"
        return 1
    }
    
    # Check if binary was created
    if [[ -f "build/box" ]]; then
        print_success "Box built successfully: nt-box/build/box"
        
        # Show version
        BOX_VERSION=$(./build/box --version 2>/dev/null || echo "unknown")
        print_info "Version: $BOX_VERSION"
    else
        print_error "Box binary not found after build"
        return 1
    fi
    
    return 0
}

# Run tests
run_tests() {
    print_header "Running Test Suite"
    
    cd "$PROJECT_ROOT"
    
    if [[ ! -f "build/neutron" ]]; then
        print_error "Neutron binary not found. Build first."
        return 1
    fi
    
    # Check if test runner exists
    if [[ -f "run_tests.sh" ]]; then
        print_info "Running tests with run_tests.sh..."
        bash run_tests.sh || {
            print_error "Tests failed"
            return 1
        }
        print_success "All tests passed"
    else
        print_warning "Test runner not found: run_tests.sh"
        print_info "Skipping tests..."
    fi
    
    return 0
}

# Main build process
main() {
    local START_TIME=$(date +%s)
    local FAILED=false
    
    # Build Neutron
    if ! build_neutron; then
        FAILED=true
    fi
    
    # Build Box
    if ! build_box; then
        FAILED=true
    fi
    
    # Run tests if requested
    if [[ "$RUN_TESTS" == true ]] && [[ "$FAILED" == false ]]; then
        if ! run_tests; then
            FAILED=true
        fi
    fi
    
    # Calculate build time
    local END_TIME=$(date +%s)
    local ELAPSED=$((END_TIME - START_TIME))
    local MINUTES=$((ELAPSED / 60))
    local SECONDS=$((ELAPSED % 60))
    
    # Print summary
    print_header "Build Summary"
    
    if [[ "$FAILED" == true ]]; then
        print_error "Build failed"
        print_info "Time: ${MINUTES}m ${SECONDS}s"
        exit 1
    else
        print_success "All builds successful!"
        print_info "Time: ${MINUTES}m ${SECONDS}s"
        echo ""
        print_info "Binaries:"
        print_msg "$GREEN" "  • Neutron: build/neutron"
        print_msg "$GREEN" "  • Box:     nt-box/build/box"
        echo ""
        print_info "Next steps:"
        echo "  ./build/neutron --help"
        echo "  ./nt-box/build/box --help"
        
        if [[ "$RUN_TESTS" == false ]]; then
            echo ""
            print_info "To run tests: $0 -t"
        fi
    fi
}

# Run main
main
