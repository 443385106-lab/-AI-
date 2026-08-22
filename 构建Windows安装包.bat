@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo 正在构建AI制度展板生成工具安装包...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-windows.ps1"
if errorlevel 1 (
  echo 构建失败，请检查Node.js和网络。
) else (
  echo 构建完成，请打开dist文件夹。
  start "" "%~dp0dist"
)
pause
