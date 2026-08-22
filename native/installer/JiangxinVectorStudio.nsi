Unicode true
!include "MUI2.nsh"

!ifndef OUTFILE
  !define OUTFILE "..\dist\JiangxinVectorStudio-1.9.0-Setup.exe"
!endif

Name "匠心矢量设计"
Caption "匠心矢量设计 1.9.0 安装程序"
UninstallCaption "卸载匠心矢量设计"
BrandingText "匠心图文"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\JiangxinVectorStudio"
InstallDirRegKey HKLM "Software\JiangxinVectorStudio" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
ShowInstDetails show
ShowUninstDetails show
Icon "..\..\assets\app-icon.ico"
UninstallIcon "..\..\assets\app-icon.ico"

VIProductVersion "1.9.0.0"
VIAddVersionKey /LANG=2052 "ProductName" "匠心矢量设计"
VIAddVersionKey /LANG=2052 "CompanyName" "匠心图文"
VIAddVersionKey /LANG=2052 "FileDescription" "国产原生矢量设计软件"
VIAddVersionKey /LANG=2052 "FileVersion" "1.9.0"

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

Function CloseRunningStudio
  nsExec::Exec /TIMEOUT=8000 '"$SYSDIR\taskkill.exe" /F /T /IM JiangxinVectorStudio.exe'
  Pop $0
  Sleep 700
FunctionEnd

Function .onInit
  SetRegView 64
  Call CloseRunningStudio
FunctionEnd

Function un.CloseRunningStudio
  nsExec::Exec /TIMEOUT=8000 '"$SYSDIR\taskkill.exe" /F /T /IM JiangxinVectorStudio.exe'
  Pop $0
  Sleep 500
FunctionEnd

Function un.onInit
  SetRegView 64
  Call un.CloseRunningStudio
FunctionEnd

Section "主程序" SecMain
retry_close:
  Call CloseRunningStudio
  ClearErrors
  IfFileExists "$INSTDIR\bin\JiangxinVectorStudio.exe" 0 copy_files
  Delete "$INSTDIR\bin\JiangxinVectorStudio.exe"
  IfErrors 0 copy_files
  MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "主程序正在运行或被安全软件占用，暂时无法更新。$\r$\n$\r$\n请关闭匠心矢量设计，并在任务管理器中结束 JiangxinVectorStudio.exe；必要时允许安全软件放行，然后点击‘重试’。" IDRETRY retry_close IDCANCEL install_abort
install_abort:
  Abort "安装已取消，原程序文件未被覆盖。"
copy_files:
  SetOverwrite on
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
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\JiangxinVectorStudio" "DisplayVersion" "1.9.0"
SectionEnd

Section "Uninstall"
  Delete "$DESKTOP\匠心矢量设计.lnk"
  RMDir /r "$SMPROGRAMS\匠心矢量设计"
  RMDir /r "$INSTDIR"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\JiangxinVectorStudio"
  DeleteRegKey HKLM "Software\JiangxinVectorStudio"
SectionEnd
