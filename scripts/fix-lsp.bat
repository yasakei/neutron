@echo off
REM Fix neutron-lsp dependency issues on Windows

echo Neutron LSP Dependency Fix Script (Windows)
echo ============================================

REM Check if we're in the neutron source directory
if not exist "CMakeLists.txt" (
    echo Error: This script must be run from the Neutron source directory
    echo Make sure you're in the directory containing CMakeLists.txt
    pause
    exit /b 1
)

if not exist "build" (
    echo Error: Build directory not found. Please run 'python package.py' first.
    pause
    exit /b 1
)

echo Rebuilding neutron-lsp with current system libraries...

REM Find cmake
set CMAKE_CMD=cmake
where cmake >nul 2>&1
if errorlevel 1 (
    REM Try Visual Studio locations
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set CMAKE_CMD="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set CMAKE_CMD="C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    ) else (
        echo Error: CMake not found. Please install Visual Studio Build Tools or CMake.
        pause
        exit /b 1
    )
)

REM Rebuild neutron-lsp
%CMAKE_CMD% --build build --target neutron-lsp --config Release

REM Check if it was built successfully
if not exist "build\Release\neutron-lsp.exe" (
    if not exist "build\neutron-lsp.exe" (
        echo Error: neutron-lsp build failed
        pause
        exit /b 1
    )
)

echo Copying neutron-lsp to project root...
if exist "build\Release\neutron-lsp.exe" (
    copy "build\Release\neutron-lsp.exe" "neutron-lsp.exe"
) else (
    copy "build\neutron-lsp.exe" "neutron-lsp.exe"
)

echo Copying required DLLs...
REM Copy vcpkg DLLs if they exist
if exist "build\vcpkg_installed\x64-windows\bin\jsoncpp.dll" (
    copy "build\vcpkg_installed\x64-windows\bin\jsoncpp.dll" "."
    echo Copied jsoncpp.dll
)
if exist "build\vcpkg_installed\x64-windows\bin\libcurl.dll" (
    copy "build\vcpkg_installed\x64-windows\bin\libcurl.dll" "."
    echo Copied libcurl.dll
)
if exist "build\vcpkg_installed\x64-windows\bin\zlib1.dll" (
    copy "build\vcpkg_installed\x64-windows\bin\zlib1.dll" "."
    echo Copied zlib1.dll
)

echo Testing neutron-lsp...
neutron-lsp.exe --version >nul 2>&1
if errorlevel 1 (
    echo Warning: neutron-lsp may have dependency issues
    echo Make sure Visual C++ Redistributable is installed
    echo Download from: https://aka.ms/vs/17/release/vc_redist.x64.exe
) else (
    echo neutron-lsp is working correctly!
)

echo.
echo LSP server files updated in current directory.
echo If using VS Code, configure the extension to use the local path:
echo   "neutron.lsp.path": "%CD%\neutron-lsp.exe"
echo.
pause