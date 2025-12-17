# Neutron Windows Compiler Setup Script
# Downloads and installs Microsoft C++ Build Tools (MSVC)

$ErrorActionPreference = "Stop"

Write-Host "Neutron Compiler Setup for Windows" -ForegroundColor Cyan
Write-Host "===================================" -ForegroundColor Cyan
Write-Host ""

# Check if cl.exe is already available
$clExists = Get-Command cl -ErrorAction SilentlyContinue
if ($clExists) {
    Write-Host "✓ MSVC (cl.exe) is already installed and in PATH" -ForegroundColor Green
    cl 2>&1 | Select-Object -First 2
    Write-Host ""
    Write-Host "You're all set! You can now use 'box install' to build native modules." -ForegroundColor Green
    exit 0
}

Write-Host "MSVC not found in PATH. Setting up Microsoft C++ Build Tools..." -ForegroundColor Yellow
Write-Host ""

# Define download URL
$buildToolsUrl = "https://aka.ms/vs/17/release/vs_BuildTools.exe"
$installerPath = "$env:TEMP\vs_BuildTools.exe"

Write-Host "This will download and install Microsoft C++ Build Tools." -ForegroundColor Cyan
Write-Host "Download size: ~1.5GB" -ForegroundColor Gray
Write-Host "Install size: ~6-7GB" -ForegroundColor Gray
Write-Host ""
Write-Host "The installer will:" -ForegroundColor Cyan
Write-Host "  1. Download Visual Studio Build Tools installer" -ForegroundColor Cyan
Write-Host "  2. Launch the installer with C++ workload pre-selected" -ForegroundColor Cyan
Write-Host "  3. You'll need to click 'Install' in the installer window" -ForegroundColor Cyan
Write-Host ""

# Ask for confirmation
$confirm = Read-Host "Continue with installation? (Y/N)"
if ($confirm -ne "Y" -and $confirm -ne "y") {
    Write-Host "Installation cancelled." -ForegroundColor Yellow
    exit 1
}

# Download Build Tools installer
Write-Host ""
Write-Host "Downloading Visual Studio Build Tools installer..." -ForegroundColor Cyan
try {
    Invoke-WebRequest -Uri $buildToolsUrl -OutFile $installerPath -UseBasicParsing
    Write-Host "✓ Download complete" -ForegroundColor Green
} catch {
    Write-Host "✗ Download failed: $_" -ForegroundColor Red
    exit 1
}

# Launch installer with C++ workload
Write-Host ""
Write-Host "Launching installer..." -ForegroundColor Cyan
Write-Host "Please follow the installer prompts to complete installation." -ForegroundColor Yellow
Write-Host ""

try {
    # Launch with Desktop development with C++ workload pre-selected
    $process = Start-Process -FilePath $installerPath `
        -ArgumentList "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive --wait" `
        -Wait -PassThru
    
    if ($process.ExitCode -eq 0) {
        Write-Host "✓ Installation completed successfully" -ForegroundColor Green
    } else {
        Write-Host "⚠ Installer exited with code: $($process.ExitCode)" -ForegroundColor Yellow
        Write-Host "The installation may have been cancelled or failed." -ForegroundColor Yellow
    }
} catch {
    Write-Host "✗ Failed to launch installer: $_" -ForegroundColor Red
    exit 1
}

# Clean up installer
Remove-Item -Path $installerPath -Force -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "Setup complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "IMPORTANT: To use the compiler, you must:" -ForegroundColor Yellow
Write-Host "  1. Close this terminal" -ForegroundColor Yellow
Write-Host "  2. Open 'Developer Command Prompt for VS 2022'" -ForegroundColor Yellow
Write-Host "     (Search for it in Start Menu)" -ForegroundColor Yellow
Write-Host "  3. Run 'box install <module>' from there" -ForegroundColor Yellow
Write-Host ""
Write-Host "Alternatively, run this before using box in any terminal:" -ForegroundColor Cyan
Write-Host '  & "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"' -ForegroundColor Gray
Write-Host ""
