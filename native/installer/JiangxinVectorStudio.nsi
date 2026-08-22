Unicode true
!include "MUI2.nsh"

!ifndef OUTFILE
  !define OUTFILE "..\dist\JiangxinVectorStudio-1.8.0-Setup.exe"
!endif

Name "匠心矢量设计"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\JiangxinVectorStudio"
InstallDirRegKey HKLM "Software\JiangxinVectorStudio" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
Icon "..\..\assets\app-icon.ico"
UninstallIcon "..\..\assets\app-icon.ico"

VIProductVersion "1.8.0.0"
VIAddVersionKey /LANG=2052 "ProductName" "匠心矢量设计"
VIAddVersionKey /LANG=2052 "CompanyName" "匠心图文"
VIAddVersionKey /LANG=2052 "FileDescription" "国产原生矢量设计软件"
VIAddVersionKey /LANG=2052 "FileVersion" "1.8.0"

!define MUI_ABORTWARNING
!define MUI_ICON "..\..\assets\app-icon.ico"
!define MUI_UNICON "..\..\assets\app-icon.ico"
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\JiangxinVectorStudio.exe"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SimpChinese"

Section "主程序" SecMain
  SetOutPath "$INSTDIR"
  File /r "..\package\*.*"
  WriteRegStr HKLM "Software\JiangxinVectorStudio" "InstallDir" "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateDirectory "$SMPROGRAMS\匠心矢量设计"
  CreateShortcut "$SMPROGRAMS\匠心矢量设计\匠心矢量设计.lnk" "$INSTDIR\bin\JiangxinVectorStudio.exe" "" "$INSTDIR\bin\JiangxinVectorStudio.exe"
  CreateShortcut "$SMPROGRAMS\匠心矢量设计\卸载.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortcut "$DESKTOP\匠心矢量设计.lnk" "$INSTDIR\bin\JiangxinVectorStudio.exe" "" "$INSTDIR\bin\JiangxinVectorStudio.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\JiangxinVectorStudio" "DisplayName" "匠心矢量设计"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\JiangxinVectorStudio" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\JiangxinVectorStudio" "Publisher" "匠心图文"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\JiangxinVectorStudio" "DisplayVersion" "1.8.0"
SectionEnd

Section "Uninstall"
  Delete "$DESKTOP\匠心矢量设计.lnk"
  RMDir /r "$SMPROGRAMS\匠心矢量设计"
  RMDir /r "$INSTDIR"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\JiangxinVectorStudio"
  DeleteRegKey HKLM "Software\JiangxinVectorStudio"
SectionEnd
