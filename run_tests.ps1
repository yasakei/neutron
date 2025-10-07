# Neutron Test Runner
param([switch]$Verbose)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot

if (-not (Test-Path (Join-Path $ProjectRoot "CMakeLists.txt"))) {
    Write-Host "Error: CMakeLists.txt not found"
    exit 1
}

$ExePaths = @(
    "$ProjectRoot\build\neutron.exe",
    "$ProjectRoot\build\Release\neutron.exe",
    "$ProjectRoot\build\Debug\neutron.exe",
    "$ProjectRoot\neutron.exe"
)

$ExePath = $null
foreach ($Path in $ExePaths) {
    if (Test-Path $Path) {
        $ExePath = $Path
        break
    }
}

if (-not $ExePath) {
    Write-Host "Error: neutron.exe not found"
    exit 1
}

Write-Host "Using: $ExePath"

$TestFiles = Get-ChildItem "$ProjectRoot\tests\test_*.nt" -ErrorAction SilentlyContinue
if (-not $TestFiles) {
    Write-Host "No tests found"
    exit 0
}

$Passed = 0
$Failed = 0

foreach ($Test in $TestFiles) {
    Write-Host "Running: $($Test.Name) ... " -NoNewline
    $Output = & $ExePath $Test.FullName 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "PASS"
        $Passed++
    } else {
        Write-Host "FAIL"
        if ($Verbose) { Write-Host $Output }
        $Failed++
    }
}

Write-Host ""
Write-Host "Passed: $Passed, Failed: $Failed"
if ($Failed -gt 0) { exit 1 }
exit 0
