# Neutron Test Runner (PowerShell)
# Runs all test files in the tests/ directory

param(
    [switch]$Verbose
)

# Set error action
$ErrorActionPreference = "Stop"

# Get script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

# Check if neutron binary exists
if (-not (Test-Path ".\neutron.exe")) {
    Write-Host "Error: neutron.exe not found" -ForegroundColor Red
    Write-Host "Please build the project first"
    exit 1
}

# Find all test files
$TestFiles = Get-ChildItem -Path "tests\test_*.nt" -File

if ($TestFiles.Count -eq 0) {
    Write-Host "No test files found in tests\ directory" -ForegroundColor Yellow
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
if ($Failed -eq 0) {
    Write-Host "All tests passed!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "Some tests failed!" -ForegroundColor Red
    exit 1
}
