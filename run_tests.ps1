# Neutron Test Runner (PowerShell)

param(
    [switch]$Verbose
)

# Set error action
$ErrorActionPreference = "Stop"

# --- Start of New Logic ---

# 1. Find the project root by searching upwards for a marker file (CMakeLists.txt)
$CurrentDir = $PSScriptRoot # $PSScriptRoot is the directory of the script
$ProjectRoot = ""
while ($CurrentDir -and (Get-Item $CurrentDir).Parent) {
    if (Test-Path (Join-Path $CurrentDir "CMakeLists.txt")) {
        $ProjectRoot = $CurrentDir
        break
    }
    $CurrentDir = (Get-Item $CurrentDir).Parent.FullName
}

if (-not $ProjectRoot) {
    Write-Host "Error: Could not find project root (CMakeLists.txt not found)." -ForegroundColor Red
    exit 1
}

# 2. Define key paths and change the working directory
$BuildDir = Join-Path $ProjectRoot "build\Release"
$TestsDir = Join-Path $ProjectRoot "tests"
$ExecutablePath = Join-Path $BuildDir "neutron.exe"

try {
    Set-Location $BuildDir
    Write-Host "Changed working directory to: $BuildDir" -ForegroundColor Gray
} catch {
    Write-Host "Error: Build directory not found at '$BuildDir'" -ForegroundColor Red
    Write-Host "Please build the project in Release mode first."
    exit 1
}

# --- End of New Logic ---


# Check if neutron binary exists
if (-not (Test-Path $ExecutablePath)) {
    Write-Host "Error: neutron.exe not found at '$ExecutablePath'" -ForegroundColor Red
    Write-Host "Please build the project first."
    exit 1
}

# Find all test files using the full path
$TestFiles = Get-ChildItem -Path (Join-Path $TestsDir "test_*.nt") -File

if ($TestFiles.Count -eq 0) {
    Write-Host "No test files found in '$TestsDir' directory" -ForegroundColor Yellow
    exit 0
}

# Test results
$Passed = 0
$Failed = 0
$FailedTests = @()

Write-Host "================================" -ForegroundColor Cyan
Write-Host "  Neutron Test Suite" -ForegroundColor Cyan
Write-Host "================================" -ForegroundColor Cyan
Write-Host ""

# Run each test
foreach ($TestFile in $TestFiles) {
    $TestName = $TestFile.Name
    Write-Host "Running: $TestName" -ForegroundColor Yellow

    # Run the test and capture output
    try {
        # The executable is now in the current directory (.\neutron.exe)
        $Output = & ".\neutron.exe" $TestFile.FullName 2>&1
        $ExitCode = $LASTEXITCODE

        if ($ExitCode -eq 0) {
            Write-Host "✓ PASSED" -ForegroundColor Green
            if ($Verbose) {
                Write-Host $Output
            }
            $Passed++
        } else {
            Write-Host "✗ FAILED" -ForegroundColor Red
            Write-Host $Output
            $FailedTests += $TestName
            $Failed++
        }
    } catch {
        Write-Host "✗ FAILED (Exception)" -ForegroundColor Red
        Write-Host $_.Exception.Message
        $FailedTests += $TestName
        $Failed++
    }

    Write-Host ""
}

# Print summary
Write-Host "================================" -ForegroundColor Cyan
Write-Host "  Test Summary" -ForegroundColor Cyan
Write-Host "================================" -ForegroundColor Cyan
Write-Host "Total tests: $($Passed + $Failed)"
Write-Host "Passed: $Passed" -ForegroundColor Green
Write-Host "Failed: $Failed" -ForegroundColor Red

if ($Failed -gt 0) {
    Write-Host ""
    Write-Host "Failed tests:" -ForegroundColor Red
    foreach ($Test in $FailedTests) {
        Write-Host "  ✗ $Test" -ForegroundColor Red
    }
}

Write-Host ""

# Exit with appropriate code
$state = 0;
if ($Failed -eq 0) {
    Write-Host "All tests passed!" -ForegroundColor Green
} else {
    Write-Host "Some tests failed!" -ForegroundColor Red
    $state = 1
}

Set-Location $ProjectRoot
Write-Host "Changed working directory to: $ProjectRoot" -ForegroundColor Gray
exit $state;