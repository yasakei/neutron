# Neutron Release Builder for Windows
# Creates release packages with Neutron + Box binaries

param(
    [switch]$SkipTests = $false
)

# Colors for output
function Write-ColorMsg {
    param(
        [string]$Message,
        [string]$Color = "White"
    )
    Write-Host $Message -ForegroundColor $Color
}

# Get version from CHANGELOG.md
function Get-Version {
    $changelog = Get-Content "CHANGELOG.md"
    $versionLine = $changelog | Select-String -Pattern "^## v" | Select-Object -First 1
    if ($versionLine) {
        $version = $versionLine.Line -replace "^## v([0-9.]+).*", '$1'
        return $version
    }
    Write-ColorMsg "Could not extract version from CHANGELOG.md" "Red"
    exit 1
}

# Build Neutron
function Build-Neutron {
    Write-ColorMsg "Building Neutron..." "Blue"
    
    # Clean build directory
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
    }
    
    # Configure
    cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) {
        Write-ColorMsg "CMake configuration failed" "Red"
        exit 1
    }
    
    # Build
    cmake --build build --config Release
    if ($LASTEXITCODE -ne 0) {
        Write-ColorMsg "Build failed" "Red"
        exit 1
    }
    
    Write-ColorMsg "Neutron built successfully!" "Green"
}

# Build Box package manager
function Build-Box {
    Write-ColorMsg "Building Box..." "Blue"
    
    Push-Location nt-box
    
    # Clean build directory
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
    }
    
    # Configure
    cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) {
        Write-ColorMsg "CMake configuration failed" "Red"
        Pop-Location
        exit 1
    }
    
    # Build
    cmake --build build --config Release
    if ($LASTEXITCODE -ne 0) {
        Write-ColorMsg "Build failed" "Red"
        Pop-Location
        exit 1
    }
    
    Pop-Location
    
    Write-ColorMsg "Box built successfully!" "Green"
}

# Run tests
function Run-Tests {
    Write-ColorMsg "Running tests..." "Blue"
    
    if (Test-Path "run_tests.ps1") {
        & .\run_tests.ps1
        if ($LASTEXITCODE -ne 0) {
            Write-ColorMsg "Tests failed!" "Red"
            exit 1
        }
        Write-ColorMsg "All tests passed!" "Green"
    } else {
        Write-ColorMsg "No test script found, skipping tests" "Yellow"
    }
}

# Create release package
function Create-Package {
    param(
        [string]$Version
    )
    
    $packageName = "neutron-v${Version}-windows-x64"
    $packageDir = "releases\${packageName}"
    
    Write-ColorMsg "Creating release package: ${packageName}" "Blue"
    
    # Create directory structure
    New-Item -ItemType Directory -Force -Path "${packageDir}\bin" | Out-Null
    New-Item -ItemType Directory -Force -Path "${packageDir}\lib" | Out-Null
    New-Item -ItemType Directory -Force -Path "${packageDir}\include\neutron" | Out-Null
    New-Item -ItemType Directory -Force -Path "${packageDir}\docs" | Out-Null
    
    # Copy Neutron binary
    if (Test-Path "build\Release\neutron.exe") {
        Copy-Item "build\Release\neutron.exe" "${packageDir}\bin\"
    } elseif (Test-Path "build\neutron.exe") {
        Copy-Item "build\neutron.exe" "${packageDir}\bin\"
    } else {
        Write-ColorMsg "neutron.exe not found" "Red"
        exit 1
    }
    
    # Copy Box binary
    if (Test-Path "nt-box\build\Release\box.exe") {
        Copy-Item "nt-box\build\Release\box.exe" "${packageDir}\bin\"
    } elseif (Test-Path "nt-box\build\box.exe") {
        Copy-Item "nt-box\build\box.exe" "${packageDir}\bin\"
    } else {
        Write-ColorMsg "Warning: box.exe not found" "Yellow"
    }
    
    # Copy standard library modules (.nt files)
    if (Test-Path "lib") {
        Copy-Item "lib\*.nt" "${packageDir}\lib\" -ErrorAction SilentlyContinue
    }
    
    # Copy headers
    Copy-Item -Recurse "include\*" "${packageDir}\include\neutron\"
    
    # Copy documentation
    Copy-Item "README.md" "${packageDir}\"
    Copy-Item "LICENSE" "${packageDir}\"
    Copy-Item "CHANGELOG.md" "${packageDir}\"
    Copy-Item -Recurse "docs" "${packageDir}\"
    
    # Create installation script
    @'
@echo off
REM Installation script for Neutron on Windows

set PREFIX=%ProgramFiles%\Neutron

echo Installing Neutron to %PREFIX%...

REM Create directories
mkdir "%PREFIX%\bin" 2>nul
mkdir "%PREFIX%\lib" 2>nul
mkdir "%PREFIX%\include\neutron" 2>nul

REM Copy binaries
copy /Y bin\neutron.exe "%PREFIX%\bin\"
copy /Y bin\box.exe "%PREFIX%\bin\"

REM Copy libraries
copy /Y lib\*.nt "%PREFIX%\lib\" 2>nul

REM Copy headers
xcopy /E /I /Y include\neutron "%PREFIX%\include\neutron"

REM Add to PATH
setx PATH "%PATH%;%PREFIX%\bin"

echo Installation complete!
echo Run 'neutron --version' to verify installation
echo NOTE: You may need to restart your terminal for PATH changes to take effect
pause
'@ | Out-File -FilePath "${packageDir}\install.bat" -Encoding ASCII
    
    # Create README for package
    @"
Neutron Programming Language v${Version}
====================================

This package contains:
- neutron.exe: The Neutron interpreter and compiler
- box.exe: The Box package manager for native modules
- Standard library (.nt modules)
- C API headers for module development
- Documentation

Installation
------------

Option 1: Run the installation script (requires Administrator):
    install.bat

Option 2: Add to PATH manually:
    1. Copy bin\neutron.exe and bin\box.exe to a directory in your PATH
    2. Or add this directory to your system PATH

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

Requirements
------------

- Windows 10 or later (x64)
- Visual C++ Redistributable 2015-2022 (included with Windows 10+)

For module development:
- Visual Studio 2019 or later (with C++ tools)
- CMake 3.15 or later

Documentation
-------------

See the docs\ directory or visit:
https://github.com/yasakei/neutron

License
-------

See LICENSE file for details.
"@ | Out-File -FilePath "${packageDir}\README.txt" -Encoding UTF8
    
    # Create ZIP archive
    Write-ColorMsg "Creating ZIP archive..." "Blue"
    $zipPath = "releases\${packageName}.zip"
    if (Test-Path $zipPath) {
        Remove-Item $zipPath
    }
    Compress-Archive -Path $packageDir -DestinationPath $zipPath
    
    # Calculate checksums
    Write-ColorMsg "Generating checksums..." "Blue"
    $sha256 = (Get-FileHash $zipPath -Algorithm SHA256).Hash
    $md5 = (Get-FileHash $zipPath -Algorithm MD5).Hash
    
    $sha256 | Out-File -FilePath "${zipPath}.sha256" -Encoding ASCII
    $md5 | Out-File -FilePath "${zipPath}.md5" -Encoding ASCII
    
    Write-ColorMsg "Package created: $zipPath" "Green"
    Write-ColorMsg "SHA256: $sha256" "Yellow"
}

# Main function
function Main {
    Write-ColorMsg "=== Neutron Release Builder ===" "Blue"
    
    # Check we're in the right directory
    if (-not (Test-Path "CMakeLists.txt") -or -not (Test-Path "nt-box")) {
        Write-ColorMsg "Error: Must run from Neutron root directory" "Red"
        exit 1
    }
    
    # Get version
    $version = Get-Version
    Write-ColorMsg "Version: $version" "Yellow"
    
    # Clean previous releases
    New-Item -ItemType Directory -Force -Path "releases" | Out-Null
    
    # Build everything
    Build-Neutron
    Build-Box
    
    # Run tests (optional)
    if (-not $SkipTests) {
        Run-Tests
    }
    
    # Create release package
    Create-Package -Version $version
    
    Write-ColorMsg "=== Release build complete! ===" "Green"
    Write-ColorMsg "Package: releases\neutron-v${version}-windows-x64.zip" "Green"
    Write-ColorMsg "" "White"
    Write-ColorMsg "To upload to GitHub:" "Blue"
    Write-ColorMsg "  gh release create v${version} releases\neutron-v${version}-windows-x64.zip --title `"Neutron v${version}`" --notes-file CHANGELOG.md" "Yellow"
}

# Run main
Main
