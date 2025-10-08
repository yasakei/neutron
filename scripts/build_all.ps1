# build_all.ps1 - Build Neutron and Box Package Manager on Windows
# Supports both MSVC and MINGW64 environments

param(
    [switch]$Debug,          # Build in Debug mode
    [switch]$Clean,          # Clean build directories
    [switch]$Test,           # Run test suite after building
    [switch]$Verbose,        # Verbose build output
    [int]$Jobs = 0,          # Number of parallel jobs (0 = auto)
    [switch]$MINGW,          # Force MINGW64 build
    [switch]$MSVC,           # Force MSVC build
    [switch]$Help            # Show help
)

# Colors for output
function Write-ColorOutput {
    param(
        [string]$Message,
        [string]$Color = "White"
    )
    Write-Host $Message -ForegroundColor $Color
}

function Write-Header {
    param([string]$Message)
    Write-Host ""
    Write-ColorOutput "═══════════════════════════════════════════════════════════════" "Blue"
    Write-ColorOutput "  $Message" "Blue"
    Write-ColorOutput "═══════════════════════════════════════════════════════════════" "Blue"
    Write-Host ""
}

function Write-Success {
    Write-ColorOutput "✓ $args" "Green"
}

function Write-Error-Custom {
    Write-ColorOutput "✗ $args" "Red"
}

function Write-Warning-Custom {
    Write-ColorOutput "⚠ $args" "Yellow"
}

function Write-Info {
    Write-ColorOutput "ℹ $args" "Cyan"
}

# Show usage
function Show-Usage {
    Write-Host @"
Usage: .\build_all.ps1 [OPTIONS]

Build Neutron interpreter and Box package manager on Windows.

OPTIONS:
    -Help           Show this help message
    -Debug          Build in Debug mode (default: Release)
    -Clean          Clean build directories before building
    -Test           Run test suite after building
    -Verbose        Verbose build output
    -Jobs N         Number of parallel jobs (default: auto)
    -MINGW          Force MINGW64/GCC build
    -MSVC           Force MSVC build (default if available)

EXAMPLES:
    .\build_all.ps1                 # Build everything in Release mode
    .\build_all.ps1 -Debug          # Build in Debug mode
    .\build_all.ps1 -Clean -Test    # Clean build and run tests
    .\build_all.ps1 -MINGW          # Force MINGW64 build
    .\build_all.ps1 -Debug -Verbose -Jobs 8

NOTES:
    - Requires Visual Studio Build Tools (MSVC) or MSYS2 (MINGW64)
    - For MSVC: Run from Developer Command Prompt or Visual Studio terminal
    - For MINGW64: Run from MINGW64 terminal
    - CMake must be in PATH

"@
    exit 0
}

if ($Help) {
    Show-Usage
}

# Configuration
$BuildType = if ($Debug) { "Debug" } else { "Release" }
$ErrorActionPreference = "Stop"

# Detect number of processors if not specified
if ($Jobs -eq 0) {
    $Jobs = [Environment]::ProcessorCount
}

# Get project root (script is in scripts/)
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir

# Detect build environment
$IsMinGW = $false
$IsMSVC = $false
$Compiler = "Unknown"
$Generator = "Ninja"

if ($MINGW -and $MSVC) {
    Write-Error-Custom "Cannot specify both -MINGW and -MSVC"
    exit 1
}

# Auto-detect or use forced environment
if ($MINGW) {
    $IsMinGW = $true
    $Compiler = "MINGW64"
    $Generator = "MSYS Makefiles"
} elseif ($MSVC) {
    $IsMSVC = $true
    $Compiler = "MSVC"
    $Generator = "Visual Studio 17 2022"
} else {
    # Auto-detect
    if ($env:MSYSTEM -match "MINGW|MSYS") {
        $IsMinGW = $true
        $Compiler = "MINGW64"
        $Generator = "MSYS Makefiles"
    } elseif ($env:VCINSTALLDIR -or $env:VisualStudioVersion) {
        $IsMSVC = $true
        $Compiler = "MSVC"
        # Try to use Ninja if available, otherwise Visual Studio generator
        if (Get-Command ninja -ErrorAction SilentlyContinue) {
            $Generator = "Ninja"
        } else {
            $Generator = "Visual Studio 17 2022"
        }
    } else {
        Write-Warning-Custom "Could not auto-detect build environment"
        Write-Info "Defaulting to MSVC. Run from Developer Command Prompt or MINGW64 terminal."
        Write-Info "Or use -MINGW or -MSVC to force a specific toolchain."
        $IsMSVC = $true
        $Compiler = "MSVC"
        $Generator = "Visual Studio 17 2022"
    }
}

Write-Header "Build Configuration"
Write-Info "OS: Windows"
Write-Info "Compiler: $Compiler"
Write-Info "Generator: $Generator"
Write-Info "Build Type: $BuildType"
Write-Info "Jobs: $Jobs"
Write-Info "Clean Build: $Clean"
Write-Info "Run Tests: $Test"
Write-Info "Verbose: $Verbose"
Write-Info "Project Root: $ProjectRoot"

# Check for required tools
Write-Header "Checking Prerequisites"

function Test-Command {
    param([string]$Command)
    
    try {
        $null = Get-Command $Command -ErrorAction Stop
        Write-Success "$Command found"
        return $true
    } catch {
        Write-Error-Custom "$Command not found"
        return $false
    }
}

$MissingDeps = $false

# Check CMake
if (-not (Test-Command "cmake")) {
    $MissingDeps = $true
}

# Check compiler
if ($IsMinGW) {
    if (-not (Test-Command "g++")) {
        $MissingDeps = $true
    }
    if (-not (Test-Command "make")) {
        $MissingDeps = $true
    }
} else {
    # For MSVC, cl.exe should be in PATH if in Developer Command Prompt
    if (-not (Test-Command "cl")) {
        Write-Warning-Custom "cl.exe not found in PATH"
        Write-Info "Make sure you're running from Developer Command Prompt"
        Write-Info "Or run: scripts\setup_msvc.bat (if available)"
    }
}

if ($MissingDeps) {
    Write-Error-Custom "Missing required dependencies"
    Write-Host ""
    if ($IsMinGW) {
        Write-Info "For MINGW64, install MSYS2 from https://www.msys2.org/"
        Write-Info "Then run in MINGW64 terminal:"
        Write-Info "  pacman -S mingw-w64-x86_64-gcc"
        Write-Info "  pacman -S mingw-w64-x86_64-cmake"
        Write-Info "  pacman -S make"
    } else {
        Write-Info "For MSVC, install Visual Studio Build Tools:"
        Write-Info "  https://visualstudio.microsoft.com/downloads/"
        Write-Info "Select 'Desktop development with C++'"
    }
    exit 1
}

# Set CMake verbose flag if requested
$CMakeVerbose = if ($Verbose) { "--verbose" } else { "" }

# Build Neutron
function Build-Neutron {
    Write-Header "Building Neutron Interpreter"
    
    Set-Location $ProjectRoot
    
    if ($Clean -and (Test-Path "build")) {
        Write-Warning-Custom "Cleaning Neutron build directory..."
        Remove-Item -Recurse -Force "build"
    }
    
    try {
        Write-Info "Configuring Neutron..."
        if ($IsMinGW) {
            cmake -B build -G "$Generator" -DCMAKE_BUILD_TYPE="$BuildType"
        } else {
            cmake -B build -G "$Generator"
        }
        
        Write-Info "Building Neutron with $Jobs jobs..."
        if ($Verbose) {
            cmake --build build --config $BuildType -j $Jobs --verbose
        } else {
            cmake --build build --config $BuildType -j $Jobs
        }
        
        # Check for binary
        $NeutronBinary = if ($IsMSVC) {
            "build\$BuildType\neutron.exe"
        } else {
            "build\neutron.exe"
        }
        
        if (Test-Path $NeutronBinary) {
            Write-Success "Neutron built successfully: $NeutronBinary"
            
            # Show version
            $Version = & $NeutronBinary --version 2>&1
            Write-Info "Version: $Version"
        } else {
            Write-Error-Custom "Neutron binary not found after build"
            Write-Info "Expected: $NeutronBinary"
            return $false
        }
        
        return $true
    } catch {
        Write-Error-Custom "Neutron build failed: $_"
        return $false
    }
}

# Build Box
function Build-Box {
    Write-Header "Building Box Package Manager"
    
    $BoxDir = Join-Path $ProjectRoot "nt-box"
    
    if (-not (Test-Path $BoxDir)) {
        Write-Error-Custom "Box directory not found: $BoxDir"
        return $false
    }
    
    Set-Location $BoxDir
    
    if ($Clean -and (Test-Path "build")) {
        Write-Warning-Custom "Cleaning Box build directory..."
        Remove-Item -Recurse -Force "build"
    }
    
    try {
        Write-Info "Configuring Box..."
        if ($IsMinGW) {
            cmake -B build -G "$Generator" -DCMAKE_BUILD_TYPE="$BuildType"
        } else {
            cmake -B build -G "$Generator"
        }
        
        Write-Info "Building Box with $Jobs jobs..."
        if ($Verbose) {
            cmake --build build --config $BuildType -j $Jobs --verbose
        } else {
            cmake --build build --config $BuildType -j $Jobs
        }
        
        # Check for binary
        $BoxBinary = if ($IsMSVC) {
            "build\$BuildType\box.exe"
        } else {
            "build\box.exe"
        }
        
        if (Test-Path $BoxBinary) {
            Write-Success "Box built successfully: nt-box\$BoxBinary"
            
            # Show version
            $Version = & $BoxBinary --version 2>&1
            Write-Info "Version: $Version"
        } else {
            Write-Error-Custom "Box binary not found after build"
            Write-Info "Expected: $BoxBinary"
            return $false
        }
        
        return $true
    } catch {
        Write-Error-Custom "Box build failed: $_"
        return $false
    }
}

# Run tests
function Run-Tests {
    Write-Header "Running Test Suite"
    
    Set-Location $ProjectRoot
    
    # Determine Neutron binary path
    $NeutronBinary = if ($IsMSVC) {
        "build\$BuildType\neutron.exe"
    } else {
        "build\neutron.exe"
    }
    
    if (-not (Test-Path $NeutronBinary)) {
        Write-Error-Custom "Neutron binary not found. Build first."
        return $false
    }
    
    # Check if test runner exists
    if (Test-Path "run_tests.ps1") {
        Write-Info "Running tests with run_tests.ps1..."
        try {
            & ".\run_tests.ps1"
            Write-Success "All tests passed"
            return $true
        } catch {
            Write-Error-Custom "Tests failed: $_"
            return $false
        }
    } else {
        Write-Warning-Custom "Test runner not found: run_tests.ps1"
        Write-Info "Skipping tests..."
        return $true
    }
}

# Main build process
function Main {
    $StartTime = Get-Date
    $Failed = $false
    
    # Build Neutron
    if (-not (Build-Neutron)) {
        $Failed = $true
    }
    
    # Build Box
    if (-not (Build-Box)) {
        $Failed = $true
    }
    
    # Run tests if requested
    if ($Test -and -not $Failed) {
        if (-not (Run-Tests)) {
            $Failed = $true
        }
    }
    
    # Calculate build time
    $EndTime = Get-Date
    $Elapsed = $EndTime - $StartTime
    $Minutes = [int]$Elapsed.TotalMinutes
    $Seconds = $Elapsed.Seconds
    
    # Print summary
    Set-Location $ProjectRoot
    Write-Header "Build Summary"
    
    if ($Failed) {
        Write-Error-Custom "Build failed"
        Write-Info "Time: ${Minutes}m ${Seconds}s"
        exit 1
    } else {
        Write-Success "All builds successful!"
        Write-Info "Time: ${Minutes}m ${Seconds}s"
        Write-Host ""
        Write-Info "Binaries:"
        
        if ($IsMSVC) {
            Write-ColorOutput "  • Neutron: build\$BuildType\neutron.exe" "Green"
            Write-ColorOutput "  • Box:     nt-box\build\$BuildType\box.exe" "Green"
        } else {
            Write-ColorOutput "  • Neutron: build\neutron.exe" "Green"
            Write-ColorOutput "  • Box:     nt-box\build\box.exe" "Green"
        }
        
        Write-Host ""
        Write-Info "Next steps:"
        if ($IsMSVC) {
            Write-Host "  .\build\$BuildType\neutron.exe --help"
            Write-Host "  .\nt-box\build\$BuildType\box.exe --help"
        } else {
            Write-Host "  .\build\neutron.exe --help"
            Write-Host "  .\nt-box\build\box.exe --help"
        }
        
        if (-not $Test) {
            Write-Host ""
            Write-Info "To run tests: .\scripts\build_all.ps1 -Test"
        }
    }
}

# Run main
try {
    Main
} catch {
    Write-Error-Custom "Unexpected error: $_"
    exit 1
}
