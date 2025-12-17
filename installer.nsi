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

  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE "LICENSE"
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
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

  SetOutPath "$INSTDIR"
  
  ; Copy main binaries
  File "neutron.exe"
  File "box.exe"
  
  ; Copy runtime library
  File /nonfatal "neutron_runtime.lib"
  File /nonfatal "libneutron_runtime.a"
  
  ; Copy any DLLs
  File /nonfatal "*.dll"
  
  ; Copy documentation
  File /nonfatal "LICENSE"
  File /nonfatal "README.md"
  
  ; Copy directories
  File /nonfatal /r "docs"
  File /nonfatal /r "include"
  File /nonfatal /r "libs"
  File /nonfatal /r "src"
  File /nonfatal /r "lib"

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
  
  ; Add to PATH
  Push "$INSTDIR"
  Call AddToPath

SectionEnd

Section /o "C++ Build Tools (Required for native modules)" SecBuildTools
  
  DetailPrint "Downloading Microsoft C++ Build Tools installer..."
  
  ; Download Build Tools installer
  NSISdl::download "https://aka.ms/vs/17/release/vs_BuildTools.exe" "$TEMP\vs_BuildTools.exe"
  Pop $0
  
  ${If} $0 == "success"
    DetailPrint "Download complete. Launching installer..."
    
    ; Launch installer with C++ workload pre-selected (silent install)
    DetailPrint "Installing C++ Build Tools (this may take several minutes)..."
    ExecWait '"$TEMP\vs_BuildTools.exe" --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --quiet --wait'
    
    ; Clean up
    Delete "$TEMP\vs_BuildTools.exe"
    
    ; Create a helper batch script to setup MSVC environment
    DetailPrint "Creating MSVC environment helper script..."
    FileOpen $0 "$INSTDIR\setup-msvc-env.bat" w
    FileWrite $0 "@echo off$\r$\n"
    FileWrite $0 "REM Neutron MSVC Environment Setup$\r$\n"
    FileWrite $0 "REM Run this script to add MSVC to your PATH for the current session$\r$\n"
    FileWrite $0 "$\r$\n"
    FileWrite $0 "echo Setting up MSVC environment...$\r$\n"
    FileWrite $0 "$\r$\n"
    FileWrite $0 "if exist $\"C:\Program Files\Microsoft Visual Studio\2026\BuildTools\VC\Auxiliary\Build\vcvars64.bat$\" ($\r$\n"
    FileWrite $0 "  call $\"C:\Program Files\Microsoft Visual Studio\2026\BuildTools\VC\Auxiliary\Build\vcvars64.bat$\"$\r$\n"
    FileWrite $0 ") else if exist $\"C:\Program Files\Microsoft Visual Studio\2026\Community\VC\Auxiliary\Build\vcvars64.bat$\" ($\r$\n"
    FileWrite $0 "  call $\"C:\Program Files\Microsoft Visual Studio\2026\Community\VC\Auxiliary\Build\vcvars64.bat$\"$\r$\n"
    FileWrite $0 ") else if exist $\"C:\Program Files\Microsoft Visual Studio\2025\BuildTools\VC\Auxiliary\Build\vcvars64.bat$\" ($\r$\n"
    FileWrite $0 "  call $\"C:\Program Files\Microsoft Visual Studio\2025\BuildTools\VC\Auxiliary\Build\vcvars64.bat$\"$\r$\n"
    FileWrite $0 ") else if exist $\"C:\Program Files\Microsoft Visual Studio\2025\Community\VC\Auxiliary\Build\vcvars64.bat$\" ($\r$\n"
    FileWrite $0 "  call $\"C:\Program Files\Microsoft Visual Studio\2025\Community\VC\Auxiliary\Build\vcvars64.bat$\"$\r$\n"
    FileWrite $0 ") else if exist $\"C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat$\" ($\r$\n"
    FileWrite $0 "  call $\"C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat$\"$\r$\n"
    FileWrite $0 ") else if exist $\"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat$\" ($\r$\n"
    FileWrite $0 "  call $\"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat$\"$\r$\n"
    FileWrite $0 ") else if exist $\"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat$\" ($\r$\n"
    FileWrite $0 "  call $\"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat$\"$\r$\n"
    FileWrite $0 ") else ($\r$\n"
    FileWrite $0 "  echo Error: Could not find MSVC installation$\r$\n"
    FileWrite $0 "  exit /b 1$\r$\n"
    FileWrite $0 ")$\r$\n"
    FileWrite $0 "$\r$\n"
    FileWrite $0 "echo.$\r$\n"
    FileWrite $0 "echo MSVC environment is now active for this terminal session.$\r$\n"
    FileWrite $0 "echo You can now use 'cl', 'link', and other MSVC tools.$\r$\n"
    FileWrite $0 "echo.$\r$\n"
    FileWrite $0 "echo Note: box install will work automatically without running this script.$\r$\n"
    FileClose $0
    
    DetailPrint "Build Tools installation complete."
    Goto done
      
  ${Else}
    DetailPrint "Failed to download Build Tools installer."
  ${EndIf}
  
  done:

SectionEnd

;--------------------------------
;Component Descriptions

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecNeutron} "Neutron programming language runtime, compiler, and Box package manager. (Required)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecBuildTools} "Microsoft C++ Build Tools for compiling native modules. Required to use 'box install' for native extensions. (~1.2GB download)"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
;Uninstaller Section

Section "Uninstall"

  ; Remove from PATH
  Push "$INSTDIR"
  Call un.RemoveFromPath

  Delete "$INSTDIR\Uninstall.exe"
  Delete "$INSTDIR\neutron.exe"
  Delete "$INSTDIR\box.exe"
  Delete "$INSTDIR\setup-msvc-env.bat"
  Delete "$INSTDIR\*.dll"
  Delete "$INSTDIR\*.a"
  Delete "$INSTDIR\*.lib"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\README.md"
  
  RMDir /r "$INSTDIR\docs"
  RMDir /r "$INSTDIR\include"
  RMDir /r "$INSTDIR\libs"
  RMDir /r "$INSTDIR\src"
  RMDir /r "$INSTDIR\lib"

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
