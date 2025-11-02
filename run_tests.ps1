# Neutron Test Suite v2.0 - PowerShell Version
param([switch]$Verbose)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot

# Colors
$Red = "`e[31m"
$Green = "`e[32m"
$Yellow = "`e[33m"
$Cyan = "`e[36m"
$Bold = "`e[1m"
$Reset = "`e[0m"

if (-not (Test-Path (Join-Path $ProjectRoot "CMakeLists.txt"))) {
    Write-Host "${Red}Error: CMakeLists.txt not found${Reset}"
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
    Write-Host "${Red}Error: neutron.exe not found${Reset}"
    exit 1
}

# Print header
Write-Host "${Cyan}╔════════════════════════════════╗${Reset}"
Write-Host "${Cyan}║  Neutron Test Suite v2.0       ║${Reset}"
Write-Host "${Cyan}╚════════════════════════════════╝${Reset}"
Write-Host ""

$TotalPassed = 0
$TotalFailed = 0

# Test directories in order
$TestDirs = @(
    "fixes",
    "core",
    "operators",
    "control-flow",
    "functions",
    "classes",
    "modules"
)

foreach ($Dir in $TestDirs) {
    $DirPath = Join-Path $ProjectRoot "tests\$Dir"
    if (-not (Test-Path $DirPath)) {
        continue
    }

    $TestFiles = Get-ChildItem "$DirPath\*.nt" -ErrorAction SilentlyContinue
    if (-not $TestFiles) {
        continue
    }

    Write-Host "${Bold}Testing: $Dir${Reset}"
    
    $DirPassed = 0
    $DirFailed = 0

    foreach ($Test in $TestFiles) {
        $TestName = [System.IO.Path]::GetFileNameWithoutExtension($Test.Name)
        
        # Run test and capture output
        $TempFile = [System.IO.Path]::GetTempFileName()
        $Output = & $ExePath $Test.FullName 2>&1 | Out-String
        $Output | Out-File -FilePath $TempFile -Encoding UTF8
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  ${Green}✓${Reset} $TestName"
            $DirPassed++
            $TotalPassed++
        } else {
            Write-Host "  ${Red}✗${Reset} $TestName"
            if ($Verbose -or $true) {
                Get-Content $TempFile | ForEach-Object { Write-Host "    $_" }
            }
            $DirFailed++
            $TotalFailed++
        }
        
        Remove-Item $TempFile -ErrorAction SilentlyContinue
    }
    
    Write-Host "  Summary: $DirPassed passed, $DirFailed failed"
    Write-Host ""
}

# Print summary
Write-Host "${Cyan}════ FINAL SUMMARY ═══${Reset}"
Write-Host "Total: $($TotalPassed + $TotalFailed)"
Write-Host "${Green}Passed: $TotalPassed${Reset}"
if ($TotalFailed -gt 0) {
    Write-Host "${Red}Failed: $TotalFailed${Reset}"
    Write-Host ""
    Write-Host "${Red}Failed tests:${Reset}"
    # Re-run to list failed tests
    foreach ($Dir in $TestDirs) {
        $DirPath = Join-Path $ProjectRoot "tests\$Dir"
        if (-not (Test-Path $DirPath)) { continue }
        $TestFiles = Get-ChildItem "$DirPath\*.nt" -ErrorAction SilentlyContinue
        foreach ($Test in $TestFiles) {
            $null = & $ExePath $Test.FullName 2>&1
            if ($LASTEXITCODE -ne 0) {
                Write-Host "  ${Red}✗${Reset} tests\$Dir\$($Test.Name)"
            }
        }
    }
}

Write-Host ""

if ($TotalFailed -eq 0) {
    Write-Host "${Green}🎉 All tests passed! 🎉${Reset}"
    exit 0
} else {
    exit 1
}
