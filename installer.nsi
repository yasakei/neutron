; Neutron Installer Script
; Requires NSIS (Nullsoft Scriptable Install System)

!include "MUI2.nsh"
!include "LogicLib.nsh"

!ifndef VERSION
  !define VERSION "Unknown"
!endif

;--------------------------------
;General

  ;Name and file
  Name "Neutron ${VERSION}"
  OutFile "NeutronInstaller.exe"
  Unicode True

  ;Default installation folder
  InstallDir "$PROGRAMFILES64\Neutron"
  
  ;Get installation folder from registry if available
  InstallDirRegKey HKCU "Software\Neutron" ""

  ;Request application privileges for Windows Vista
  RequestExecutionLevel admin

;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING

;--------------------------------
;Pages

  !define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of Neutron.$\r$\n$\r$\nNeutron is a modern programming language with a focus on simplicity and performance.$\r$\n$\r$\nNote: To build projects with 'neutron build', you'll need Microsoft C++ Build Tools. You can install them during this setup or later."
  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE "LICENSE"
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  
  !define MUI_FINISHPAGE_TITLE "Neutron Installation Complete"
  !define MUI_FINISHPAGE_TEXT "Neutron has been installed successfully!$\r$\n$\r$\nQuick Start:$\r$\n  1. Open a new terminal (Command Prompt or PowerShell)$\r$\n  2. Run: neutron --version$\r$\n  3. Create a project: neutron init myproject$\r$\n$\r$\nIf you didn't install C++ Build Tools, you can:$\r$\n  - Download from: https://aka.ms/vs/17/release/vs_BuildTools.exe$\r$\n  - Or run the installer again and select the Build Tools option$\r$\n$\r$\nDocumentation: https://neutron.ct.ws/docs"
  !insertmacro MUI_PAGE_FINISH

  !insertmacro MUI_UNPAGE_WELCOME
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Section "Neutron Core" SecNeutron

  ; Install runtime binaries to the installation root ($INSTDIR)
  SetOutPath "$INSTDIR"
  
  ; Core executables
  File "neutron.exe"
  File "box.exe"
  File /nonfatal "neutron-lsp.exe"
  
  ; Shared runtime DLL (required for native modules)
  File "neutron_shared.dll"
  
  ; Copy dependency DLLs
  File /nonfatal "libcurl.dll"
  File /nonfatal "zlib1.dll"
  File /nonfatal "dl.dll"
  
  ; Copy vcpkg DLLs for neutron-lsp
  File /nonfatal "build\vcpkg_installed\x64-windows\bin\jsoncpp.dll"
  File /nonfatal "build\vcpkg_installed\x64-windows\bin\libcurl.dll"
  File /nonfatal "build\vcpkg_installed\x64-windows\bin\zlib1.dll"
  
  ; Copy Visual C++ runtime DLLs if available
  File /nonfatal "vcruntime140.dll"
  File /nonfatal "msvcp140.dll"
  File /nonfatal "vcruntime140_1.dll"
  
  ; Install headers (required for building native modules)
  ; Copy the entire include directory structure
  SetOutPath "$INSTDIR"
  File /r /x ".git" /x "neutron-*" /x "build" /x "*.obj" "include"
  
  ; Install libs directory (Neutron standard library source)
  SetOutPath "$INSTDIR"
  File /r /x ".git" /x "neutron-*" /x "build" /x "*.obj" "libs"
  
  ; Install vcpkg headers (curl and jsoncpp) for http module
  SetOutPath "$INSTDIR\vcpkg_installed\x64-windows\include\curl"
  File /nonfatal "build\vcpkg_installed\x64-windows\include\curl\*.h"
  
  SetOutPath "$INSTDIR\vcpkg_installed\x64-windows\include\json"
  File /nonfatal "build\vcpkg_installed\x64-windows\include\json\*.h"
  
  ; Install vcpkg libs to root directory for linking
  SetOutPath "$INSTDIR"
  File /nonfatal "build\vcpkg_installed\x64-windows\lib\libcurl.lib"
  File /nonfatal "build\vcpkg_installed\x64-windows\lib\jsoncpp.lib"
  
  ; Install native shim (required for building native modules)
  SetOutPath "$INSTDIR\nt-box\src"
  File /nonfatal "nt-box\src\native_shim.cpp"
  
  ; Install platform header (required by native_shim.cpp)
  SetOutPath "$INSTDIR\nt-box\include"
  File /nonfatal "nt-box\include\platform.h"
  
  ; Install dlfcn compatibility shim (required for neutron build on Windows)
  SetOutPath "$INSTDIR\src\platform"
  File /nonfatal "src\platform\dlfcn_compat_win.cpp"
  
  ; Install dlfcn header
  SetOutPath "$INSTDIR\include\cross-platfrom"
  File /nonfatal "include\cross-platfrom\dlfcn_compat.h"
  
  ; Install runtime library (required for neutron build) - copy to root like Linux
  SetOutPath "$INSTDIR"
  File /nonfatal "neutron_runtime.lib"
  File /nonfatal "neutron_shared.lib"
  
  ; Also copy to lib directory for alternative search path
  SetOutPath "$INSTDIR\lib"
  File /nonfatal "..\neutron_runtime.lib"
  File /nonfatal "..\neutron_shared.lib"

  ; Copy README/License to root
  SetOutPath "$INSTDIR"
  File /nonfatal "LICENSE"
  File /nonfatal "README.md"
  
  ; Create MSVC environment helper script
  FileOpen $0 "$INSTDIR\setup-msvc.bat" w
  FileWrite $0 "@echo off$\r$\n"
  FileWrite $0 "REM Neutron MSVC Environment Setup$\r$\n"
  FileWrite $0 "REM Run this script before using 'neutron build' to enable MSVC compiler$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 "echo Searching for MSVC installation...$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 "if exist $\"C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat$\" ($\r$\n"
  FileWrite $0 "  call $\"C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat$\"$\r$\n"
  FileWrite $0 "  goto :success$\r$\n"
  FileWrite $0 ")$\r$\n"
  FileWrite $0 "if exist $\"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat$\" ($\r$\n"
  FileWrite $0 "  call $\"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat$\"$\r$\n"
  FileWrite $0 "  goto :success$\r$\n"
  FileWrite $0 ")$\r$\n"
  FileWrite $0 "if exist $\"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat$\" ($\r$\n"
  FileWrite $0 "  call $\"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat$\"$\r$\n"
  FileWrite $0 "  goto :success$\r$\n"
  FileWrite $0 ")$\r$\n"
  FileWrite $0 "if exist $\"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat$\" ($\r$\n"
  FileWrite $0 "  call $\"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat$\"$\r$\n"
  FileWrite $0 "  goto :success$\r$\n"
  FileWrite $0 ")$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 "echo Error: Could not find MSVC installation$\r$\n"
  FileWrite $0 "echo Please install Visual Studio Build Tools from:$\r$\n"
  FileWrite $0 "echo https://aka.ms/vs/17/release/vs_BuildTools.exe$\r$\n"
  FileWrite $0 "exit /b 1$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 ":success$\r$\n"
  FileWrite $0 "echo.$\r$\n"
  FileWrite $0 "echo MSVC environment is now active for this terminal session.$\r$\n"
  FileWrite $0 "echo You can now use 'neutron build' and 'box install' commands.$\r$\n"
  FileWrite $0 "echo.$\r$\n"
  FileClose $0

  ;Store installation folder
  WriteRegStr HKCU "Software\Neutron" "" $INSTDIR
  
  ;Register Uninstaller
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Neutron" "DisplayName" "Neutron ${VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Neutron" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Neutron" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Neutron" "Publisher" "Neutron Team"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Neutron" "DisplayIcon" "$INSTDIR\neutron.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Neutron" "DisplayVersion" "${VERSION}"
  
  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

SectionEnd

Section "Add $INSTDIR to PATH (optional)" SecAddPath
  ; Optional user-controlled PATH addition
  Push "$INSTDIR"
  Call AddToPath
SectionEnd



Section /o "C++ Build Tools (Required for native modules)" SecBuildTools
  
  DetailPrint "Checking for existing MSVC installation..."
  
  ; Check if MSVC is already installed
  IfFileExists "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" msvc_found
  IfFileExists "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" msvc_found
  IfFileExists "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" msvc_found
  IfFileExists "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" msvc_found
  
  DetailPrint "MSVC not found. Downloading Microsoft C++ Build Tools installer..."
  
  ; Download Build Tools installer using built-in NSISdl plugin
  NSISdl::download "https://aka.ms/vs/17/release/vs_BuildTools.exe" "$TEMP\vs_BuildTools.exe"
  Pop $0
  
  ${If} $0 == "success"
    DetailPrint "Download complete. Launching installer..."
    
    ; Launch installer with C++ workload pre-selected (silent install)
    DetailPrint "Installing C++ Build Tools (this may take several minutes)..."
    DetailPrint "Note: This is a large download (~1-2GB). Please be patient..."
    ExecWait '"$TEMP\vs_BuildTools.exe" --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --quiet --wait' $1
    
    ${If} $1 == "0"
      DetailPrint "Build Tools installed successfully."
    ${Else}
      DetailPrint "Build Tools installation returned code: $1"
      DetailPrint "You may need to install manually from: https://aka.ms/vs/17/release/vs_BuildTools.exe"
    ${EndIf}
    
    ; Clean up
    Delete "$TEMP\vs_BuildTools.exe"
    Goto done
  ${Else}
    DetailPrint "Failed to download Build Tools installer: $0"
    DetailPrint "Please download manually from: https://aka.ms/vs/17/release/vs_BuildTools.exe"
    Goto done
  ${EndIf}
  
  msvc_found:
    DetailPrint "MSVC installation found. Skipping download."
    

  
  done:

SectionEnd

;--------------------------------
;Component Descriptions

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecNeutron} "Neutron programming language runtime, compiler, Box package manager, headers, and libraries. (Required)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecAddPath} "Add Neutron to system PATH so you can run 'neutron' and 'box' from any directory. (Recommended)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecBuildTools} "Microsoft C++ Build Tools - Required for 'neutron build' and native modules. If you already have Visual Studio installed, you can skip this. (~1-2GB download, takes 10-15 minutes)"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
;Uninstaller Section

Section "Uninstall"

  ; Optionally remove $INSTDIR from PATH
  Push "$INSTDIR"
  Call un.RemoveFromPath

  Delete "$INSTDIR\Uninstall.exe"
  Delete "$INSTDIR\neutron.exe"
  Delete "$INSTDIR\box.exe"
  Delete "$INSTDIR\neutron-lsp.exe"
  Delete "$INSTDIR\setup-msvc.bat"
  Delete "$INSTDIR\*.dll"
  Delete "$INSTDIR\*.lib"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\README.md"
  
  ; Remove libraries
  Delete "$INSTDIR\lib\*.lib"
  Delete "$INSTDIR\lib\*.a"
  RMDir "$INSTDIR\lib"
  
  ; Remove headers
  RMDir /r "$INSTDIR\include"

  RMDir "$INSTDIR"

  DeleteRegKey /ifempty HKCU "Software\Neutron"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Neutron"

SectionEnd

;--------------------------------
;Path Manipulation Functions

!define ENV_HKLM 'HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"'
!define ENV_HKCU 'HKCU "Environment"'

Function AddToPath
  Exch $0
  Push $1
  Push $2
  Push $3

  ; Try HKLM first (System Path)
  ReadRegStr $1 ${ENV_HKLM} "Path"
  ${If} $1 == ""
    ReadRegStr $1 ${ENV_HKCU} "Path"
  ${EndIf}

  ; Check if already in path
  Push "$1;"
  Push "$0;"
  Call StrStr
  Pop $2
  ${If} $2 != ""
    Goto Done
  ${EndIf}

  ; Append to path
  StrCpy $2 "$1;$0"
  WriteRegExpandStr ${ENV_HKLM} "Path" $2
  SendMessage ${HWND_BROADCAST} ${WM_SETTINGCHANGE} 0 "STR:Environment" /TIMEOUT=5000

  Done:
    Pop $3
    Pop $2
    Pop $1
    Pop $0
FunctionEnd

Function un.RemoveFromPath
  Exch $0
  Push $1
  Push $2
  Push $3
  Push $4
  Push $5

  ReadRegStr $1 ${ENV_HKLM} "Path"
  StrCpy $5 $1 1 -1 # copy last char
  ${If} $5 != ";"
    StrCpy $1 "$1;" # ensure trailing semicolon
  ${EndIf}

  Push $1
  Push "$0;"
  Call un.StrStr
  Pop $2 ; $2 is the substring starting with $0;
  
  ${If} $2 == ""
    Goto Done
  ${EndIf}

  StrLen $3 "$0;"
  StrLen $4 $2
  StrCpy $5 $1 -$4 # $5 is the part before $0;
  StrCpy $2 $2 "" $3 # $2 is the part after $0;
  StrCpy $1 "$5$2"

  ; Remove trailing semicolon if it exists and we are not empty
  StrLen $3 $1
  ${If} $3 > 0
    StrCpy $5 $1 1 -1
    ${If} $5 == ";"
      StrCpy $1 $1 -1
    ${EndIf}
  ${EndIf}

  WriteRegExpandStr ${ENV_HKLM} "Path" $1
  SendMessage ${HWND_BROADCAST} ${WM_SETTINGCHANGE} 0 "STR:Environment" /TIMEOUT=5000

  Done:
    Pop $5
    Pop $4
    Pop $3
    Pop $2
    Pop $1
    Pop $0
FunctionEnd

; String Search Function
Function StrStr
  Exch $R1 ; st=haystack,old$R1, $R1=needle
  Exch    ; st=old$R1,haystack
  Exch $R2 ; st=old$R1,old$R2, $R2=haystack
  Push $R3
  Push $R4
  Push $R5
  StrLen $R3 $R1
  StrCpy $R4 0
  ; $R1=needle
  ; $R2=haystack
  ; $R3=len(needle)
  ; $R4=cnt
  ; $R5=tmp
  loop:
    StrCpy $R5 $R2 $R3 $R4
    StrCmp $R5 $R1 done
    StrCmp $R5 "" done
    IntOp $R4 $R4 + 1
    Goto loop
  done:
    StrCpy $R1 $R2 "" $R4
    Pop $R5
    Pop $R4
    Pop $R3
    Pop $R2
    Exch $R1
FunctionEnd

Function un.StrStr
  Exch $R1 ; st=haystack,old$R1, $R1=needle
  Exch    ; st=old$R1,haystack
  Exch $R2 ; st=old$R1,old$R2, $R2=haystack
  Push $R3
  Push $R4
  Push $R5
  StrLen $R3 $R1
  StrCpy $R4 0
  ; $R1=needle
  ; $R2=haystack
  ; $R3=len(needle)
  ; $R4=cnt
  ; $R5=tmp
  loop:
    StrCpy $R5 $R2 $R3 $R4
    StrCmp $R5 $R1 done
    StrCmp $R5 "" done
    IntOp $R4 $R4 + 1
    Goto loop
  done:
    StrCpy $R1 $R2 "" $R4
    Pop $R5
    Pop $R4
    Pop $R3
    Pop $R2
    Exch $R1
FunctionEnd
